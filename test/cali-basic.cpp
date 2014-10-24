// A basic Caliper instrumentation demo / test file

#include <Annotation.h>
#include <Caliper.h>
#include <Csv.h>

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using namespace std;

void begin_foo_op()
{
    // Begin "foo"->"fooing" and keep it alive past the end of the current C++ scope
    cali::Annotation("foo", cali::Annotation::KeepAlive).begin("fooing");
}

void end_foo_op()
{
    // Explicitly end innermost level of "foo" annotation
    cali::Annotation("foo").end();
}


void print_context()
{
    cali::Caliper* c { cali::Caliper::instance() };
    ctx_id_t     env { c->current_environment()  };

    vector<uint64_t> ctx(c->context_size(env), 0);

    cout << "Context size: " << ctx.size() << endl;

    // retrieve current context record
    const size_t ctxsize = c->get_context(env, ctx.data(), ctx.size());

    for (auto const &q : c->unpack(ctx.data(), ctxsize))
        cout << q << "\n";

    cout << endl;
}


int main(int argc, char* argv[])
{
    // Declare "phase" annotation
    cali::Annotation phase("phase");

    // Begin scope of phase->"main"
    phase.begin("main");

    int count = argc > 1 ? atoi(argv[1]) : 4;

    // An annotation with a user-defined datatype

    struct my_weird_type {
        unsigned u = 42;
        char     c = 'c';
        float    f = 42.42;
    } my_weird_elem;

    cali::Annotation usr = cali::Annotation("mydata");
    usr.set(CTX_TYPE_USR, &my_weird_elem, sizeof(my_weird_elem));

    // Add new scope phase->"loop" under phase->"main" 
    phase.begin("loop");

    {
        // Set "loopcount" annotation to 'count' in current C++ scope
        auto a = cali::Annotation::set("loopcount", count);

        // Declare "iteration" annotation, store entries explicitly as values
        cali::Annotation iteration("iteration", cali::Annotation::StoreAsValue);

        for (int i = 0; i < count; ++i) {
            // Set "iteration" annotation to current value of 'i'
            iteration.set(i);

            begin_foo_op();
            print_context();

            end_foo_op();
            print_context();
        }

        // "loopcount" and "iteration" annotations implicitly end here 
    }

    // End innermost level phase->"loop"
    phase.end();
    print_context();

    // explicitly end "usr"
    usr.end();

    // Test serialization API
    {
        cali::Caliper* caliper = cali::Caliper::instance();

        using std::placeholders::_1;

        cali::CsvWriter().write(std::bind(&cali::Caliper::foreach_attribute, caliper, _1),
                                std::bind(&cali::Caliper::foreach_node,      caliper, _1));
    }

    // implicitly end phase->"main"
}
