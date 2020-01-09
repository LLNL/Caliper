// --- Caliper continuous integration test app for C++ annotation class

#include "caliper/cali.h"

int main()
{
    std::map<const char*, cali::Variant> metadata = {
        { "meta.int", cali::Variant(42) }
    };

    // Test proper escaping
    cali_set_string_byname(" =\\weird \"\"attribute\"=  ", "  \\\\ weird,\" name\",");
    cali_set_global_string_byname(" =\\weird \"\" global attribute\"=  ", "  \\\\ weird,\" name\",");

    cali::Annotation phase_ann("phase", metadata);
    std::size_t size = 8;
    cali::Annotation size_annot("dgs");
    size_annot.begin(size);
    phase_ann.begin("initialization");
    const int count = 4;
    phase_ann.end();

    cali::Annotation copy_ann = phase_ann;
    copy_ann.begin("loop");

    cali::Annotation iter_ann("iteration", CALI_ATTR_ASVALUE);
    iter_ann.begin(uint64_t(5));
    for (int i = 0; i < count; ++i) {
        cali::Annotation::Guard
            g_iter_ann(iter_ann.begin(i));
    }

    phase_ann = copy_ann;
    phase_ann.end();
}
