// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

/// \file CaliFunctional.h
/// Caliper C++ Functional Annotation Utilities

#ifndef CALI_FUNCTIONAL_H
#define CALI_FUNCTIONAL_H


#define GCC_VERSION (__GNUC__ * 10000 \
                     + __GNUC_MINOR__ * 100 \
                     + __GNUC_PATCHLEVEL__)
/* Test for GCC > 3.2.0 */


#define VARIADIC_RETURN_SAFE

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

} // end namespace cali
#endif //CALI_ANNOTATED_REGION_H
