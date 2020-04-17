// --- Caliper continuous integration test app: threads

#include "caliper/cali.h"
#include "caliper/Caliper.h"

#include "caliper/common/RuntimeConfig.h"

#include <pthread.h>

cali_id_t thread_attr_id = CALI_INV_ID;

void* thread_proc(void* arg)
{
    CALI_CXX_MARK_FUNCTION;

    int thread_id = *(static_cast<int*>(arg));
    cali_set_int(thread_attr_id, thread_id);

    return NULL;
}

int main()
{
    CALI_CXX_MARK_FUNCTION;

    thread_attr_id =
        cali_create_attribute("my_thread_id", CALI_TYPE_INT, CALI_ATTR_SCOPE_THREAD | CALI_ATTR_UNALIGNED);

    cali::Caliper c;
    cali::RuntimeConfig exp_nopthread_cfg;

    exp_nopthread_cfg.set("CALI_SERVICES_ENABLE", "event,trace,recorder");
    exp_nopthread_cfg.set("CALI_CHANNEL_SNAPSHOT_SCOPES", "process,thread,channel");
    exp_nopthread_cfg.set("CALI_RECORDER_FILENAME", "stdout");

    cali::Channel* exp_nopthread =
        c.create_channel("exp_nopthread", exp_nopthread_cfg);

    cali::RuntimeConfig exp_pthread_cfg;

    exp_pthread_cfg.set("CALI_SERVICES_ENABLE", "event,trace,pthread,recorder");
    exp_pthread_cfg.set("CALI_CHANNEL_SNAPSHOT_SCOPES", "process,thread,channel");
    exp_pthread_cfg.set("CALI_RECORDER_FILENAME", "stdout");

    cali::Channel* exp_pthread =
        c.create_channel("exp_pthread", exp_pthread_cfg);

    cali::Attribute nopthread_attr =
        c.create_attribute("nopthread_exp", CALI_TYPE_BOOL, CALI_ATTR_SCOPE_PROCESS);
    cali::Attribute pthread_attr =
        c.create_attribute("pthread_exp",   CALI_TYPE_BOOL, CALI_ATTR_SCOPE_PROCESS);

    c.set(exp_nopthread, nopthread_attr, cali::Variant(true));
    c.set(exp_pthread,   pthread_attr,   cali::Variant(true));

    int       thread_ids[4] = { 16, 25, 36, 49 };
    pthread_t thread[4];

    cali::Annotation("local",  CALI_ATTR_SCOPE_THREAD  | CALI_ATTR_UNALIGNED).set(99);
    cali::Annotation("global", CALI_ATTR_SCOPE_PROCESS | CALI_ATTR_UNALIGNED).set(999);

    for (int i = 0; i < 4; ++i)
        pthread_create(&thread[i], NULL, thread_proc, &thread_ids[i]);

    for (int i = 0; i < 4; ++i)
        pthread_join(thread[i], NULL);
}
