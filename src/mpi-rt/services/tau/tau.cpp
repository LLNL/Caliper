// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// Copyright (c) 2018, University of Oregon
// See top-level LICENSE file for details.

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

#include <mpi.h>

using namespace cali;

namespace
{

class TAUBinding : public cali::AnnotationBinding
{

public:

    void initialize(Caliper* c, Channel* chn) {
        // initialize TAU
        int argc = 1;
        const char *dummy = "Caliper Application";
        char* argv[1];
        argv[0] = const_cast<char*>(dummy);
        Tau_init(argc,argv);

        int flag = 0;        
        PMPI_Initialized(&flag);

        if (flag == 0) {
            Tau_set_node(0);
        } else {
            int rank = 0;
            PMPI_Comm_rank(MPI_COMM_WORLD, &rank);
            
            Tau_set_node(rank);
        }
        // register for events of interest?
    }

    void finalize(Caliper*, Channel*) {
        // do something?
    }

    const char* service_tag() const { return "tau"; };

    // handle a begin event by starting a TAU timer
    void on_begin(Caliper* c, Channel*, const Attribute& attr, const Variant& value) {
        if (attr.type() == CALI_TYPE_STRING) {
            Tau_start((const char*)value.data());
        } else {
            Tau_start((const char*)(value.to_string().data()));
        }
    }

    // handle an end event by stopping a TAU timer
    void on_end(Caliper* c, Channel*, const Attribute& attr, const Variant& value) {
        if (attr.type() == CALI_TYPE_STRING) {
            Tau_stop((const char*) value.data());
        } else {
            Tau_stop((const char*)(value.to_string().data()));
        }
    }
};

} // namespace

namespace cali
{

CaliperService tau_service { "tau", &AnnotationBinding::make_binding<::TAUBinding> };

}

