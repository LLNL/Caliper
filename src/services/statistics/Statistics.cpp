// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// Statistics.cpp
// Caliper statistics service

#include "caliper/CaliperService.h"

#include "caliper/Caliper.h"

#include "caliper/common/Log.h"

#include <atomic>

using namespace cali;

namespace
{

class Statistics
{
    std::atomic<unsigned> num_snapshots;

    std::atomic<unsigned> num_begin;
    std::atomic<unsigned> num_end;
    std::atomic<unsigned> num_set;

    unsigned num_threads;
    unsigned max_threads;

    void finish_cb(Caliper* c, Channel* chn) {
        Log(1).stream() << chn->name() << ": statistics:"
                        << "\n  Number of begin events: " << num_begin.load()
                        << "\n  Number of end events:   " << num_end.load()
                        << "\n  Number of set events:   " << num_set.load()
                        << "\n  Number of snapshots:    " << num_snapshots.load()
                        << std::endl;

        if (chn->id() == 0) { 
            // only print this for the default channel

            auto vec = c->get_all_attributes();
            
            Log(1).stream() << "Global statistics:"
                            << "\n  Number of attributes:   " << vec.size()
                            << "\n  Number of threads:      " << num_threads
                            << " (max " << max_threads << ")" << std::endl;
        }
    }

    Statistics()
        : num_snapshots(0),
          num_begin(0),
          num_end(0),
          num_set(0),
          num_threads(1),
          max_threads(1)
        { }

public:
    
    static void statistics_service_register(Caliper* c, Channel* chn) {
        Statistics* instance = new Statistics;
        
        chn->events().pre_begin_evt.connect(
            [instance](Caliper* c, Channel* chn, const Attribute&, const Variant&){
                ++instance->num_begin;
            });
        chn->events().pre_end_evt.connect(
            [instance](Caliper* c, Channel* chn, const Attribute&, const Variant&){
                ++instance->num_end;
            });
        chn->events().pre_set_evt.connect(
            [instance](Caliper* c, Channel* chn, const Attribute&, const Variant&){
                ++instance->num_set;
            });
        chn->events().snapshot.connect(
            [instance](Caliper*,Channel*,int,const SnapshotRecord*,SnapshotRecord*){
                ++instance->num_snapshots;
            });

        if (chn->id() == 0) {
            chn->events().create_thread_evt.connect(
                [instance](Caliper* c, Channel* chn){
                    ++instance->num_threads;
                    instance->max_threads =
                        std::max(instance->num_threads, instance->max_threads);
                });
            chn->events().release_thread_evt.connect(
                [instance](Caliper* c, Channel* chn){
                    --instance->num_threads;
                });
        }
        
        chn->events().finish_evt.connect(
            [instance](Caliper* c, Channel* chn){
                instance->finish_cb(c, chn);
                delete instance;
            });

        Log(1).stream() << chn->name() << ": Registered statistics service" << std::endl;
    }
};

} // namespace

namespace cali
{

CaliperService statistics_service = { "statistics", ::Statistics::statistics_service_register };

}
