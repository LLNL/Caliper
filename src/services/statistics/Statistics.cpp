// Copyright (c) 2015, Lawrence Livermore National Security, LLC.  
// Produced at the Lawrence Livermore National Laboratory.
//
// This file is part of Caliper.
// Written by David Boehme, boehme3@llnl.gov.
// LLNL-CODE-678900
// All rights reserved.
//
// For details, see https://github.com/scalability-llnl/Caliper.
// Please also see the LICENSE file for our additional BSD notice.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
//  * Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the disclaimer below.
//  * Redistributions in binary form must reproduce the above copyright notice, this list of
//    conditions and the disclaimer (as noted below) in the documentation and/or other materials
//    provided with the distribution.
//  * Neither the name of the LLNS/LLNL nor the names of its contributors may be used to endorse
//    or promote products derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS
// OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// LAWRENCE LIVERMORE NATIONAL SECURITY, LLC, THE U.S. DEPARTMENT OF ENERGY OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
// ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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

            auto vec = c->get_attributes();
            
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
