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

/// \file CaliFunctional.h
/// Caliper C++ Functional Annotation Utilities

#ifndef CALI_FUNCTIONAL_H
#define CALI_FUNCTIONAL_H

#include "Annotation.h"
#include "cali_definitions.h"

#include "common/Variant.h"

#include <cstring>
#include <iostream>
#include <sstream>
#include <vector>
#include <tuple>
#include <type_traits>

namespace cali{

namespace functional {
using AnnotationType = cali::Annotation;
using AnnotationVector = std::vector<AnnotationType>;

template<typename... Args>
struct Nase{
    static auto record(int arg_num, AnnotationVector annotations) -> void {
    }
};
template<typename T>
struct recordValue;
template<typename T>
struct recordValue{
static void record(AnnotationType& annot, T arg){
annot.set(arg);
}
};
template<>
struct recordValue<unsigned long>{
static void record(AnnotationType& annot, unsigned long in){
annot.set((int)in);
}
};
template<typename T>
struct recordValue<T*>{
static void record(AnnotationType& annot, T* in){
  annot.set((int)(unsigned long)in);
}
};
template<typename T>
struct recordValue<const T*>{
static void record(AnnotationType& annot, const T* in){
  annot.set((int)(unsigned long)in);
}
};
template<typename T>
void recorder_helper(AnnotationType& annot, T in){
  recordValue<T>::record(annot,in);
}
template <typename Arg, typename... Args>
struct Nase<Arg, Args...>{
    using NextRecorder = Nase<Args...>;
    
    static auto record(int arg_num, AnnotationVector annotations, Arg arg, Args... args) -> void {
       //annotations[arg_num].set(arg);
       recorder_helper(annotations[arg_num],arg);
       NextRecorder::record(arg_num + 1, annotations, args...);
    }
};

void* rip_the_rip(){
  return __builtin_return_address(0);
}

template<class LB>
struct TrackedExecutor {
    TrackedExecutor(std::string func_name, LB func, std::vector<std::string> arg_names_in, std::string returned_name_in = "return_value") : func_annot(func_name.c_str()), body(func), returned_annotation((func_name + "#"+returned_name_in).c_str(), CALI_ATTR_SKIP_EVENTS), rip_annot("rip", CALI_ATTR_SKIP_EVENTS){
        for(auto name : arg_names_in){
          argument_annotations.push_back(AnnotationType((func_name + "#"+ name).c_str(), CALI_ATTR_SKIP_EVENTS));
        }
    }
    template <typename... Args>
    auto untracked_call(Args... args) -> typename std::result_of<LB(Args...)>::type 
    {
      return body(args...);
    }
    void close(){
      for(auto annot: argument_annotations){
        annot.end();
      }
      returned_annotation.end();
      rip_annot.end();
    }
    template <typename... Args>
    auto operator()(Args... args) -> typename std::enable_if<
        !std::is_same<typename std::result_of<LB(Args...)>::type, void>::value,
        typename std::result_of<LB(Args...)>::type>::type {
      func_annot.begin();
      Nase<Args...>::record(0,argument_annotations,args...);
      recorder_helper(rip_annot,rip_the_rip());
      auto return_value = body(args...);
      recorder_helper(returned_annotation, return_value);
      func_annot.end();
      close();
      
      return return_value;
    }
    template <typename... Args>
    auto operator()(Args... args) -> typename std::enable_if<
        std::is_same<typename std::result_of<LB(Args...)>::type, void>::value,
        typename std::result_of<LB(Args...)>::type>::type {
      func_annot.begin();
      Nase<Args...>::record(0,argument_annotations,args...);
      recorder_helper(rip_annot,rip_the_rip());
      func_annot.end();
      close();

      return body(args...);
    }
    LB body;
    AnnotationType func_annot;
    AnnotationVector argument_annotations;
    AnnotationType returned_annotation;
    AnnotationType rip_annot;
};

template<typename Callable>
TrackedExecutor<Callable> wrap(std::string name, Callable callee, std::vector<std::string> arg_names, std::string returned_name){
  return TrackedExecutor<Callable>(name, callee, arg_names, returned_name);
}

} // end namespace functional

} // end namespace cali
#endif //CALI_ANNOTATED_REGION_H
