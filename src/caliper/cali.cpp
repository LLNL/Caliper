// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// Caliper C interface implementation

#include "caliper/caliper-config.h"

#include "caliper/cali.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include "../common/CompressedSnapshotRecord.h"
#include "../common/RuntimeConfig.h"

#include "caliper/common/Log.h"
#include "caliper/common/Node.h"
#include "caliper/common/OutputStream.h"
#include "caliper/common/Variant.h"

#include "caliper/reader/CalQLParser.h"
#include "caliper/reader/QueryProcessor.h"

#include <cstring>
#include <mutex>

#define SNAP_MAX 120

namespace cali
{

extern Attribute region_attr;
extern Attribute phase_attr;
extern Attribute comm_region_attr;

} // namespace cali

using namespace cali;

//
// --- Miscellaneous
//

const char* cali_caliper_version()
{
    return CALIPER_VERSION;
}

//
// --- Attribute interface
//

cali_id_t cali_create_attribute(const char* name, cali_attr_type type, int properties)
{
    Attribute a = Caliper::instance().create_attribute(name, type, properties);

    return a.id();
}

cali_id_t cali_create_attribute_with_metadata(
    const char*          name,
    cali_attr_type       type,
    int                  properties,
    int                  n,
    const cali_id_t      meta_attr_list[],
    const cali_variant_t meta_val_list[]
)
{
    if (n < 1)
        return cali_create_attribute(name, type, properties);

    Caliper c;

    Attribute* meta_attr = new Attribute[n];
    Variant*   meta_data = new Variant[n];

    for (int i = 0; i < n; ++i) {
        meta_attr[i] = c.get_attribute(meta_attr_list[i]);

        if (!meta_attr[i])
            continue;

        meta_data[i] = Variant(meta_val_list[i]);
    }

    Attribute attr = c.create_attribute(name, type, properties, n, meta_attr, meta_data);

    delete[] meta_data;
    delete[] meta_attr;

    return attr.id();
}

cali_id_t cali_find_attribute(const char* name)
{
    Attribute a = Caliper::instance().get_attribute(name);

    return a.id();
}

cali_attr_type cali_attribute_type(cali_id_t attr_id)
{
    Attribute a = Caliper::instance().get_attribute(attr_id);
    return a.type();
}

int cali_attribute_properties(cali_id_t attr_id)
{
    Attribute a = Caliper::instance().get_attribute(attr_id);
    return a.properties();
}

const char* cali_attribute_name(cali_id_t attr_id)
{
    return Caliper::instance().get_attribute(attr_id).name_c_str();
}

//
// --- Context interface
//

void cali_push_snapshot(
    int /*scope*/,
    int                  n,
    const cali_id_t      trigger_info_attr_list[],
    const cali_variant_t trigger_info_val_list[]
)
{
    Caliper c;

    Attribute attr[64];
    Variant   data[64];

    n = std::min(std::max(n, 0), 64);

    for (int i = 0; i < n; ++i) {
        attr[i] = c.get_attribute(trigger_info_attr_list[i]);
        data[i] = Variant(trigger_info_val_list[i]);
    }

    FixedSizeSnapshotRecord<64> trigger_info;
    c.make_record(n, attr, data, trigger_info.builder());

    for (auto& channel : c.get_all_channels())
        if (channel.is_active())
            c.push_snapshot(channel.body(), trigger_info.view());
}

void cali_channel_push_snapshot(
    cali_id_t chn_id,
    int /*scope*/,
    int                  n,
    const cali_id_t      trigger_info_attr_list[],
    const cali_variant_t trigger_info_val_list[]
)
{
    Caliper c;

    Attribute attr[64];
    Variant   data[64];

    n = std::min(std::max(n, 0), 64);

    for (int i = 0; i < n; ++i) {
        attr[i] = c.get_attribute(trigger_info_attr_list[i]);
        data[i] = Variant(trigger_info_val_list[i]);
    }

    FixedSizeSnapshotRecord<64> trigger_info;
    c.make_record(n, attr, data, trigger_info.builder());

    Channel channel = c.get_channel(chn_id);

    if (channel && channel.is_active())
        c.push_snapshot(channel.body(), trigger_info.view());
}

size_t cali_channel_pull_snapshot(cali_id_t chn_id, int /* scopes */, size_t len, unsigned char* buf)
{
    Caliper c = Caliper::sigsafe_instance();

    if (!c)
        return 0;

    FixedSizeSnapshotRecord<SNAP_MAX> snapshot;
    Channel                           channel = c.get_channel(chn_id);

    if (channel)
        c.pull_snapshot(channel.body(), SnapshotView(), snapshot.builder());
    else
        Log(0).stream() << "cali_channel_pull_snapshot(): invalid channel id " << chn_id << std::endl;

    CompressedSnapshotRecord rec(len, buf);
    SnapshotView             view(snapshot.view());
    rec.append(view.size(), view.data());

    return rec.needed_len();
}

//
// --- Snapshot parsing
//

namespace
{
// Helper operator to unpack entries from
// CompressedSnapshotRecordView::unpack()

class UnpackEntryOp
{
    void*              m_arg;
    cali_entry_proc_fn m_fn;

public:

    UnpackEntryOp(cali_entry_proc_fn fn, void* user_arg) : m_arg(user_arg), m_fn(fn) {}

    inline bool operator() (const Entry& e)
    {
        if (e.is_immediate()) {
            if ((*m_fn)(m_arg, e.attribute(), e.value().c_variant()) == 0)
                return false;
        } else {
            for (const Node* node = e.node(); node && node->id() != CALI_INV_ID; node = node->parent())
                if ((*m_fn)(m_arg, node->attribute(), node->data().c_variant()) == 0)
                    return false;
        }

        return true;
    }
};

class UnpackAttributeEntryOp
{
    void*              m_arg;
    cali_entry_proc_fn m_fn;
    cali_id_t          m_id;

public:

    UnpackAttributeEntryOp(cali_id_t id, cali_entry_proc_fn fn, void* user_arg) : m_arg(user_arg), m_fn(fn), m_id(id) {}

    inline bool operator() (const Entry& e)
    {
        if (e.is_immediate() && e.attribute() == m_id) {
            if ((*m_fn)(m_arg, e.attribute(), e.value().c_variant()) == 0)
                return false;
        } else {
            for (const Node* node = e.node(); node && node->id() != CALI_INV_ID; node = node->parent())
                if (node->attribute() == m_id)
                    if ((*m_fn)(m_arg, node->attribute(), node->data().c_variant()) == 0)
                        return false;
        }

        return true;
    }
};
} // namespace

void cali_unpack_snapshot(const unsigned char* buf, size_t* bytes_read, cali_entry_proc_fn proc_fn, void* user_arg)
{
    size_t          pos = 0;
    ::UnpackEntryOp op(proc_fn, user_arg);

    // FIXME: Need sigsafe instance? Only does read-only
    // node-by-id lookup though, so we should be safe
    Caliper c;

    CompressedSnapshotRecordView(buf, &pos).unpack(&c, op);

    if (bytes_read)
        *bytes_read += pos;
}

cali_variant_t cali_find_first_in_snapshot(const unsigned char* buf, cali_id_t attr_id, size_t* bytes_read)
{
    size_t  pos = 0;
    Variant res;

    Caliper c;

    CompressedSnapshotRecordView(buf, &pos).unpack(&c, [&res, attr_id](const Entry& e) {
        if (e.is_immediate()) {
            if (e.attribute() == attr_id) {
                res = e.value();
                return false;
            }
        } else {
            for (const Node* node = e.node(); node; node = node->parent())
                if (node->attribute() == attr_id) {
                    res = node->data();
                    return false;
                }
        }

        return true;
    });

    if (bytes_read)
        *bytes_read += pos;

    return res.c_variant();
}

void cali_find_all_in_snapshot(
    const unsigned char* buf,
    cali_id_t            attr_id,
    size_t*              bytes_read,
    cali_entry_proc_fn   proc_fn,
    void*                user_arg
)
{
    size_t                   pos = 0;
    ::UnpackAttributeEntryOp op(attr_id, proc_fn, user_arg);

    // FIXME: Need sigsafe instance? Only does read-only
    // node-by-id lookup though, so we should be safe
    Caliper c;

    CompressedSnapshotRecordView(buf, &pos).unpack(&c, op);

    if (bytes_read)
        *bytes_read += pos;
}

//
// --- Blackboard access interface
//

cali_variant_t cali_get(cali_id_t attr_id)
{
    Caliper c = Caliper::sigsafe_instance();

    if (!c)
        return cali_make_empty_variant();

    return c.get(c.get_attribute(attr_id)).value().c_variant();
}

cali_variant_t cali_channel_get(cali_id_t chn_id, cali_id_t attr_id)
{
    Caliper c       = Caliper::sigsafe_instance();
    Channel channel = c.get_channel(chn_id);

    if (!c || !channel)
        return cali_make_empty_variant();

    return c.get(channel.body(), c.get_attribute(attr_id)).value().c_variant();
}

const char* cali_get_current_region_or(const char* alt)
{
    Caliper c = Caliper::sigsafe_instance();

    if (!c)
        return alt;

    Entry e = c.get_path_node();

    if (!e.empty() && e.value().type() == CALI_TYPE_STRING)
        return static_cast<const char*>(e.value().data());

    return alt;
}

//
// --- Annotation interface
//

void cali_begin_region(const char* name)
{
    Caliper c;
    c.begin(cali::region_attr, Variant(name));
}

void cali_end_region(const char* name)
{
    Caliper c;
    c.end_with_value_check(cali::region_attr, Variant(name));
}

void cali_begin_phase(const char* name)
{
    Caliper c;
    c.begin(cali::phase_attr, Variant(name));
}

void cali_end_phase(const char* name)
{
    Caliper c;
    c.end_with_value_check(cali::phase_attr, Variant(name));
}

void cali_begin_comm_region(const char* name)
{
    Caliper c;
    c.begin(cali::comm_region_attr, Variant(name));
}

void cali_end_comm_region(const char* name)
{
    Caliper c;
    c.end_with_value_check(cali::comm_region_attr, Variant(name));
}

void cali_begin(cali_id_t attr_id)
{
    Caliper   c;
    Attribute attr = c.get_attribute(attr_id);

    c.begin(attr, Variant(true));
}

void cali_end(cali_id_t attr_id)
{
    Caliper   c;
    Attribute attr = c.get_attribute(attr_id);

    c.end(attr);
}

void cali_set(cali_id_t attr_id, const void* value, size_t size)
{
    Caliper   c;
    Attribute attr = c.get_attribute(attr_id);

    c.set(attr, Variant(attr.type(), value, size));
}

void cali_begin_double(cali_id_t attr_id, double val)
{
    Caliper   c;
    Attribute attr = c.get_attribute(attr_id);

    c.begin(attr, Variant(val));
}

void cali_begin_int(cali_id_t attr_id, int val)
{
    Caliper   c;
    Attribute attr = c.get_attribute(attr_id);

    c.begin(attr, Variant(val));
}

void cali_begin_string(cali_id_t attr_id, const char* val)
{
    Caliper   c;
    Attribute attr = c.get_attribute(attr_id);

    c.begin(attr, Variant(CALI_TYPE_STRING, val, strlen(val)));
}

void cali_set_double(cali_id_t attr_id, double val)
{
    Caliper   c;
    Attribute attr = c.get_attribute(attr_id);

    c.set(attr, Variant(val));
}

void cali_set_int(cali_id_t attr_id, int val)
{
    Caliper   c;
    Attribute attr = c.get_attribute(attr_id);

    c.set(attr, Variant(val));
}

void cali_set_string(cali_id_t attr_id, const char* val)
{
    Caliper   c;
    Attribute attr = c.get_attribute(attr_id);

    c.set(attr, Variant(CALI_TYPE_STRING, val, strlen(val)));
}

//
// --- By-name annotation interface
//

void cali_begin_byname(const char* attr_name)
{
    Caliper   c;
    Attribute attr = c.create_attribute(attr_name, CALI_TYPE_BOOL, CALI_ATTR_DEFAULT);

    c.begin(attr, Variant(true));
}

void cali_begin_double_byname(const char* attr_name, double val)
{
    Caliper   c;
    Attribute attr = c.create_attribute(attr_name, CALI_TYPE_DOUBLE, CALI_ATTR_DEFAULT);

    c.begin(attr, Variant(val));
}

void cali_begin_int_byname(const char* attr_name, int val)
{
    Caliper   c;
    Attribute attr = c.create_attribute(attr_name, CALI_TYPE_INT, CALI_ATTR_DEFAULT);

    c.begin(attr, Variant(val));
}

void cali_begin_string_byname(const char* attr_name, const char* val)
{
    Caliper   c;
    Attribute attr = c.create_attribute(attr_name, CALI_TYPE_STRING, CALI_ATTR_DEFAULT);

    c.begin(attr, Variant(CALI_TYPE_STRING, val, strlen(val)));
}

void cali_set_double_byname(const char* attr_name, double val)
{
    Caliper   c;
    Attribute attr = c.create_attribute(attr_name, CALI_TYPE_DOUBLE, CALI_ATTR_UNALIGNED);

    c.set(attr, Variant(val));
}

void cali_set_int_byname(const char* attr_name, int val)
{
    Caliper   c;
    Attribute attr = c.create_attribute(attr_name, CALI_TYPE_INT, CALI_ATTR_UNALIGNED);

    c.set(attr, Variant(val));
}

void cali_set_string_byname(const char* attr_name, const char* val)
{
    Caliper   c;
    Attribute attr = c.create_attribute(attr_name, CALI_TYPE_STRING, CALI_ATTR_UNALIGNED);

    c.set(attr, Variant(CALI_TYPE_STRING, val, strlen(val)));
}

void cali_end_byname(const char* attr_name)
{
    Caliper   c;
    Attribute attr = c.get_attribute(attr_name);

    c.end(attr);
}

// --- Set globals
//

void cali_set_global_double_byname(const char* name, double val)
{
    Caliper   c;
    Attribute attr =
        c.create_attribute(name, CALI_TYPE_DOUBLE, CALI_ATTR_GLOBAL | CALI_ATTR_UNALIGNED | CALI_ATTR_SKIP_EVENTS);

    // TODO: check for existing incompatible attribute key

    c.set(attr, cali_make_variant_from_double(val));
}

void cali_set_global_int_byname(const char* name, int val)
{
    Caliper   c;
    Attribute attr =
        c.create_attribute(name, CALI_TYPE_INT, CALI_ATTR_GLOBAL | CALI_ATTR_UNALIGNED | CALI_ATTR_SKIP_EVENTS);

    // TODO: check for existing incompatible attribute key

    c.set(attr, cali_make_variant_from_int(val));
}

void cali_set_global_string_byname(const char* name, const char* val)
{
    Caliper   c;
    Attribute attr =
        c.create_attribute(name, CALI_TYPE_STRING, CALI_ATTR_GLOBAL | CALI_ATTR_UNALIGNED | CALI_ATTR_SKIP_EVENTS);

    // TODO: check for existing incompatible attribute key

    c.set(attr, Variant(CALI_TYPE_STRING, val, strlen(val) + 1));
}

void cali_set_global_uint_byname(const char* name, uint64_t val)
{
    Caliper   c;
    Attribute attr =
        c.create_attribute(name, CALI_TYPE_UINT, CALI_ATTR_GLOBAL | CALI_ATTR_UNALIGNED | CALI_ATTR_SKIP_EVENTS);

    // TODO: check for existing incompatible attribute key

    c.set(attr, cali_make_variant_from_uint(val));
}

// --- Config API
//

void cali_config_preset(const char* key, const char* value)
{
    if (Caliper::is_initialized())
        Log(0).stream() << "Warning: Caliper is already initialized. "
                        << "cali_config_preset(\"" << key << "\", \"" << value << "\") has no effect." << std::endl;

    RuntimeConfig::get_default_config().preset(key, value);
}

void cali_config_set(const char* key, const char* value)
{
    if (Caliper::is_initialized())
        Log(0).stream() << "Warning: Caliper is already initialized. "
                        << "cali_config_set(\"" << key << "\", \"" << value << "\") has no effect." << std::endl;

    RuntimeConfig::get_default_config().set(key, value);
}

void cali_config_allow_read_env(int allow)
{
    RuntimeConfig::get_default_config().allow_read_env(allow != 0);
}

struct _cali_configset_t {
    std::map<std::string, std::string> cfgset;
};

cali_configset_t cali_create_configset(const char* keyvallist[][2])
{
    cali_configset_t cfg = new _cali_configset_t;

    if (!keyvallist)
        return cfg;

    for (; (*keyvallist)[0] && (*keyvallist)[1]; ++keyvallist)
        cfg->cfgset.insert(std::make_pair((*keyvallist)[0], (*keyvallist)[1]));

    return cfg;
}

void cali_delete_configset(cali_configset_t cfg)
{
    delete cfg;
}

void cali_configset_set(cali_configset_t cfg, const char* key, const char* value)
{
    cfg->cfgset[key] = value;
}

cali_id_t cali_create_channel(const char* name, int flags, cali_configset_t cfgset)
{
    RuntimeConfig cfg;

    cfg.allow_read_env(flags & CALI_CHANNEL_ALLOW_READ_ENV);
    cfg.import(cfgset->cfgset);

    Caliper c;
    Channel channel = c.create_channel(name, cfg);

    if (!channel)
        return CALI_INV_ID;
    if (!(flags & CALI_CHANNEL_LEAVE_INACTIVE))
        c.activate_channel(channel);

    return channel.id();
}

void cali_delete_channel(cali_id_t chn_id)
{
    Caliper c;
    Channel channel = c.get_channel(chn_id);

    if (channel)
        c.delete_channel(channel);
    else
        Log(0).stream() << "cali_channel_delete(): invalid channel id " << chn_id << std::endl;
}

void cali_activate_channel(cali_id_t chn_id)
{
    Caliper c;
    Channel channel = c.get_channel(chn_id);

    if (channel)
        c.activate_channel(channel);
    else
        Log(0).stream() << "cali_activate_channel(): invalid channel id " << chn_id << std::endl;
}

void cali_deactivate_channel(cali_id_t chn_id)
{
    Caliper c;
    Channel channel = c.get_channel(chn_id);

    if (channel)
        c.deactivate_channel(channel);
    else
        Log(0).stream() << "cali_deactivate_channel(): invalid channel id " << chn_id << std::endl;
}

int cali_channel_is_active(cali_id_t chn_id)
{
    Channel channel = Caliper::instance().get_channel(chn_id);

    if (!channel) {
        Log(0).stream() << "cali_channel_is_active(): invalid channel id " << chn_id << std::endl;
        return 0;
    }

    return (channel.is_active() ? 1 : 0);
}

void cali_flush(int flush_opts)
{
    Caliper c;
    Channel channel = c.get_channel(0); // channel 0 should be the default channel

    if (channel) {
        c.flush_and_write(channel.body(), SnapshotView());

        if (flush_opts & CALI_FLUSH_CLEAR_BUFFERS)
            c.clear(&channel);
    }
}

void cali_channel_flush(cali_id_t chn_id, int flush_opts)
{
    Caliper c;
    Channel channel = c.get_channel(chn_id);

    c.flush_and_write(channel.body(), SnapshotView());

    if (flush_opts & CALI_FLUSH_CLEAR_BUFFERS)
        c.clear(&channel);
}

void cali_init()
{
    Caliper::instance();
}

int cali_is_initialized()
{
    return Caliper::is_initialized() ? 1 : 0;
}

//
// --- Helper functions for high-level macro interface
//

namespace cali
{

extern Attribute class_iteration_attr;

}

cali_id_t cali_make_loop_iteration_attribute(const char* name)
{
    Variant v_true(true);

    Caliper   c;
    Attribute attr = c.create_attribute(
        std::string("iteration#").append(name),
        CALI_TYPE_INT,
        CALI_ATTR_ASVALUE,
        1,
        &class_iteration_attr,
        &v_true
    );

    return attr.id();
}

//
// --- C++ convenience API
//

namespace cali
{

cali_id_t create_channel(const char* name, int flags, const config_map_t& cfgmap)
{
    RuntimeConfig cfg;

    cfg.allow_read_env(flags & CALI_CHANNEL_ALLOW_READ_ENV);
    cfg.import(cfgmap);

    Caliper c;
    Channel channel = c.create_channel(name, cfg);

    if (!channel)
        return CALI_INV_ID;
    if (!(flags & CALI_CHANNEL_LEAVE_INACTIVE))
        c.activate_channel(channel);

    return channel.id();
}

void write_report_for_query(cali_id_t chn_id, const char* query, int flush_opts, std::ostream& os)
{
    Caliper c;
    Channel channel = c.get_channel(chn_id);

    if (!channel) {
        Log(0).stream() << "write_report_for_query(): invalid channel id " << chn_id << std::endl;

        return;
    }

    CalQLParser parser(query);

    if (parser.error()) {
        Log(0).stream() << "write_report_for_query(): query parse error: " << parser.error_msg() << std::endl;

        return;
    }

    QuerySpec    spec(parser.spec());
    OutputStream stream;

    stream.set_stream(&os);

    QueryProcessor queryP(spec, stream);

    c.flush(channel.body(), SnapshotView(), [&queryP](CaliperMetadataAccessInterface& db, const std::vector<Entry>& rec) {
        queryP.process_record(db, rec);
    });

    queryP.flush(c);
}

} // namespace cali
