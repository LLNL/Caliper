// Copyright (c) 2024, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// Example for using asynchronous timed events across threads

#include <caliper/cali.h>
#include <caliper/cali-manager.h>

#include <caliper/AsyncEvent.h>

#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>

std::queue<cali::TimedAsyncEvent> q;
std::mutex q_mtx;
std::condition_variable cv;
bool done = false;

void consumer_thread_fn()
{
    CALI_CXX_MARK_FUNCTION;

    while (true) {
        cali::TimedAsyncEvent evt;

        CALI_MARK_BEGIN("waiting");
        {
            std::unique_lock<std::mutex> g(q_mtx);
            cv.wait(g, [](){ return !q.empty() || done; });
            if (!q.empty()) {
                evt = q.front();
                q.pop();
            } else
                return;
        }
        CALI_MARK_END("waiting");

        evt.end();

        CALI_CXX_MARK_SCOPE("processing");
        std::this_thread::sleep_for(std::chrono::microseconds(200));
    }
}

int main(int argc, char* argv[])
{
    cali::ConfigManager mgr;
    mgr.set_default_parameter("aggregate_across_ranks", "false");

    if (argc > 1) {
        mgr.add(argv[1]);
        if (mgr.error()) {
            std::cerr << "ConfigManager: " << mgr.error_msg() << std::endl;
            return -1;
        }
    }

    mgr.start();
    cali_init(); // initialize Caliper before creating sub-thread

    std::thread consumer(consumer_thread_fn);

    CALI_MARK_BEGIN("main_thread");

    int N = 200;

    CALI_MARK_BEGIN("producing");

    for (int i = 0; i < N; ++i) {
        q_mtx.lock();
        q.push(cali::TimedAsyncEvent::begin("queue_wait"));
        q_mtx.unlock();
        cv.notify_one();
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }

    CALI_MARK_END("producing");

    {
        std::lock_guard<std::mutex> g(q_mtx);
        done = true;
    }

    cv.notify_all();

    CALI_MARK_BEGIN("waiting");
    consumer.join();
    CALI_MARK_END("waiting");
    CALI_MARK_END("main_thread");

    mgr.flush();
}