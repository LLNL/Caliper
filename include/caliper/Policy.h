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

/// \file Policy.h
/// Caliper C++ Policy-based Annotations

/**
  * TODO: Namespace detail
  */

#ifndef CALI_POLICY_H
#define CALI_POLICY_H

#include "Annotation.h"
#include "AnnotationStub.h"
#include "cali_definitions.h"

#include "common/Variant.h"

#include <cstring>
#include <iostream>
#include <tuple>
#include <type_traits>

namespace cali{

	namespace policy {
	  namespace tags{
	    struct function{};
	    struct package{};
			struct description{};
	    struct loop{};
	  } // end namespace tags
	
	struct Nil{};

	template<typename... Types>
	struct TypeList{};
	template<>
	struct TypeList<>{
     using Cons = Nil;
	   static constexpr const int size = 0;
	};
	
	template<typename ConsType,typename... Types>
	struct TypeList<ConsType,Types...>
	{
	    using Cons = ConsType;
	    using More = TypeList<Types...>;
	    static constexpr const int size = sizeof...(Types)+1;
	};
	
	template<typename Term,typename tl>
	constexpr const typename std::enable_if<tl::size==0,bool>::type in_helper(){
	  return false;
	}
	    
	template<typename Term,typename tl>
	constexpr const typename std::enable_if<tl::size!=0,bool>::type in_helper(){
    return std::is_same<typename tl::Cons,Term>::value || policy::in_helper<Term,typename tl::More>();
	}
	
	template<typename Term,typename... Tags>
	constexpr const bool in(){
	  return in_helper<Term,TypeList<Tags...>>();
	}
	
	template<typename... Tags>
	constexpr const bool DefaultPolicy(){
	   return policy::in<policy::tags::function,Tags...>();
	}
	
	
	template<typename Head, typename... Included>
	struct Inclusive{
     static constexpr int tail_size = sizeof...(Included);

	   template<typename Shadow=Head,int ss=tail_size,typename... Tags>
	   static constexpr const typename std::enable_if<ss!=0,bool>::type policy_help(){
	      return policy::in<Shadow,Tags...>() || (Inclusive<Included...>::template policy<Tags...>());
	   }
	   template<typename Shadow=Head,int ss=tail_size, typename... Tags>
	   static constexpr const typename std::enable_if<ss==0,bool>::type policy_help(){
	      return policy::in<Shadow,Tags...>();
	   }
	   template<typename... Tags>
	   static constexpr const bool policy(){
	      return policy_help<Head, tail_size, Tags...>();
	   }

	
	};
	
	template<bool enable>
	struct AnnotationSelector{};
	
	template<>
	struct AnnotationSelector<false>{
	   using AnnotationType = cali::AnnotationStub;
	};
	
	template<>
	struct AnnotationSelector<true>{
	   using AnnotationType = cali::Annotation;
	};
	
  template<typename... Tags>
  static constexpr bool default_policy(){
    return Inclusive<cali::policy::tags::function, cali::policy::tags::loop>::policy<Tags...>();
  } 

	//template<typename... Tags>
	//using DefaultAnnotationTypeForTags = typename AnnotationSelector<
	//                          policy::default_policy<Tags...>()
	//                       >::AnnotationType;    

  //using DefaultFunctionAnnotation = DefaultAnnotationTypeForTags<cali::policy::tags::function>;
  //using DefaultLoopAnnotation     = DefaultAnnotationTypeForTags<cali::policy::tags::loop>;
  //using DefaultPackageAnnotation  = DefaultAnnotationTypeForTags<cali::policy::tags::package>;

} // end namespace policy  

} // end namespace cali

#endif //CALI_POLICY_H
