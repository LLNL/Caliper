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
#include <TAU.h>

using namespace cali;

namespace
{

class TAUBinding : public cali::AnnotationBinding
{

public:

    const char* service_tag() const { return "tau"; };

/*
    void initialize(){
        TAU_PROFILE_SET_NODE(0);
    }

	void finalize(Caliper* c) {
    }
*/

    void on_begin(Caliper* c, const Attribute& attr, const Variant& value) {
        if (value.type() == CALI_TYPE_STRING) {
          TAU_PROFILE_START((const char*)(value.data()));
        }
    }

    void on_end(Caliper* c, const Attribute& attr, const Variant& value) {
        if (value.type() == CALI_TYPE_STRING) {
          TAU_PROFILE_STOP((const char*)(value.data()));
        }
    }
};

} // namespace

namespace cali {
    CaliperService tau_service { "tau", &AnnotationBinding::make_binding<::TAUBinding> };
}

