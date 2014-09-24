// A basic Caliper instrumentation demo / test file

#include <Annotation.h>
#include <Caliper.h>

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using namespace std;

void begin_foo_op()
{
    cali::Annotation("foo", cali::Annotation::KeepAlive).begin("fooing");
}

void end_foo_op()
{
    cali::Annotation("foo").end();
}

void print_context()
{
    cali::Caliper* c { cali::Caliper::instance() };
    ctx_id_t     env { c->current_environment()  };

    vector<uint64_t> ctx(c->context_size(env), 0);

    const size_t ctxsize = c->get_context(env, ctx.data(), ctx.size());

    cout << "Context size: " << ctxsize << ": ";

    for (size_t i = 0; i < ctxsize / 2; ++i)
        cout << "(" << ctx[i*2] << ", " << ctx[i*2+1] << ") ";

    cout << endl;
}

int main(int argc, char* argv[])
{
    cali::Annotation phase { "phase" };

    phase.begin("main");

    int count = argc > 1 ? atoi(argv[1]) : 42;

    phase.begin("loop");

    {
        auto a = cali::Annotation::set("loopcount", count);

        cali::Annotation iteration { "iteration", cali::Annotation::StoreAsValue };

        for (int i = 0; i < count; ++i) {
            iteration.set(i);

            begin_foo_op();
            print_context();

            end_foo_op();
            print_context();
        }
    }

    phase.end();
    print_context();
}
