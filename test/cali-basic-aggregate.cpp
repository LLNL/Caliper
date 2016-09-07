#include <Annotation.h>
    
void foo(int c) {
    cali::Annotation::Guard
        g( cali::Annotation("function").begin("foo") );

    // ...
}

int main()
{
    {   // "A" loop
        cali::Annotation::Guard
            g( cali::Annotation("loop.id").begin("A") );
        
        for (int i = 0; i < 3; ++i) {
            cali::Annotation("iteration").set(i);            

            foo(1);
            foo(2);
        }
    }

    {   // "B" loop
        cali::Annotation::Guard
            g( cali::Annotation("loop.id").begin("B") );
        
        for (int i = 0; i < 4; ++i) {
            cali::Annotation("iteration").set(i);
            
            foo(1);
        }
    }
}
