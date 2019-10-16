// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// Caliper annotation bindings for Intel VTune Task API

#include "caliper/AnnotationBinding.h"

#include "caliper/common/Attribute.h"
#include "caliper/common/Variant.h"

#include <ittnotify.h>

#include <map>

using namespace cali;

namespace
{

class ITTBinding : public cali::AnnotationBinding
{
    static thread_local std::map<cali_id_t, __itt_domain*> s_itt_domains;

    // We do map string pointers here: expectation is that names come mostly
    // from const strings that don't change. Will be more robust when
    // Caliper gets its string storage.
    static thread_local std::map<const char*, __itt_string_handle*> s_itt_strings;

    inline __itt_domain* get_itt_domain(const Attribute& attr) {
        __itt_domain* domain = nullptr;
        cali_id_t attr_id = attr.id();

        {
            auto it = s_itt_domains.lower_bound(attr_id);

            if (it == s_itt_domains.end() || it->first != attr_id) {
                domain = __itt_domain_create(attr.name_c_str());
                s_itt_domains.insert(it, std::make_pair(attr_id, domain));
            } else {
                domain = it->second;
            }
        }

        return domain;
    }

    inline __itt_string_handle* get_itt_string(const Variant& val) {
        __itt_string_handle* itt_str = nullptr;

        {
            const char* str = static_cast<const char*>(val.data());
            auto it = s_itt_strings.lower_bound(str);

            if (it == s_itt_strings.end() || it->first != str) {
                itt_str = __itt_string_handle_create(str);
                s_itt_strings.insert(it, std::make_pair(str, itt_str));
            } else {
                itt_str = it->second;
            }
        }

        return itt_str;
    }


public:

    const char* service_tag() const { return "vtune"; };

    void on_begin(Caliper* c, Channel*, const Attribute& attr, const Variant& value) {
        if (attr.type() == CALI_TYPE_STRING)
            __itt_task_begin(get_itt_domain(attr), __itt_null, __itt_null, get_itt_string(value));
    }

    void on_end(Caliper* c, Channel*, const Attribute& attr, const Variant& value) {
        if (attr.type() == CALI_TYPE_STRING)
            __itt_task_end(get_itt_domain(attr));
    }
};

thread_local std::map<cali_id_t, __itt_domain*> ITTBinding::s_itt_domains;
thread_local std::map<const char*, __itt_string_handle*> ITTBinding::s_itt_strings;

} // namespace [anonymous]


namespace cali
{

CaliperService vtune_service { "vtune", &AnnotationBinding::make_binding<::ITTBinding> };

}
