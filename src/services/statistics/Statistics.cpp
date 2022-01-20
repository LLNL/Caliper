// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// Statistics.cpp
// Caliper statistics service

#include "caliper/CaliperService.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/common/Log.h"

#include <atomic>
#include <mutex>

using namespace cali;

namespace
{

class Statistics
{
    std::atomic<unsigned> num_snapshots;

    std::atomic<unsigned> num_begin;
    std::atomic<unsigned> num_end;
    std::atomic<unsigned> num_set;

    std::mutex lock;

    unsigned max_snapshot_len;

    unsigned num_threads;
    unsigned max_threads;

    void finish_cb(Caliper* c, Channel* chn) {
        Log(1).stream() << chn->name() << ": statistics:"
                        << "\n  Number of begin events: " << num_begin.load()
                        << "\n  Number of end events:   " << num_end.load()
                        << "\n  Number of set events:   " << num_set.load()
                        << "\n  Number of snapshots:    " << num_snapshots.load()
                        << "\n  Max snapshot entries:   " << max_snapshot_len
                        << std::endl;

        if (chn->id() == 0) {
            // only print this for the default channel

            auto vec = c->get_all_attributes();

            unsigned n_global = 0;
            unsigned n_hidden_ref = 0;
            unsigned n_hidden_val = 0;
            unsigned n_val    = 0;
            unsigned n_ref    = 0;

            for (const Attribute& attr : vec) {
                if (attr.is_hidden()) {
                    if (attr.store_as_value())
                        ++n_hidden_val;
                    else
                        ++n_hidden_ref;
                    continue;
                }

                if (attr.is_global()) {
                    ++n_global;
                    continue;
                }

                if(attr.store_as_value())
                    ++n_val;
                else
                    ++n_ref;
            }

            Log(1).stream() << "Global statistics:"
                            << "\n  Number of attributes:   " << vec.size()
                            << "\n    reference:            " << n_ref
                            << "\n    value:                " << n_val
                            << "\n    global:               " << n_global
                            << "\n    hidden:               " << (n_hidden_ref + n_hidden_val)
                            << " (" << n_hidden_ref << " reference, " << n_hidden_val << " value)"
                            << "\n  Number of threads:      " << num_threads
                            << " (max " << max_threads << ")" << std::endl;
        }
    }

    Statistics()
        : num_snapshots(0),
          num_begin(0),
          num_end(0),
          num_set(0),
          max_snapshot_len(0),
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
            [instance](Caliper*,Channel*,int,SnapshotView,SnapshotBuilder& rec){
                ++instance->num_snapshots;
            });
        chn->events().process_snapshot.connect(
            [instance](Caliper*,Channel*,SnapshotView,SnapshotView rec){
                std::lock_guard<std::mutex>
                    g(instance->lock);

                instance->max_snapshot_len = std::max<unsigned int>(rec.size(), instance->max_snapshot_len);
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
