// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "caliper/CaliperService.h"

#include "caliper/Caliper.h"

#include "caliper/common/Log.h"
#include "caliper/common/Node.h"

#include <adiak_tool.h>

#include <cstring>
#include <ctime>
#include <memory>
#include <sstream>

using namespace cali;

namespace
{

Attribute meta_attr[2];

unsigned s_unknown_type_error = 0;

std::ostream&
recursive_unpack(std::ostream& os, adiak_value_t *val, adiak_datatype_t* t)
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
    case adiak_timeval:
    {
        struct timeval *tval = static_cast<struct timeval*>(val->v_ptr);
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
        adiak_value_t* subvals = static_cast<adiak_value_t*>(val->v_ptr);
        recursive_unpack(os, subvals+0, t->subtype[0]);
        os << '-';
        recursive_unpack(os, subvals+1, t->subtype[0]);
    }
    break;
    case adiak_set:
    {
        adiak_value_t* subvals = static_cast<adiak_value_t*>(val->v_ptr);
        os << '[';
        for (int i = 0; i < t->num_elements; ++i) {
            if (i > 0)
                os << ',';
            recursive_unpack(os, subvals+i, t->subtype[0]);
        }
        os << ']';
    }
    break;
    case adiak_list:
    {
        adiak_value_t* subvals = static_cast<adiak_value_t*>(val->v_ptr);
        os << '{';
        for (int i = 0; i < t->num_elements; ++i) {
            if (i > 0)
                os << ',';
            recursive_unpack(os, subvals+i, t->subtype[0]);
        }
        os << '}';
    }
    break;
    case adiak_tuple:
    {
        adiak_value_t* subvals = static_cast<adiak_value_t*>(val->v_ptr);
        os << '(';
        for (int i = 0; i < t->num_elements; ++i) {
            if (i > 0)
                os << ',';
            recursive_unpack(os, subvals+i, t->subtype[i]);
        }
        os << ')';
    }
    break;
    default:
        ++s_unknown_type_error;
    }

    return os;
}

void
set_val(Channel* channel, const char* name, const Variant& val, adiak_datatype_t* type, adiak_category_t, const char *subcategory)
{
    Caliper c;
    Variant v_metavals[2];

    std::shared_ptr<char> typestr(adiak_type_to_string(type, 1), free);
    v_metavals[0] = Variant(CALI_TYPE_STRING, typestr.get(), strlen(typestr.get())+1);

    if (!subcategory || subcategory[0] == '\0')
       subcategory = "none";
    v_metavals[1] = Variant(CALI_TYPE_STRING, subcategory, strlen(subcategory)+1);

    Attribute attr =
       c.create_attribute(name, val.type(), CALI_ATTR_GLOBAL | CALI_ATTR_SKIP_EVENTS,
                          2, meta_attr, v_metavals);

    c.set(channel, attr, val);
}

struct nameval_usr_args_t {
    Channel* channel;
    int count;
};

void
nameval_cb(const char *name, adiak_category_t category, const char *subcategory, adiak_value_t *val, adiak_datatype_t *t, void* usr_args)
{
    nameval_usr_args_t* args = static_cast<nameval_usr_args_t*>(usr_args);

    Channel* channel = args->channel;

    if (!channel)
        return;
    if (category == adiak_control)
       return;

    Caliper c;

    switch (t->dtype) {
    case adiak_type_unset:
    {
        Attribute attr = c.get_attribute(name);

        if (attr != Attribute::invalid && attr.is_global())
            c.end(attr);
        else
            Log(0).stream() << "adiak: unset invoked for unknown key " << name << std::endl;

        return;
    }
    case adiak_long:
        set_val(channel, name, Variant(static_cast<int>(val->v_long)), t, category, subcategory);
        ++args->count;
        break;
    case adiak_int:
        set_val(channel, name, Variant(val->v_int), t, category, subcategory);
        ++args->count;
        break;
    case adiak_ulong:
        set_val(channel, name, Variant(static_cast<uint64_t>(val->v_long)), t, category, subcategory);
        ++args->count;
        break;
    case adiak_uint:
        set_val(channel, name, Variant(static_cast<uint64_t>(val->v_int)), t, category, subcategory);
        ++args->count;
        break;
    case adiak_double:
        set_val(channel, name, Variant(val->v_double), t, category, subcategory);
        ++args->count;
        break;
    case adiak_date:
        set_val(channel, name, Variant(static_cast<uint64_t>(val->v_long)), t, category, subcategory);
        ++args->count;
        break;
    case adiak_timeval:
    {
        struct timeval *tval = static_cast<struct timeval*>(val->v_ptr);
        set_val(channel, name, Variant(tval->tv_sec + tval->tv_usec / 1000000.0), t, category, subcategory);
        ++args->count;
        break;
    }
    case adiak_version:
    case adiak_string:
    case adiak_catstring:
    case adiak_path:
        set_val(channel, name, Variant(CALI_TYPE_STRING, val->v_ptr, strlen(static_cast<const char*>(val->v_ptr))), t, category, subcategory);
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

        set_val(channel, name, Variant(CALI_TYPE_STRING, str.c_str(), str.length()+1), t, category, subcategory);
        ++args->count;
        break;
    }
    default:
        ++s_unknown_type_error;
    }
}

void
register_adiak_import(Caliper* c, Channel* chn)
{
    meta_attr[0] =
        c->create_attribute("adiak.type", CALI_TYPE_STRING, CALI_ATTR_DEFAULT | CALI_ATTR_SKIP_EVENTS);
    meta_attr[1] =
       c->create_attribute("adiak.category", CALI_TYPE_STRING, CALI_ATTR_DEFAULT | CALI_ATTR_SKIP_EVENTS);

    chn->events().pre_flush_evt.connect(
        [](Caliper*, Channel* chn, const SnapshotRecord*){
            nameval_usr_args_t args { chn, 0 };

            adiak_list_namevals(1, adiak_category_all, nameval_cb, &args);

            Log(1).stream() << chn->name() << ": adiak_import: Imported " << args.count
                            << " adiak values" << std::endl;
        });

    Log(1).stream() << chn->name() << ": Registered adiak_import service" << std::endl;
}

} // namespace [anonymous]

namespace cali
{

CaliperService adiak_import_service { "adiak_import", ::register_adiak_import };

}
