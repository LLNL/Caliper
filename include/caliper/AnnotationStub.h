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

/// \file AnnotationStub.h
/// Caliper C++ annotation stub interface

//
// --- NOTE: This interface must be C++98 only!
//

#ifndef CALI_ANNOTATION_STUB_H
#define CALI_ANNOTATION_STUB_H

#include "common/cali_types.h"

namespace cali
{
    
class Variant;

/// \brief Instrumentation interface to add and manipulate context attributes
///
/// The Annotation class is the primary source-code instrumentation interface
/// for %Caliper. %Annotation objects provide access to named %Caliper context 
/// attributes. If the referenced attribute does not exist yet, it will be 
/// created automatically.
///
/// Example:
/// \code
/// cali::Annotation phase_ann("myprogram.phase");
///
/// phase_ann.begin("Initialization");
///   // ...
/// phase_ann.end();
/// \endcode
/// This example creates an annotation object for the \c myprogram.phase 
/// attribute, and uses the \c begin()/end() methods to mark a section 
/// of code where that attribute is set to "Initialization".
///
/// \note Access to the underlying named context attribute through
/// %Annotation objects is not exclusive: multiple %Annotation objects
/// can reference and update the same context attribute.

class AnnotationStub 
{


public:

    /// \brief Creates an annotation object to manipulate 
    ///   the context attribute with the given \a name. 
    /// 
    /// \param name The attribute name
    /// \param opt  %Attribute flags. Bitwise OR combination 
    ///   of \ref cali_attr_properties values.
    AnnotationStub(const char* name, int opt = 0);

    AnnotationStub(const AnnotationStub&);

    ~AnnotationStub();

    AnnotationStub& operator = (const AnnotationStub&);

    class Guard {
        Guard(const Guard&);
        Guard& operator = (const Guard&);

    public:

        Guard(AnnotationStub& a);

        ~Guard();
    };

    // Keep AutoScope name for backward compatibility
    typedef Guard AutoScope;

    /// \name begin() overloads
    /// \{

    AnnotationStub& begin();

    /// \brief Begin <em>name</em>=<em>data</em> region for the associated 
    ///    context attribute.
    ///
    /// Marks begin of the <em>name</em>=<em>data</em> region, where
    /// \a name is the attribute name given in
    /// cali::AnnotationStub::AnnotationStub(). The new value will be nested
    /// under already open regions for the \a name context attribute.
    AnnotationStub& begin(int data);
    /// \copydoc cali::AnnotationStub::begin(int)
    AnnotationStub& begin(double data);
    /// \copydoc cali::AnnotationStub::begin(int)
    AnnotationStub& begin(const char* data);
    AnnotationStub& begin(cali_attr_type type, void* data, uint64_t size);
    /// \copydoc cali::AnnotationStub::begin(int)
    AnnotationStub& begin(const Variant& data);

    /// \}
    /// \name set() overloads
    /// \{

    /// \brief Set <em>name</em>=<em>data</em> for the associated 
    ///    context attribute.
    ///
    /// Exports <em>name</em>=<em>data</em>, where \a name is the
    /// attribute name given in cali::AnnotationStub::AnnotationStub(). The
    /// top-most prior open value for the \a name context attribute,
    /// if any, will be overwritten.
    AnnotationStub& set(int data);
    /// \copydoc cali::AnnotationStub::set(int)
    AnnotationStub& set(double data);
    /// \copydoc cali::AnnotationStub::set(int)
    AnnotationStub& set(const char* data);
    AnnotationStub& set(cali_attr_type type, void* data, uint64_t size);
    /// \copydoc cali::AnnotationStub::set(int)
    AnnotationStub& set(const Variant& data);

    /// \}

    /// \brief Close top-most open region for the associated 
    ///   context attribute.
    void end();
};

/// \} // AnnotationAPI group
    
} // namespace cali

#endif
