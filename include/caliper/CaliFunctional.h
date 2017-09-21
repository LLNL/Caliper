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
#include <tuple>
#include <type_traits>

namespace cali{
struct SafeAnnotation{
public:
    //ValidatedAnnotation() : inner_annot("ERROR"){}
    SafeAnnotation(const char* name, int opt=0) : inner_annot(name,opt){
        inner_annot = Annotation(name,opt);
    }
    SafeAnnotation(SafeAnnotation& other) : inner_annot(other.getAnnot()) {
        
    }
    Annotation& getAnnot() {
        return inner_annot;
    }
    SafeAnnotation& operator = (SafeAnnotation& other){
        inner_annot = other.getAnnot();
        return *this;
    }
    SafeAnnotation& begin(){
        inner_annot.begin();
        return *this;
    }
    SafeAnnotation& begin(int data){
        inner_annot.begin(data);
        return *this;
    }
    SafeAnnotation& begin(double data){
        inner_annot.begin(data);
        return *this;
    }
    SafeAnnotation& begin(const char* data){
        inner_annot.begin(Variant(CALI_TYPE_STRING, data, strlen(data)));
        return *this;
    }
    SafeAnnotation& begin(cali_attr_type type, void* data, uint64_t size){
        inner_annot.begin(type,data,size);
        return *this;
    }
    SafeAnnotation& begin(const Variant& data){
        inner_annot.begin(data);
        return *this;
    }
    //set
    SafeAnnotation& set(int data){
        inner_annot.set(data);
        return *this;
    }
    SafeAnnotation& set(double data){
        inner_annot.set(data);
        return *this;
    }
    SafeAnnotation& set(const char* data){
        inner_annot.set(Variant(CALI_TYPE_STRING, data, strlen(data)));
        return *this;
    }
    SafeAnnotation& set(cali_attr_type type, void* data, uint64_t size){
        inner_annot.set(type,data,size);
        return *this;
    }
    SafeAnnotation& set(const Variant& data){
        inner_annot.set(data);
        return *this;
    }
    void end(){
        inner_annot.end();
    }
    template<typename T>
    SafeAnnotation & begin(T start){
        inner_annot.begin("Unmeasurable");
        return *this;
    }
    template<typename T>
    SafeAnnotation & set(T start){
        inner_annot.set("Unmeasurable");
        return *this;
    }
    Annotation inner_annot;
};

cali::SafeAnnotation& wrapper_annotation(){
        static cali::SafeAnnotation instance("wrapped_function");
        return instance;
}       

template<int N>
const char* annotation_name(){
    static std::string name = "function_argument_"+std::to_string(N); 
    return (name.c_str());
}
template<int N>
cali::SafeAnnotation& arg_annotation(){
        static cali::SafeAnnotation instance(annotation_name<N>());
        return instance;
}       
template<int N>
cali::Annotation& arg_annotation_raw(){
        static cali::Annotation instance(annotation_name<N>());
        return instance;
}       


//Wrap a call to a function
template<typename LB, typename... Args>
auto wrap(const char* name, LB body, Args... args) -> typename std::result_of<LB(Args...)>::type{
    Annotation startedAnnot = wrapper_annotation().begin(name).getAnnot();
    cali::Annotation::Guard func_annot(startedAnnot);
    return body(args...);
}

cali::Annotation& dummy_annot(){
    static cali::Annotation instance ("wrapped func");
    return instance;
}

template<int N, typename... Args>
struct ArgRecorder{
    static auto record(const char* name) -> std::tuple<> {
        return std::make_tuple();
    }
};

template <int N, typename Arg, typename... Args>
struct ArgRecorder<N, Arg, Args...>{
    using NextRecorder = ArgRecorder<N+1,Args...>;
    static auto record(const char* name, Arg arg, Args... args) -> decltype(std::tuple_cat(
                                                                                        NextRecorder::record(name,args...),
                                                                                        std::move(std::forward_as_tuple(std::move(cali::Annotation::Guard(dummy_annot())  )))
                                                                                    )
                                                                     ){
        cali::Annotation::Guard func_annot (arg_annotation<N>().begin(arg).getAnnot());
        return std::tuple_cat(
            NextRecorder::record(name,args...),
            std::forward_as_tuple(std::move(func_annot))
        );
    }
};

//TODO: fix
template<typename LB, typename... Args>
auto wrap_with_args(const char* name, LB body, Args... args) -> typename std::result_of<LB(Args...)>::type{
    Annotation startedAnnot = wrapper_annotation().begin(name).getAnnot();
    cali::Annotation::Guard func_annot(startedAnnot);
    #ifdef VARIADIC_RETURN_SAFE
    auto n =ArgRecorder<1,Args...>::record(name,args...);
    #else
    #warning CALIPER WARNING: This  C++ compiler has bugs which prevent argument profiling
    #endif
    return body(args...);
}

//Functor containing a function which should always be wrapped. Should not be instantiated directly, 
//but through calls to wrap_function below
template<class LB>
struct WrappedFunction {
    WrappedFunction(const char* func_name, LB func) : body(func){
        name = func_name;
    }
    template <typename... Args>
    auto operator()(Args... args) -> typename std::result_of<LB(Args...)>::type {

        cali::Annotation::Guard func_annot(wrapper_annotation().begin(name).getAnnot());
        return body(args...);
    }
    LB body;
    const char* name;
};

template<class LB>
struct ArgWrappedFunction {
    ArgWrappedFunction(const char* func_name, LB func) : body(func){
        name = func_name;
    }
    template <typename... Args>
    auto operator()(Args... args) -> typename std::enable_if<
        !std::is_same<typename std::result_of<LB(Args...)>::type, void>::value,
        typename std::result_of<LB(Args...)>::type>::type {
      cali::Annotation::Guard func_annot(
          wrapper_annotation().begin(name).getAnnot());
      #ifdef VARIADIC_RETURN_SAFE
      auto n =ArgRecorder<1,Args...>::record(name,args...);
      #else
      #warning CALIPER WARNING: This  C++ compiler has bugs which prevent argument profiling
      #endif
      auto return_value = body(args...);
      cali::Annotation return_value_annot("return");
      return_value_annot.set(return_value);
      return_value_annot.end();
      return return_value;
    }
    template <typename... Args>
    auto operator()(Args... args) -> typename std::enable_if<
        std::is_same<typename std::result_of<LB(Args...)>::type, void>::value,
        typename std::result_of<LB(Args...)>::type>::type {
      cali::Annotation::Guard func_annot(
          wrapper_annotation().begin(name).getAnnot());
      #ifdef VARIADIC_RETURN_SAFE
      auto n =ArgRecorder<1,Args...>::record(name,args...);
      #else
      #warning CALIPER WARNING: This  C++ compiler has bugs which prevent argument profiling
      #endif
      return body(args...);
    }
    LB body;
    const char* name;
};

//Helper factory function to create WrappedFunction objects
template<typename LB>
WrappedFunction<LB> wrap_function(const char* name, LB body){
    return WrappedFunction<LB>(name,body);
}

template<typename LB>
ArgWrappedFunction<LB> wrap_function_and_args(const char* name, LB body){
    return ArgWrappedFunction<LB>(name,body);
}

}

#endif //CALI_ANNOTATED_REGION_H
