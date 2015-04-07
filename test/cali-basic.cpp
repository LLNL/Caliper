// A minimal Caliper instrumentation demo 

#include <Annotation.h>

int main(int argc, char* argv[])
{
    // Create an annotation object for the "phase" annotation
    cali::Annotation phase_annotation("phase");

    // Declare begin of phase "main" 
    phase_annotation.begin("main");

    // Declare begin of phase "init" under "main". Thus, "phase" is now set to "main/init"
    phase_annotation.begin("init");
    int count = 4;
    // End innermost level of the phase annotation. Thus, "phase" is now set to "main"
    phase_annotation.end();

    if (count > 0) {
        // Declare begin of phase "loop" under "main". Thus, phase is now set to "main/loop".
        // The AutoScope object automatically closes this annotation level at the end
        // of the current C++ scope.
        cali::Annotation::AutoScope ann_loop( phase_annotation.begin("loop") );

        // Create iteration annotation object
        // The CALI_ATTR_ASVALUE option indicates that iteration values should be stored 
        // explicitly in each context record. 
        cali::Annotation iteration_annotation("iteration", CALI_ATTR_ASVALUE);
        
        for (int i = 0; i < count; ++i) {
            // Set "iteration" annotation to current value of 'i'
            iteration_annotation.set(i);
        }

        // Explicitly end the iteration annotation. This removes the iteration values 
        // from context records from here on. 
        iteration_annotation.end();

        // The ann_loop AutoScope object implicitly closes the "loop" annotation level here, 
        // thus phase is now back to "main"
    }

    // Close level "main" of the phase annotation, thus phase is now removed from context records
    phase_annotation.end();
}
