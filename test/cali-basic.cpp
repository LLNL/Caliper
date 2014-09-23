// A basic Caliper instrumentation demo / test file

#include <Annotation.h>
#include <Caliper.h>

#include <cstdlib>
#include <iostream>
#include <string>


void begin_foo_op()
{
    cali::Annotation("foo", cali::Annotation::KeepAlive).begin("fooing");
}

void end_foo_op()
{
    cali::Annotation("foo").end();
}

int main(int argc, char* argv[])
{
    cali::Annotation phase { "phase" };

    phase.begin("main");

    int count = argc > 1 ? atoi(argv[1]) : 42;
        
    cali::Annotation("loopcount").set(count);
    cali::Annotation iteration { "iteration", cali::Annotation::StoreAsValue };

    phase.begin("loop");

    for (int i = 0; i < count; ++i) {
        iteration.set(i);

        begin_foo_op();

        std::cout << "Context size " << cali::Caliper::instance()->context_size(0) << std::endl; 

        end_foo_op();

        std::cout << "Context size " << cali::Caliper::instance()->context_size(0) << std::endl;
    }

    phase.end();    
}
