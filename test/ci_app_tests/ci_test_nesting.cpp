#include "caliper/cali.h"
#include "caliper/Caliper.h"

#include <pthread.h>

#include <iostream>
#include <cstring>

using namespace cali;


namespace 
{

void* mismatch_thread_fn(void*)
{
    Caliper c;

    Attribute tA = 
        c.create_attribute("mismatch-thread.A", CALI_TYPE_INT, 
                           CALI_ATTR_SCOPE_THREAD | CALI_ATTR_NESTED);
    Attribute tN = 
        c.create_attribute("mismatch-thread.N", CALI_TYPE_INT, CALI_ATTR_SCOPE_THREAD);
    Attribute tB = 
        c.create_attribute("mismatch-thread.B", CALI_TYPE_INT, 
                           CALI_ATTR_SCOPE_THREAD | CALI_ATTR_NESTED);

    c.begin(tA, Variant(16));
    c.begin(tN, Variant(25));
    c.begin(tB, Variant(32));
    c.end(tN); // this should work: non-nested attribute
    c.end(tA); // error: incorrect nesting!
    c.end(tB);

    return NULL;
}

} // namespace 

void test_nesting_threadscope()
{
    pthread_t tid;

    pthread_create(&tid, NULL, ::mismatch_thread_fn, NULL);
    pthread_join(tid, NULL);
}

void test_nesting_procscope()
{
    Caliper c;
    Variant v_true(true);

    Attribute tA = 
        c.create_attribute("mismatch-process.A", CALI_TYPE_INT, 
                           CALI_SCOPE_PROCESS | CALI_ATTR_NESTED);
    Attribute tN = 
        c.create_attribute("mismatch-process.N", CALI_TYPE_INT, CALI_SCOPE_PROCESS);
    Attribute tB = 
        c.create_attribute("mismatch-process.B", CALI_TYPE_INT, 
                           CALI_SCOPE_PROCESS | CALI_ATTR_NESTED);

    c.begin(tA, Variant(16));
    c.begin(tN, Variant(25));
    c.begin(tB, Variant(32));
    c.end(tN); // this should work: non-nested attribute
    c.end(tA); // error: incorrect nesting!
    c.end(tB);    
}

void test_nesting_end_missing()
{
    Caliper c;

    Attribute tA = 
        c.create_attribute("missing-end.A", CALI_TYPE_STRING, CALI_ATTR_SCOPE_THREAD);
    Attribute tN = 
        c.create_attribute("missing-end.N", CALI_TYPE_STRING, CALI_ATTR_SCOPE_THREAD);

    c.begin(tN, Variant(CALI_TYPE_STRING, "no.error.0", 11));
    c.begin(tA, Variant(CALI_TYPE_STRING, "missing.0",  10));
    c.begin(tN, Variant(CALI_TYPE_STRING, "no.error.1", 11));
    c.begin(tA, Variant(CALI_TYPE_STRING, "missing.1",  10));
    c.begin(tA, Variant(CALI_TYPE_STRING, "no.error.2", 11));

    c.end(tN);
    c.end(tA);
    c.end(tN);
}


int main(int argc, char* argv[])
{
    const struct test_info_t {
        const char* name;
        void (*fn)(void);
    } test_info[] = {
        { "nesting_threadscope", test_nesting_threadscope },
        { "nesting_procscope",   test_nesting_procscope   },
        { "nesting_end_missing", test_nesting_end_missing },
        { 0, 0 }
    };

    Caliper c;

    // --- check if argument is valid test case

    if (argc > 1) {
        const test_info_t* t = test_info;
        for ( ; t->name && 0 != strcmp(argv[1], t->name); ++t)
            ;

        if (!t->name) {
            std::cerr << "No test case \"" << argv[1] << "\" found!" << std::endl;
            return 1;
        }
    }

    // --- run tests

    for (const test_info_t* t = test_info; t->fn; ++t)
        if (argc == 1 || (argc > 1 && 0 == strcmp(argv[1], t->name)))
            (*(t->fn))();

    return 0;
}
