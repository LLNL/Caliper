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
struct SafeAnnotation{
public:
    //ValidatedAnnotation() : inner_annot("ERROR"){}
    SafeAnnotation(const char* name, int opt=0) : inner_annot(name,opt){
        inner_annot = Annotation(name,opt);
    }
    SafeAnnotation(SafeAnnotation& other) : inner_annot(other.getAnnot()) {
    }
    SafeAnnotation(const SafeAnnotation& other) : inner_annot(other.getAnnot()) {
    }
    const Annotation& getAnnot() const {
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
    SafeAnnotation& set(unsigned long data){
      // Bad, but seemingly cali-necessary
        inner_annot.set((int)data);
        return *this;
    }
    void end(){
        inner_annot.end();
    }
    template<typename T>
    SafeAnnotation & set(T* start){
        // dumb
        std::ostringstream address;
        address << (void const *)start;
        std::string name = address.str();
        //auto foo = 
        inner_annot.set(  Variant(CALI_TYPE_STRING, name.c_str(), strlen(name.c_str())) ) ;
        return *this;
    }
    template<typename T>
    SafeAnnotation & set(const T* start){
        // dumb
        std::ostringstream address;
        address << (void const *)start;
        std::string name = address.str();
        auto foo = Variant(CALI_TYPE_STRING, name.c_str(), strlen(name.c_str()));
        inner_annot.set(foo);
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

template<typename InstanceType>
struct Recordable{
    static void record(InstanceType instance){};
};

template<typename... Args>
struct MapRecordingOperator{
    static auto record() -> void {
    }
};
template<typename MaybeRecordable>
void recordIfPossible(const MaybeRecordable instance){
    Recordable<MaybeRecordable>::record(instance);
}
template<typename MaybeRecordable>
void recordIfPossible(const MaybeRecordable* instance){
    Recordable<MaybeRecordable>::record(*instance);
}
cali::Annotation argument_number_annot("argument");
template <typename Arg, typename... Args>
struct MapRecordingOperator<Arg, Args...>{
    static auto record(Arg arg, Args... args) -> void {
        argument_number_annot.set(static_cast<int>(sizeof...(args)));
        recordIfPossible(arg);
        argument_number_annot.end(); // can be moved to callsite for speed, looks bad though
        MapRecordingOperator<Args...>::record(args...);
    }
};



template<typename... Args>
void recordAll(Args... args){
  MapRecordingOperator<Args...>::record(args...);
}

cali::Annotation recording_phase("recording_phase");
template<class LB>
struct RecordedFunction {
    RecordedFunction(const char* func_name, LB func) : body(func){
        name = func_name;
    }
    template <typename... Args>
    auto operator()(Args... args) -> typename std::enable_if<
        !std::is_same<typename std::result_of<LB(Args...)>::type, void>::value,
        typename std::result_of<LB(Args...)>::type>::type {
      recording_phase.begin("pre"); 
      recordAll(args...);
      recording_phase.end();
      cali::Annotation::Guard func_annot(
          cali::Annotation("debugged_function").begin(name)
      );
      auto return_value = body(args...);
      //cali::Annotation return_value_annot("return");
      //return_value_annot.set(return_value);
      //return_value_annot.end();
      recording_phase.begin("post");
      recordAll(args...);
      recording_phase.end();
      return return_value;
    }
    template <typename... Args>
    auto operator()(Args... args) -> typename std::enable_if<
        std::is_same<typename std::result_of<LB(Args...)>::type, void>::value,
        typename std::result_of<LB(Args...)>::type>::type {
      cali::Annotation::Guard func_annot(
          cali::Annotation("debugged_function").begin(name)
      );
      recording_phase.begin("pre"); 
      recordAll(args...);
      recording_phase.end();
      body(args...);
      recording_phase.begin("post"); 
      recordAll(args...);
      recording_phase.end(); 
    }
    LB body;
    const char* name;
};

template<typename LB>
RecordedFunction<LB> make_recorded_function(const char* name, LB body){
    return RecordedFunction<LB>(name,body);
}

namespace bluthund{
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
    TrackedExecutor(std::string func_name, LB func, std::vector<std::string> arg_names_in, std::string returned_name_in) : func_annot(func_name.c_str()), body(func), returned_annotation((func_name + "#"+returned_name_in).c_str(), CALI_ATTR_SKIP_EVENTS), rip_annot("rip", CALI_ATTR_SKIP_EVENTS){
        for(auto name : arg_names_in){
          argument_annotations.push_back(AnnotationType((func_name + "#"+ name).c_str(), CALI_ATTR_SKIP_EVENTS));
        }
    }
    template <typename... Args>
    auto untracked_call(Args... args) -> typename std::result_of<LB(Args...)>::type 
    {
      return body(args...);
    }
    template <typename... Args>
    auto operator()(Args... args) -> typename std::enable_if<
        !std::is_same<typename std::result_of<LB(Args...)>::type, void>::value,
        typename std::result_of<LB(Args...)>::type>::type {
      cali::Annotation::Guard func_annot_guard(func_annot.begin());
      Nase<Args...>::record(0,argument_annotations,args...);
      recorder_helper(rip_annot,rip_the_rip());
      auto return_value = body(args...);
      recorder_helper(returned_annotation, return_value);
      return return_value;
    }
    template <typename... Args>
    auto operator()(Args... args) -> typename std::enable_if<
        std::is_same<typename std::result_of<LB(Args...)>::type, void>::value,
        typename std::result_of<LB(Args...)>::type>::type {
      cali::Annotation::Guard func_annot_guard(func_annot.begin());
      Nase<Args...>::record(0,argument_annotations,args...);
      recorder_helper(rip_annot,rip_the_rip());
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

} // end namespace bluthund

} // end namespace cali
#endif //CALI_ANNOTATED_REGION_H
