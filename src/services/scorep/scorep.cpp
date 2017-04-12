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

/// @file  scorep.cpp
/// @brief Caliper Score-P service

#include "../common/ToolWrapper.h"
#include "./common/filters/RegexFilter.h"


#include <map>
#define SCOREP_USER_ENABLE
#include <scorep/SCOREP_User.h>
#include <scorep/SCOREP_PublicTypes.h>

#include <stddef.h>

extern const struct SCOREP_Subsystem SCOREP_Subsystem_Substrates;
extern const struct SCOREP_Subsystem SCOREP_Subsystem_TaskStack;
extern const struct SCOREP_Subsystem SCOREP_Subsystem_MetricService;
extern const struct SCOREP_Subsystem SCOREP_Subsystem_UnwindingService;
extern const struct SCOREP_Subsystem SCOREP_Subsystem_SamplingService;
extern const struct SCOREP_Subsystem SCOREP_Subsystem_CompilerAdapter;
extern const struct SCOREP_Subsystem SCOREP_Subsystem_UserAdapter;

const struct SCOREP_Subsystem* scorep_subsystems[] = {
    &SCOREP_Subsystem_Substrates,
    &SCOREP_Subsystem_TaskStack,
    &SCOREP_Subsystem_MetricService,
    &SCOREP_Subsystem_UnwindingService,
    &SCOREP_Subsystem_SamplingService,
    &SCOREP_Subsystem_CompilerAdapter,
    &SCOREP_Subsystem_UserAdapter
};

extern const size_t scorep_number_of_subsystems = sizeof( scorep_subsystems ) /
                                           sizeof( scorep_subsystems[ 0 ] );

using ScorePHandleType = SCOREP_User_RegionHandle;


namespace cali {

//ScorePHandleType caliper_scorep_handle;
SCOREP_USER_GLOBAL_REGION_DEFINE( caliper_scorep_handle )

class ScorePWrapper : public ToolWrapper {
  public:
    virtual void initialize(){
         SCOREP_RECORDING_ON();
         SCOREP_USER_REGION_INIT( caliper_scorep_handle, "Caliper controlled region", SCOREP_USER_REGION_TYPE_PHASE )
         //SCOREP_USER_GLOBAL_REGION_DEFINE( caliper_scorep_handle_intermediate )
         //
         //caliper_scorep_handle = caliper_scorep_handle_intermediate;
    }

    virtual std::string service_name() { 
      return "Score-P service";
    }
    virtual std::string service_tag(){
      return "scorep";
    }
    virtual void beginAction(Caliper* c, const Attribute &attr, const Variant& value){
      std::stringstream ss;

      ss << attr.name() << "=" << value.to_string();
      std::string name = ss.str();

      SCOREP_USER_REGION_BEGIN(caliper_scorep_handle,name.c_str(),SCOREP_USER_REGION_TYPE_PHASE);
    }

    virtual void endAction(Caliper* c, const Attribute& attr, const Variant& value){
      std::stringstream ss;
      ss << attr.name() << "=" << value.to_string();
      std::string name = ss.str();
      SCOREP_USER_REGION_END(caliper_scorep_handle);
    }
};

CaliperService scorep_service { "scorep", &setCallbacks<ScorePWrapper>};


}

