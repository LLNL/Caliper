// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "../Services.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/common/Log.h"
#include "caliper/common/Node.h"

#include <adiak_tool.h>

#include <cstdlib>
#include <cstring>
#include <ctime>
#include <memory>
#include <sstream>
#include <vector>

using namespace cali;

namespace
{

Attribute meta_attr[3];

unsigned s_unknown_type_error = 0;

std::ostream& recursive_unpack(std::ostream& os, adiak_value_t* val, adiak_datatype_t* t)
{
    switch (t->dtype) {
    case adiak_date:
    case adiak_long:
        os << val->v_long;
        break;
    case adiak_ulong:
        os << static_cast<unsigned long>(val->v_long);
        break;
    case adiak_int:
        os << val->v_int;
        break;
    case adiak_uint:
        os << static_cast<unsigned int>(val->v_int);
        break;
    case adiak_double:
        os << val->v_double;
        break;
#ifdef ADIAK_HAVE_LONGLONG
    case adiak_longlong:
        os << val->v_longlong;
        break;
    case adiak_ulonglong:
        os << static_cast<unsigned long long>(val->v_longlong);
        break;
#endif
    case adiak_timeval:
        {
            struct timeval* tval = static_cast<struct timeval*>(val->v_ptr);
            os << tval->tv_sec + tval->tv_usec / 1000000.0;
        }
        break;
    case adiak_version:
    case adiak_string:
    case adiak_catstring:
    case adiak_path:
        os << static_cast<const char*>(val->v_ptr);
        break;
    case adiak_range:
        {
            adiak_value_t     subvals[2];
            adiak_datatype_t* subtypes[2];

            adiak_get_subval(t, val, 0, subtypes + 0, subvals + 0);
            adiak_get_subval(t, val, 1, subtypes + 1, subvals + 1);

            recursive_unpack(os, subvals + 0, *(subtypes + 0));
            os << '-';
            recursive_unpack(os, subvals + 1, *(subtypes + 1));
        }
        break;
    case adiak_set:
        {
            os << '[';
            int n = adiak_num_subvals(t);
            for (int i = 0; i < n; i++) {
                adiak_value_t     subval;
                adiak_datatype_t* subtype;
                adiak_get_subval(t, val, i, &subtype, &subval);
                if (i > 0)
                    os << ',';
                recursive_unpack(os, &subval, subtype);
            }
            os << ']';
        }
        break;
    case adiak_list:
        {
            os << '{';
            int n = adiak_num_subvals(t);
            for (int i = 0; i < n; i++) {
                adiak_value_t     subval;
                adiak_datatype_t* subtype;
                adiak_get_subval(t, val, i, &subtype, &subval);
                if (i > 0)
                    os << ',';
                recursive_unpack(os, &subval, subtype);
            }
            os << '}';
        }
        break;
    case adiak_tuple:
        {
            os << '(';
            int n = adiak_num_subvals(t);
            for (int i = 0; i < n; i++) {
                adiak_value_t     subval;
                adiak_datatype_t* subtype;
                adiak_get_subval(t, val, i, &subtype, &subval);
                if (i > 0)
                    os << ',';
                recursive_unpack(os, &subval, subtype);
            }
            os << ')';
        }
        break;
    default:
        ++s_unknown_type_error;
    }

    return os;
}

void set_val(
    ChannelBody*      chB,
    const char*       name,
    const Variant&    val,
    adiak_datatype_t* type,
    adiak_category_t  category,
    const char*       subcategory
)
{
    Caliper c;
    Variant v_metavals[3];

    std::shared_ptr<char> typestr(adiak_type_to_string(type, 1), std::free);
    v_metavals[0] = Variant(typestr.get());
    v_metavals[1] = Variant(static_cast<int>(category));

    if (!subcategory || subcategory[0] == '\0')
        subcategory = "none";
    v_metavals[2] = Variant(subcategory);

    Attribute attr =
        c.create_attribute(name, val.type(), CALI_ATTR_GLOBAL | CALI_ATTR_SKIP_EVENTS, 3, meta_attr, v_metavals);

    c.set(chB, attr, val);
}

struct nameval_usr_args_t {
    ChannelBody* chB;
    int          count;
};

void nameval_cb(
    const char*       name,
    adiak_category_t  category,
    const char*       subcategory,
    adiak_value_t*    val,
    adiak_datatype_t* t,
    void*             usr_args
)
{
    nameval_usr_args_t* args = static_cast<nameval_usr_args_t*>(usr_args);

    if (!args->chB)
        return;
    if (category == adiak_control)
        return;

    Caliper c;

    switch (t->dtype) {
    case adiak_type_unset:
        {
            Attribute attr = c.get_attribute(name);

            if (attr && attr.is_global())
                c.end(attr);
            else
                Log(0).stream() << "adiak: unset invoked for unknown key " << name << std::endl;

            return;
        }
    case adiak_long:
        set_val(args->chB, name, Variant(static_cast<int>(val->v_long)), t, category, subcategory);
        ++args->count;
        break;
    case adiak_int:
        set_val(args->chB, name, Variant(val->v_int), t, category, subcategory);
        ++args->count;
        break;
    case adiak_ulong:
        set_val(args->chB, name, Variant(static_cast<uint64_t>(val->v_long)), t, category, subcategory);
        ++args->count;
        break;
#ifdef ADIAK_HAVE_LONGLONG
    case adiak_longlong:
        set_val(args->chB, name, Variant(cali_make_variant_from_int64(val->v_longlong)), t, category, subcategory);
        ++args->count;
        break;
    case adiak_ulonglong:
        set_val(
            args->chB,
            name,
            Variant(cali_make_variant_from_uint(static_cast<uint64_t>(val->v_longlong))),
            t,
            category,
            subcategory
        );
        ++args->count;
        break;
#endif
    case adiak_uint:
        set_val(args->chB, name, Variant(static_cast<uint64_t>(val->v_int)), t, category, subcategory);
        ++args->count;
        break;
    case adiak_double:
        set_val(args->chB, name, Variant(val->v_double), t, category, subcategory);
        ++args->count;
        break;
    case adiak_date:
        set_val(args->chB, name, Variant(static_cast<uint64_t>(val->v_long)), t, category, subcategory);
        ++args->count;
        break;
    case adiak_timeval:
        {
            struct timeval* tval = static_cast<struct timeval*>(val->v_ptr);
            set_val(args->chB, name, Variant(tval->tv_sec + tval->tv_usec / 1000000.0), t, category, subcategory);
            ++args->count;
            break;
        }
    case adiak_version:
    case adiak_string:
    case adiak_catstring:
    case adiak_path:
        set_val(
            args->chB,
            name,
            Variant(CALI_TYPE_STRING, val->v_ptr, strlen(static_cast<const char*>(val->v_ptr))),
            t,
            category,
            subcategory
        );
        ++args->count;
        break;
    case adiak_range:
    case adiak_set:
    case adiak_list:
    case adiak_tuple:
        {
            std::ostringstream os;
            recursive_unpack(os, val, t);
            std::string str = os.str();

            set_val(args->chB, name, Variant(CALI_TYPE_STRING, str.c_str(), str.length() + 1), t, category, subcategory);
            ++args->count;
            break;
        }
    default:
        ++s_unknown_type_error;
    }
}

const char* spec = R"json(
{
"name"        : "adiak_import",
"description" : "Import program run metadata from Adiak",
"config"      :
[
{
 "name": "categories",
 "type": "string",
 "description": "List of Adiak categories to import",
 "value": "2,3"
}
]}
)json";

void register_adiak_import(Caliper* c, Channel* channel)
{
    ConfigSet        cfg = services::init_config_from_spec(channel->config(), spec);
    std::vector<int> categories;

    for (const std::string& s : cfg.get("categories").to_stringlist())
        categories.push_back(std::stoi(s));

    meta_attr[0] = c->create_attribute("adiak.type", CALI_TYPE_STRING, CALI_ATTR_DEFAULT | CALI_ATTR_SKIP_EVENTS);
    meta_attr[1] = c->create_attribute("adiak.category", CALI_TYPE_INT, CALI_ATTR_DEFAULT | CALI_ATTR_SKIP_EVENTS);
    meta_attr[2] =
        c->create_attribute("adiak.subcategory", CALI_TYPE_STRING, CALI_ATTR_DEFAULT | CALI_ATTR_SKIP_EVENTS);

    std::string channel_name = channel->name();

    channel->events().pre_flush_evt.connect([categories,channel_name](Caliper*, ChannelBody* chB, SnapshotView) {
        nameval_usr_args_t args { chB, 0 };

        for (int category : categories)
            adiak_list_namevals(1, static_cast<adiak_category_t>(category), nameval_cb, &args);

        Log(1).stream() << channel_name << ": adiak_import: Imported " << args.count << " adiak values" << std::endl;
    });

    Log(1).stream() << channel_name << ": Registered adiak_import service" << std::endl;
}

} // namespace

namespace cali
{

CaliperService adiak_import_service { spec, ::register_adiak_import };

}
