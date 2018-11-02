// Copyright (c) 2018, Lawrence Livermore National Security, LLC.
// Copyright (c) 2018, University of Oregon
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

/// @file  tau.cpp
/// @brief Caliper TAU service

// Caliper annotation bindings for TAU service

#include "caliper/AnnotationBinding.h"
#include "caliper/common/Attribute.h"
#include "caliper/common/Variant.h"
#include <map>
#define TAU_ENABLED
#define TAU_DOT_H_LESS_HEADERS
#include "TAU.h"


using namespace cali;

namespace
{

class TAUBinding : public cali::AnnotationBinding
{
    // when an attribute is created, store the value in TAU as metadata.
    // this seems reasonable, but might be misguided.
    static void create_attr_cb(Caliper* c, const Attribute& attr) {
        std::string tmp("Caliper: " + attr.name());
        Tau_metadata(tmp.c_str(), c->get(attr).value().to_string().c_str());
    }

public:

    void pre_initialize(Caliper* c) {
        // register for events of interest
        c->events().create_attr_evt.connect(&TAUBinding::create_attr_cb);
    }

    void initialize(Caliper* c) {
        // initialize TAU
        int argc = 1;
        const char *dummy = "Caliper Application";
        char* argv[1];
        argv[0] = const_cast<char*>(dummy);
        Tau_init(argc,argv);
        // want to get the *real* MPI rank.  How do I get it? like this.
        Attribute mpi_rank_attr = c->get_attribute("mpi.rank");
        if (mpi_rank_attr != Attribute::invalid) {
            // no MPI
            Tau_set_node(0);
        } else {
            // have MPI, get the rank
            Tau_set_node(c->get(mpi_rank_attr).value().to_int());
        }
    }

    void finalize(Caliper* c) {
        // do something?
    }

    const char* service_tag() const { return "tau"; };

    // handle a begin event by starting a TAU timer
    void on_begin(Caliper* c, const Attribute& attr, const Variant& value) {
        Tau_start((const char*)(value.to_string().data()));
    }

    // handle an end event by stopping a TAU timer
    void on_end(Caliper* c, const Attribute& attr, const Variant& value) {
        Tau_stop((const char*)(value.to_string().data()));
    }
};

} // namespace

namespace cali {
    CaliperService tau_service { "tau", &AnnotationBinding::make_binding<::TAUBinding> };
}

