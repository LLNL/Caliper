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

/// \file Caliper.h 
/// Initialization function and global data declaration

#pragma once

#include "cali_definitions.h"

#include "Attribute.h"
#include "Entry.h"
#include "Record.h"
#include "Variant.h"
#include "util/callback.hpp"

#include <functional>
#include <utility>


namespace cali
{

// Forward declarations

class Node;    

template<int> class FixedSnapshot;
typedef FixedSnapshot<64> Snapshot;

/// @class Caliper

class Caliper 
{
public:

    struct Scope;

    typedef Scope* (*ScopeCallbackFn)(void);

    
private:
    
    struct GlobalData;
    
    GlobalData* mG;
    
    Scope* m_thread_scope;
    Scope* m_task_scope;

    
    Caliper(GlobalData* g, Scope* thread = 0, Scope* task = 0)
        : mG(g), m_thread_scope(thread), m_task_scope(task)
        { }

    Scope* scope(cali_context_scope_t scope);
    

public:
    
    Caliper();
    
    ~Caliper()
        { }

    Caliper(const Caliper&) = default;

    Caliper& operator = (const Caliper&) = default;
    
    // --- Events

    struct Events {
        util::callback<void(Caliper*, const Attribute&)> create_attr_evt;

        util::callback<void(Caliper*, const Attribute&)> pre_begin_evt;
        util::callback<void(Caliper*, const Attribute&)> post_begin_evt;
        util::callback<void(Caliper*, const Attribute&)> pre_end_evt;
        util::callback<void(Caliper*, const Attribute&)> post_end_evt;
        util::callback<void(Caliper*, const Attribute&)> pre_set_evt;
        util::callback<void(Caliper*, const Attribute&)> post_set_evt;

        util::callback<void(Caliper*,
                            cali_context_scope_t)>       create_scope_evt;
        util::callback<void(Caliper*,
                            cali_context_scope_t)>       release_scope_evt;

        util::callback<void(Caliper*)>                   post_init_evt;
        util::callback<void(Caliper*)>                   finish_evt;

        util::callback<void(Caliper*, 
                            int, 
                            const Entry*,
                            Snapshot*)>                  snapshot;
        util::callback<void(Caliper*,
                            const Entry*,
                            const Snapshot*)>            process_snapshot;

        util::callback<void(const RecordDescriptor&,
                            const int*,
                            const Variant**)>            write_record;
    };

    Events&   events();

    // --- Context environment API

    Scope*    create_scope(cali_context_scope_t context);
    Scope*    default_scope(cali_context_scope_t context);

    void      release_scope(Scope*);

    void      set_scope_callback(cali_context_scope_t context, ScopeCallbackFn cb);

    // --- Snapshot API

    void      push_snapshot(int scopes, const Entry* trigger_info);
    void      pull_snapshot(int scopes, const Entry* trigger_info, Snapshot* snapshot);

    // --- Annotation API

    cali_err  begin(const Attribute& attr, const Variant& data);
    cali_err  end(const Attribute& attr);
    cali_err  set(const Attribute& attr, const Variant& data);
    cali_err  set_path(const Attribute& attr, size_t n, const Variant data[]);

    Variant   exchange(const Attribute& attr, const Variant& data);

    // --- Direct metadata API

    Entry     make_entry(size_t n, const Attribute* attr, const Variant* value);
    Entry     make_entry(const Attribute& attr, const Variant& value);

    // --- Query API

    Entry     get(const Attribute& attr);

    // --- Attribute API

    size_t    num_attributes() const;

    Attribute get_attribute(cali_id_t id) const;
    Attribute get_attribute(const std::string& name) const;

    Attribute create_attribute(const std::string& name, cali_attr_type type, int prop = CALI_ATTR_DEFAULT);


    // --- Caliper API access

    operator bool () const {
        return mG != 0;
    }

    static Caliper instance();
    static void    release();
    
    // static Caliper try_instance();
};

} // namespace cali

