// --- Caliper continuous integration test app: threads

#include "caliper/cali.h"

#include <pthread.h>


void* thread_proc(void* arg)
{
    CALI_CXX_MARK_FUNCTION;

    int thread_id = *(static_cast<int*>(arg));

    cali::Annotation("my_thread_id").set(thread_id);

    return NULL;
}

int main() 
{
    CALI_CXX_MARK_FUNCTION;

    int       thread_ids[2] = { 42, 1337 };
    pthread_t thread[2];

    cali::Annotation("local", CALI_ATTR_DEFAULT).set(99);
    cali::Annotation("global", CALI_ATTR_SCOPE_PROCESS).set(999);

    for (int i = 0; i < 2; ++i)
        pthread_create(&thread[i], NULL, thread_proc, &thread_ids[i]);

    for (int i = 0; i < 2; ++i)
        pthread_join(thread[i], NULL);
}
