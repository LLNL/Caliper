#include "caliper/Annotation.h"
    
void foo(int c) {
    cali::Annotation::Guard
        g( cali::Annotation("function").begin("foo") );

    // ...
}

int main()
{
    {   // "A" loop
        cali::Annotation::Guard
            g( cali::Annotation("loop.id", CALI_ATTR_NESTED).begin("A") );
        
        for (int i = 0; i < 3; ++i) {
            cali::Annotation("iteration", CALI_ATTR_ASVALUE).set(i);            

            foo(1);
            foo(2);
        }
    }

    {   // "B" loop
        cali::Annotation::Guard
            g( cali::Annotation("loop.id", CALI_ATTR_NESTED).begin("B") );
        
        for (int i = 0; i < 4; ++i) {
            cali::Annotation("iteration", CALI_ATTR_ASVALUE).set(i);
            
            foo(1);
        }
    }
}
