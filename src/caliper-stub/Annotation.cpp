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

/// \file Annotation.cpp
/// Annotation stub interface

#include "caliper/Annotation.h"


using namespace cali;

struct Loop::Impl { };

Loop::Iteration::Iteration(const Impl*, int)
    : pI(0)
{ }

Loop::Iteration::~Iteration()
{ }

Loop::Loop(const char*)
    : pI(0)
{ }

Loop::Loop(const Loop&)
    : pI(0)
{ }

Loop::~Loop()
{ }

Loop::Iteration Loop::iteration(int i) const
{
    return Loop::Iteration(nullptr, i);
} 

void Loop::end()
{}

struct Annotation::Impl { };

// --- Guard subclass

Annotation::Guard::Guard(Annotation&)
{ }

Annotation::Guard::~Guard()
{
}


// --- Constructors / destructor

/// Construct an annotation object for context attribute \c name. 
/// 

Annotation::Annotation(const char*, int)
    : pI(0)
{ }

Annotation::Annotation(const Annotation&)
    : pI(0)
{ }

Annotation::Annotation(const char*, const MetadataListType&, int)
    : pI(0)
{ }

Annotation::~Annotation()
{
}

Annotation& Annotation::operator = (const Annotation&)
{
    return *this;
}

// --- begin() overloads

Annotation& Annotation::begin()
{
    return *this;
}

Annotation& Annotation::begin(int data)
{
    return *this;
}

Annotation& Annotation::begin(double data)
{
    return *this;
}

Annotation& Annotation::begin(const char* data)
{
    return *this;
}

Annotation& Annotation::begin(cali_attr_type type, void* data, uint64_t size)
{
    return *this;
}

Annotation& Annotation::begin(const Variant& data)
{
    return *this;
}

// --- set() overloads

Annotation& Annotation::set(int data)
{
    return *this;
}

Annotation& Annotation::set(double data)
{
    return *this;
}

Annotation& Annotation::set(const char* data)
{
    return *this;
}

Annotation& Annotation::set(cali_attr_type type, void* data, uint64_t size)
{
    return *this;
}

Annotation& Annotation::set(const Variant& data)
{
    return *this;
}

void Annotation::end()
{
}
