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

///@ file ContextBuffer.h
///@ ContextBuffer class declaration

#ifndef CALI_CONTEXTBUFFER_H
#define CALI_CONTEXTBUFFER_H

#include "Record.h"
#include "Variant.h"

#include <iostream>
#include <memory>

namespace cali
{

class Attribute;
class SnapshotRecord;
class Node;

class ContextBuffer
{
    struct ContextBufferImpl;

    std::unique_ptr<ContextBufferImpl> mP;

public:

    ContextBuffer();
    ~ContextBuffer();

    /// @name set / unset entries
    /// @{

    Variant  get(const Attribute&) const;
    Node*    get_node(const Attribute&) const;

    Variant  exchange(const Attribute&, const Variant&);

    cali_err set_node(const Attribute&, Node*);
    cali_err set(const Attribute&, const Variant&);
    cali_err unset(const Attribute&);

    /// @}
    /// @name get context
    /// @{

    void     snapshot(SnapshotRecord* sbuf) const;

    /// @}
    /// @name Serialization API
    /// @{

    void     push_record(WriteRecordFn fn) const;

    /// @}
    /// @name Statistics
    /// @{

    std::ostream& print_statistics(std::ostream& os) const;

    /// @}
};

} // namespace cali

#endif // CALI_CONTEXTBUFFER_H
