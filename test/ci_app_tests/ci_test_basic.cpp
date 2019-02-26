// --- Caliper continuous integration test app for C++ annotation class

#include "caliper/Annotation.h"

#include "caliper/common/Variant.h"

int main()
{
    std::map<const char*, cali::Variant> metadata = {
        { "meta.int", cali::Variant(42) }
    };

    cali::Annotation phase_ann("phase", metadata);

    phase_ann.begin("initialization");
    const int count = 4;
    phase_ann.end();

    cali::Annotation copy_ann = phase_ann;
    copy_ann.begin("loop");

    cali::Annotation iter_ann("iteration", CALI_ATTR_ASVALUE);

    for (int i = 0; i < count; ++i) {
        cali::Annotation::Guard
            g_iter_ann(iter_ann.begin(i));
    }

    phase_ann = copy_ann;
    phase_ann.end();
}
