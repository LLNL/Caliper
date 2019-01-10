#include <caliper/cali.h>
#include <caliper/Caliper.h>

#include <caliper/common/RuntimeConfig.h>

#include <condition_variable>
#include <mutex>
#include <thread>

using namespace cali;

std::condition_variable flag_cv;
std::mutex flag_mtx;
bool flag = false;

void thread_fn()
{
    std::unique_lock<std::mutex>
        g(flag_mtx);
    flag_cv.wait(g, [] { return flag; } );

    cali_begin_byname("thread");
    cali_end_byname("thread");
}

int main()
{
    Caliper c;

    Attribute chn_id_attr =
        c.create_attribute("chn.id", CALI_TYPE_INT, CALI_ATTR_SCOPE_PROCESS);

    for (int i = 1; i < 21; ++i) {
        std::string s("chn.");
        s.append(std::to_string(i));

        Channel* chn = c.create_channel(s.c_str(), RuntimeConfig::get_default_config());

        c.set(chn, chn_id_attr, Variant(i));
    }

    std::thread t(thread_fn);

    // create second half of channels after the new thread is active
    
    for (int i = 21; i <= 42; ++i) {
        std::string s("chn.");
        s.append(std::to_string(i));

        Channel* chn = c.create_channel(s.c_str(), RuntimeConfig::get_default_config());

        c.set(chn, chn_id_attr, Variant(i));
    }

    cali_begin_byname("main");

    // wake up the thread
    
    {
        std::lock_guard<std::mutex>
            g(flag_mtx);
        flag = true;
    }
    
    flag_cv.notify_one();
    t.join();
    
    cali_end_byname("main");
}
