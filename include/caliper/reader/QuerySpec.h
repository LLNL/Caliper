// Copyright (c) 2017, Lawrence Livermore National Security, LLC.  
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

/// \file QuerySpec.h
/// \brief %Caliper query specification definition

#pragma once

#include <string>
#include <vector>

namespace cali
{

/// \brief Describes a %Caliper data processing rule

struct QuerySpec
{
    //
    // --- Types
    //
    
    /// \brief Template type for list-style query options
    template<class T>
    struct SelectionList {
        enum SelectionOpt {
            Default, ///< Take the default 
            None,    ///< Take none
            All,     ///< Take all available 
            List     ///< User-defined list
        }              selection; ///< Selection specification

        /// \brief User-defined list 
        std::vector<T> list;
    };

    /// \brief Describe function signatures in query specs
    struct FunctionSignature {
        int          id;
        const char*  name;     ///< Function name
        int          min_args; ///< Minimum required number of arguments
        int          max_args; ///< Maximum allowed number of function arguments
        const char** args;     ///< Names of the function arguments
    };

    static constexpr FunctionSignature FunctionSignatureTerminator = { -1, nullptr, -1, 0, nullptr };
    
    /// \brief An aggregation function invocation in a query spec
    struct AggregationOp {
        FunctionSignature        op;    ///< The aggregation operation
        std::vector<std::string> args;  ///< Arguments
        std::string              alias; ///< Output name

        AggregationOp()
            : op(FunctionSignatureTerminator)
        { }

        AggregationOp(const FunctionSignature& s)
            : op(s)
        { }
    };

    /// \brief Sort description
    struct SortSpec {
        enum Order {
            Ascending, Descending
        }           order;
        std::string attribute;
    };

    /// \brief Filter condition
    struct Condition {
        enum Op   {
            Exist, NotExist, Equal, UnEqual, LessThan, GreaterThan, LessOrEqual, GreaterOrEqual
        }           op;
        std::string attribute;
        std::string value;
    };

    struct FormatSpec {
        enum Opt {
            Default, User
        }                        opt;        
        FunctionSignature        formatter;
        std::vector<std::string> args;
    };
    
    //
    // --- Data members
    //

    SelectionList<AggregationOp> aggregation_ops;
    SelectionList<std::string>   aggregation_key;

    SelectionList<std::string>   attribute_selection;

    SelectionList<Condition>     snapshot_filter;
    SelectionList<SortSpec>      sort;

    FormatSpec                   format;
};

} // namespace cali
