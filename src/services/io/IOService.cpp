// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include <curious/curious.h>

#include "caliper/Caliper.h"
#include "caliper/CaliperService.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/common/Log.h"

#include <string.h>
#include <cstddef>
#include <memory>
#include <unordered_map>

using namespace cali;

namespace
{
struct user_data_t {
    curious_t curious_ctx;
    Channel   channel;

    user_data_t(curious_t ctx, Channel& chn) : curious_ctx(ctx), channel(chn) {}
};

std::unordered_map<cali_id_t, std::unique_ptr<user_data_t>> curious_insts;

int num_io_callbacks;
int num_failed_io_callbacks;

Attribute io_region_attr;
Attribute io_filesystem_attr;
Attribute io_mount_point_attr;
Attribute io_bytes_read_attr;
Attribute io_bytes_written_attr;

// The least significant bit of he callback type is for whether
// it comes before or after an I/O call, so we can discard it
// to index the categories of I/O calls
#define GET_CATEGORY(callback_type) ((callback_type) >> 1)

// A lookup table for the Variant corresponding to each callback category
Variant categories[] = {
    Variant(CALI_TYPE_STRING, "read", 5),
    Variant(CALI_TYPE_STRING, "write", 6),
    Variant(CALI_TYPE_STRING, "metadata", 9),
};

// We need to record different info for different I/O regions/records:

// For read regions...
void handle_unique_record_data(Caliper& c, Channel& channel, curious_read_record_t* record)
{
    if (record->bytes_read > 0) {
        // ...we care about how much data was read
        Entry data(io_bytes_read_attr, cali_make_variant_from_uint(record->bytes_read));
        c.push_snapshot(&channel, SnapshotView(1, &data));
    }
}

// For write regions...
void handle_unique_record_data(Caliper& c, Channel& channel, curious_write_record_t* record)
{
    if (record->bytes_written > 0) {
        // ...we care about how much data was written
        Entry data(io_bytes_written_attr, cali_make_variant_from_uint(record->bytes_written));
        c.push_snapshot(&channel, SnapshotView(1, &data));
    }
}

// We don't need anything extra from metadata regions
void handle_unique_record_data(Caliper& c, Channel& channel, curious_metadata_record_t* record)
{}

// Just preforms some sanity checking before making a Variant from a C string
Variant make_variant(char* str)
{
    Variant var;

    if (nullptr == str) {
        var = Variant(CALI_TYPE_STRING, "unknown", 8);
    } else {
        var = Variant(CALI_TYPE_STRING, str, strlen(str) + 1);
    }

    return var;
}

template <typename Record>
void handle_io(curious_callback_type_t type, void* usr_data, Record* record)
{
    ++num_io_callbacks;

    // get a Caliper instance

    Caliper c    = Caliper::sigsafe_instance();
    auto    data = static_cast<user_data_t*>(usr_data);

    // If we got a Caliper instance successfully...
    if (c && data->channel.is_active()) {
        // ...mark the end...
        if (type & CURIOUS_POST_CALLBACK) {
            handle_unique_record_data(c, data->channel, record);

            c.end(io_region_attr);
            c.end(io_filesystem_attr);
            c.end(io_mount_point_attr);

            // ...or begining of the I/O region, as appropriate
        } else {
            Variant filesystem_var  = make_variant(record->filesystem);
            Variant mount_point_var = make_variant(record->mount_point);

            c.begin(io_mount_point_attr, mount_point_var);
            c.begin(io_filesystem_attr, filesystem_var);
            c.begin(io_region_attr, categories[GET_CATEGORY(type)]);
        }

        // ...otherwise not our failure (probably already inside Caliper)
    } else {
        ++num_failed_io_callbacks;
    }
}

void init_curious_in_channel(Caliper* c, Channel* channel)
{
    channel->events().subscribe_attribute(c, channel, io_region_attr);

    curious_t curious_inst = curious_init(CURIOUS_ALL_APIS);
    auto      data         = std::unique_ptr<user_data_t>(new user_data_t { curious_inst, *channel });

    curious_register_callback(
        curious_inst,
        CURIOUS_READ_CALLBACK,
        (curious_callback_f) handle_io<curious_read_record_t>,
        data.get()
    );
    curious_register_callback(
        curious_inst,
        CURIOUS_READ_CALLBACK | CURIOUS_POST_CALLBACK,
        (curious_callback_f) handle_io<curious_read_record_t>,
        data.get()
    );
    curious_register_callback(
        curious_inst,
        CURIOUS_WRITE_CALLBACK,
        (curious_callback_f) handle_io<curious_write_record_t>,
        data.get()
    );
    curious_register_callback(
        curious_inst,
        CURIOUS_WRITE_CALLBACK | CURIOUS_POST_CALLBACK,
        (curious_callback_f) handle_io<curious_write_record_t>,
        data.get()
    );
    curious_register_callback(
        curious_inst,
        CURIOUS_METADATA_CALLBACK,
        (curious_callback_f) handle_io<curious_metadata_record_t>,
        data.get()
    );
    curious_register_callback(
        curious_inst,
        CURIOUS_METADATA_CALLBACK | CURIOUS_POST_CALLBACK,
        (curious_callback_f) handle_io<curious_metadata_record_t>,
        data.get()
    );

    curious_insts.insert(std::make_pair(channel->id(), std::move(data)));
}

void finalize_curious_in_channel(Caliper* c, Channel* channel)
{
    auto it = curious_insts.find(channel->id());
    if (it != curious_insts.end()) {
        curious_finalize(it->second->curious_ctx);
        curious_insts.erase(it);
    }

    if (Log::verbosity() >= 2)
        Log(2).stream() << channel->name() << ": Processed " << num_io_callbacks << " I/O callbacks, "
                        << num_failed_io_callbacks << " failed." << std::endl;
}

void init_curious_service(Caliper* c, Channel* channel)
{
    Attribute subscribe_attr = c->get_attribute("subscription_event");
    Variant   v_true(true);

    // create I/O attributes
    io_region_attr =
        c->create_attribute("io.region", CALI_TYPE_STRING, CALI_ATTR_SCOPE_THREAD, 1, &subscribe_attr, &v_true);
    io_filesystem_attr =
        c->create_attribute("io.filesystem", CALI_TYPE_STRING, CALI_ATTR_SCOPE_THREAD | CALI_ATTR_SKIP_EVENTS);
    io_mount_point_attr =
        c->create_attribute("io.mount.point", CALI_TYPE_STRING, CALI_ATTR_SCOPE_THREAD | CALI_ATTR_SKIP_EVENTS);

    io_bytes_read_attr = c->create_attribute(
        "io.bytes.read",
        CALI_TYPE_UINT,
        CALI_ATTR_SCOPE_THREAD | CALI_ATTR_ASVALUE | CALI_ATTR_AGGREGATABLE
    );
    io_bytes_written_attr = c->create_attribute(
        "io.bytes.written",
        CALI_TYPE_UINT,
        CALI_ATTR_SCOPE_THREAD | CALI_ATTR_ASVALUE | CALI_ATTR_AGGREGATABLE
    );

    // register Caliper post_init_evt and finish_evt callbacks
    channel->events().post_init_evt.connect(init_curious_in_channel);

    // finish_evt: unregister channel from curious (??)
    channel->events().pre_finish_evt.connect(finalize_curious_in_channel);

    Log(1).stream() << channel->name() << ": Registered io service" << std::endl;
}
} // namespace

namespace cali
{

CaliperService io_service { "io", ::init_curious_service };

}
