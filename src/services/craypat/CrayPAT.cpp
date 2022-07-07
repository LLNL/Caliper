// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// Caliper RocTX bindings

#include "caliper/AnnotationBinding.h"

#include "caliper/common/Attribute.h"
#include "caliper/common/Log.h"

#include <pat_api.h>

#include <cstdlib>
/*
extern char** environ;

extern "C" 
{
    void *sbrk(__intptr_t increment);
    void __pat_init (int, const char *[ ], const char *[ ], const void *, const void *);
    void __pat_exit (void);
}
*/
using namespace cali;

namespace
{

class CrayPATBinding : public AnnotationBinding
{

public:

    void on_begin(Caliper*, Channel*, const Attribute &attr, const Variant& value) {
        if (attr.is_nested()) {
            const char* msg = nullptr;
            std::string str;

            if (attr.type() == CALI_TYPE_STRING) {
                msg = static_cast<const char*>(value.data());
            } else {
                str = value.to_string();
                msg = str.c_str();
            }

            PAT_region_push(msg);
        }
    }

    void on_end(Caliper*, Channel*, const Attribute& attr, const Variant& value) {
        if (attr.is_nested()) {
            const char* msg = nullptr;
            std::string str;

            if (attr.type() == CALI_TYPE_STRING) {
                msg = static_cast<const char*>(value.data());
            } else {
                str = value.to_string();
                msg = str.c_str();
            }

            PAT_region_pop(msg);
        }
    }

/*
    void initialize(Caliper*, Channel*) {
        const char* argv[] = { "" };
        putenv(const_cast<char*>("PAT_RT_EXPERIMENT=trace"));
        putenv(const_cast<char*>("PAT_RT_CALLSTACK_MODE=trace"));
        __pat_init(1, argv, const_cast<const char**>(environ), __builtin_frame_address (0), sbrk (0L));
    }

    void finalize(Caliper*, Channel*) {
        __pat_exit();
    }
*/
    const char* service_tag() const { return "craypat"; }
};

} // namespace [anonymous]

namespace cali
{

CaliperService craypat_service { "craypat", AnnotationBinding::make_binding<CrayPATBinding> };

}
