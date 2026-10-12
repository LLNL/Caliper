// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// Caliper annotation bindings for the Ariel API, a simulation
// component in SST-Elements.

#include "../Services.h"

#include "../../caliper/AnnotationBinding.h"

#include "caliper/common/Attribute.h"
#include "caliper/common/Variant.h"

#include <arielapi.h>
#include <inttypes.h>

using namespace cali;

namespace
{

class ArielBinding : public cali::AnnotationBinding
{

    bool enabled = false;
    int depth = 0;

public:

    const char* service_tag() const { return "ariel"; };

    void on_begin(Caliper* c, const Attribute& attr, const Variant& value) override
    {
        if (!enabled) {
            ariel_enable();
            enabled=true;
        }
        depth++;
        enabled = true;
        if (attr.type() == CALI_TYPE_STRING) {
            const char* str = static_cast<const char*>(value.data());
            ariel_output_stats_begin_region(str);
        } else {
            printf("ARIEL-CALI-WARNING: Unknown attr.type()\n");
        }
    }

    void on_end(Caliper* c, const Attribute& attr, const Variant& value) override
    {
        depth--;
        if (depth == 0) {
            ariel_disable();
            enabled = false;
        }
        if (attr.type() == CALI_TYPE_STRING) {
            const char* str = static_cast<const char*>(value.data());
            ariel_output_stats_end_region(str);
        } else {
            printf("ARIEL-CALI-WARNING: Unknown attr.type()\n");
        }
    }

};

} // namespace

namespace cali
{

CaliperService ariel_service { "ariel", &AnnotationBinding::make_binding<::ArielBinding> };

}
