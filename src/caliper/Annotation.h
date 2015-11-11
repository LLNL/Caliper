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

/// \file Annotation.h
/// Caliper C++ annotation interface

//
// --- NOTE: This interface must be C++98 only!
//

#ifndef CALI_ANNOTATION_H
#define CALI_ANNOTATION_H

#include "cali_types.h"

namespace cali
{

class Variant;

/// \brief Instrumentation interface to add and manipulate context attributes

class Annotation 
{
    struct Impl;
    Impl*  pI;

    Annotation(const Annotation&);
    Annotation& operator = (const Annotation&);

public:

    /// \brief Constructor. Creates an annotation object to manipulate 
    ///    the context attribute \c name. 

    Annotation(const char* name, int opt = 0);

    ~Annotation();


    /// \brief Scope guard to automatically \c end() an annotation at the end of
    ///   the C++ scope.

    class Guard {
        Impl* pI;

        Guard(const Guard&);
        Guard& operator = (const Guard&);

    public:

        Guard(Annotation& a);

        ~Guard();
    };

    // Keep AutoScope name for backward compatibility
    typedef Guard AutoScope;

    /// \name \c begin() overloads
    /// \{

    Annotation& begin(int data);
    Annotation& begin(double data);
    Annotation& begin(const char* data);
    Annotation& begin(cali_attr_type type, void* data, uint64_t size);
    Annotation& begin(const Variant& data);

    /// \}
    /// \name \c set() overloads
    /// \{

    Annotation& set(int data);
    Annotation& set(double data);
    Annotation& set(const char* data);
    Annotation& set(cali_attr_type type, void* data, uint64_t size);
    Annotation& set(const Variant& data);

    /// \}
    /// \name \c end()
    /// \{

    void end();

    /// \}
};

} // namespace cali

#endif
