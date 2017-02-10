// --- Caliper continuous integration test app for basic trace test

#include <Annotation.h>

int main() 
{
    cali::Annotation phase_ann("phase");

    phase_ann.begin("initialization");
    const int count = 4;
    phase_ann.end();

    phase_ann.begin("loop");
    
    cali::Annotation iter_ann("iteration", CALI_ATTR_ASVALUE);

    for (int i = 0; i < count; ++i) {
        cali::Annotation::Guard
            g_iter_ann(iter_ann.begin(i));
    }

    phase_ann.end();
}
