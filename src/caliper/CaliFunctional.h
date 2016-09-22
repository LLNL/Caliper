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
#include <type_traits>
#include <iostream>
#include <tuple>
namespace cali{
cali::Annotation& wrapper_annotation(){
        static cali::Annotation instance("wrapped_function");
        return instance;
}       

template<int N>
const char* annotation_name(){
    static std::string name = "function_argument_"+std::to_string(N); 
    return (name.c_str());
}
template<int N>
cali::Annotation& arg_annotation(){
        static cali::Annotation instance(annotation_name<N>());
        return instance;
}       

//Wrap a call to a function
template<typename LB, typename... Args>
auto wrap(const char* name, LB body, Args... args) -> typename std::result_of<LB(Args...)>::type{
    cali::Annotation::Guard func_annot(wrapper_annotation().begin(name));
    return body(args...);
}

template<int N,typename... Args>
auto record_args(const char* name, Args... args) -> std::tuple<> {
    return std::make_tuple();
}


template<int N,typename Arg, typename... Args>
auto record_args(const char* name, Arg arg, Args... args) -> decltype(std::tuple_cat(
                                                                                        record_args<N+1>(name,args...),
                                                                                        std::move(std::forward_as_tuple(std::move(cali::Annotation::Guard(wrapper_annotation()))))
                                                                                    )
                                                                     ){
    cali::Annotation::Guard x (arg_annotation<N>().begin(arg));
    return std::tuple_cat(record_args<N+1>(name,args...),std::forward_as_tuple(std::move(x)));
}

template<typename LB, typename... Args>
auto wrap_with_args(const char* name, LB body, Args... args) -> typename std::result_of<LB(Args...)>::type{
    #ifdef VARIADIC_RETURN_SAFE
    auto n =record_args<1>(name,args...);
    #else
    #warning CALIPER WARNING: This version of the C++ compiler has bugs which prevent argument profiling
    #endif
    return wrap(name,body, args...);
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

        cali::Annotation::Guard func_annot(wrapper_annotation().begin(name));
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
    auto operator()(Args... args) -> typename std::result_of<LB(Args...)>::type {
        cali::Annotation::Guard func_annot(wrapper_annotation().begin(name));
        #ifdef VARIADIC_RETURN_SAFE
        auto n =record_args<1>(name, args...);
        #else
        #warning CALIPER WARNING: This version of the C++ compiler has bugs which prevent argument profiling
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


