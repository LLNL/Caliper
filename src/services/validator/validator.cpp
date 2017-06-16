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

/// @file  NVVP.cpp
/// @brief Caliper NVVP service

#include "../common/ToolWrapper.h"
#include "caliper/common/filters/RegexFilter.h"
#include <cstdio>
#include <vector>
namespace cali {


class Validator : public ToolWrapper {
  private:
     std::vector<std::string> regionTracker;
     bool isValidlyNested;
     FILE* logFile;
  public:
    void registerBadAnnotation(const Attribute& attr, const Variant& value, const std::string& error = "UNSPECIFIED"){
        fprintf(logFile,"%lu ; %s ; %s\n",attr.id(),value.to_string().c_str(),error.c_str()); 
    }
    virtual void initialize(){
        isValidlyNested=true;
        logFile = fopen("AnnotationLog.err","w");
    }

    virtual std::string service_name() { 
      return "Annotation Validator Service";
    }
    virtual std::string service_tag(){
      return "validator";
    }
    virtual void finalize(){
        fclose(logFile);
    }
    virtual void beginAction(Caliper* c, const Attribute &attr, const Variant& value){
      std::stringstream ss;
      ss << attr.name() << "=" << value.to_string();
      std::string name = ss.str();
      regionTracker.push_back(name);
    }

    virtual void endAction(Caliper* c, const Attribute& attr, const Variant& value){
      std::stringstream ss;
      ss << attr.name() << "=" << value.to_string();
      std::string name = ss.str();
      if(name!=regionTracker[regionTracker.size()-1]){
          std::cout<<"BAD REGION NESTING ON "<<name<<std::endl;
          isValidlyNested=false;
          registerBadAnnotation(attr, value, "BAD NESTING");
      }
    }
};

CaliperService validator_service { "validator", &setCallbacks<Validator>};

}

