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

#include "common/Attribute.h"
#include "common/CaliperMetadataAccessInterface.h"
#include "common/Entry.h"
#include "common/IdType.h"
#include "common/Variant.h"
#include "common/util/callback.hpp"

#include <memory>
#include <utility>

namespace cali
{

// --- Forward declarations

class Caliper;
class CaliperService;
class Node;
class RuntimeConfig;
class SnapshotRecord;

// --- Typedefs

typedef std::function<bool(const SnapshotRecord*)> SnapshotFlushFn;

/// \brief Maintain a single data collection configuration with
///    callbacks and associated measurement data

class Experiment : public IdType
{
    struct ExperimentImpl;
    std::shared_ptr<ExperimentImpl> mP;

    Experiment(cali_id_t id, const char* name, const RuntimeConfig& cfg);

public:

    ~Experiment();    

    // --- Events (callback functions)
    
    struct Events {
        typedef util::callback<void(Caliper*,Experiment*,const Attribute&)>
            create_attr_cbvec;
        typedef util::callback<void(Caliper*,Experiment*,const std::string&,cali_attr_type,int*,Node**)>
            pre_create_attr_cbvec;                        
        typedef util::callback<void(Caliper*,Experiment*,const Attribute&,const Variant&)>
            update_cbvec;
        typedef util::callback<void(Caliper*,Experiment*)>
            caliper_cbvec;

        typedef util::callback<void(Caliper*,Experiment*,int,const SnapshotRecord*,SnapshotRecord*)>
            snapshot_cbvec;
        typedef util::callback<void(Caliper*,Experiment*,const SnapshotRecord*,const SnapshotRecord*)>
            process_snapshot_cbvec;
        typedef util::callback<void(Caliper*,Experiment*,SnapshotRecord*)>
            edit_snapshot_cbvec;

        typedef util::callback<void(Caliper*,Experiment*,const SnapshotRecord*,SnapshotFlushFn)>
            flush_cbvec;
        typedef util::callback<void(Caliper*,Experiment*,const SnapshotRecord*)>
            write_cbvec;

        typedef util::callback<void(Caliper*,Experiment*,const void*, const char*, size_t, size_t, const size_t*)>
            track_mem_cbvec;
        typedef util::callback<void(Caliper*,Experiment*,const void*)>
            untrack_mem_cbvec;
                                            
        pre_create_attr_cbvec  pre_create_attr_evt;
        create_attr_cbvec      create_attr_evt;

        update_cbvec           pre_begin_evt;
        update_cbvec           post_begin_evt;
        update_cbvec           pre_set_evt;
        update_cbvec           post_set_evt;
        update_cbvec           pre_end_evt;
        update_cbvec           post_end_evt;

        caliper_cbvec          create_thread_evt;
        caliper_cbvec          release_thread_evt;

        caliper_cbvec          post_init_evt;
        caliper_cbvec          finish_evt;

        snapshot_cbvec         snapshot;
        process_snapshot_cbvec process_snapshot;

        write_cbvec            pre_flush_evt;
        flush_cbvec            flush_evt;
        write_cbvec            post_flush_evt;

        edit_snapshot_cbvec    postprocess_snapshot;
        process_snapshot_cbvec write_snapshot;

        track_mem_cbvec        track_mem_evt;
        untrack_mem_cbvec      untrack_mem_evt;

        caliper_cbvec          clear_evt;
    };
    
    Events&        events();

    RuntimeConfig  config();

    // --- Experiment management
    
    std::string    name() const;

    bool           is_active() const;

    friend class Caliper;
};


/// \class Caliper
/// \brief The main interface for the caliper runtime system

class Caliper : public CaliperMetadataAccessInterface
{
    struct GlobalData;
    struct ThreadData;
    
    static              std::unique_ptr<GlobalData> sG;
    static thread_local std::unique_ptr<ThreadData> sT;
    
    bool m_is_signal; // are we in a signal handler?
    
    explicit Caliper(bool sig)
        : m_is_signal(sig)
        { }    

public:    

    //
    // --- Global Caliper API
    //
    
    /// \name Annotations (across experiments)
    /// \{

    void      begin(const Attribute& attr, const Variant& data);
    void      end(const Attribute& attr);
    void      set(const Attribute& attr, const Variant& data);

    /// \}
    /// \name Memory region tracking (across experiments)
    /// \{

    void      memory_region_begin(const void* ptr, const char* label, size_t elem_size, size_t ndim, const size_t dims[]);
    void      memory_region_end(const void* ptr);

    //
    // --- Per-experiment API
    //    

    /// \name Snapshot API
    /// \{

    void      push_snapshot(Experiment* exp, int scopes, const SnapshotRecord* trigger_info);
    void      pull_snapshot(Experiment* exp, int scopes, const SnapshotRecord* trigger_info, SnapshotRecord* snapshot);

    // --- Flush and I/O API

    /// \}
    /// \name Flush and I/O
    /// \{

    void      flush(Experiment* exp, const SnapshotRecord* flush_info, SnapshotFlushFn proc_fn);
    void      flush_and_write(Experiment* exp, const SnapshotRecord* flush_info);

    void      clear(Experiment* exp);

    // --- Annotation API
    
    /// \}
    /// \name Annotation API (single experiment)
    /// \{

    cali_err  begin(Experiment* exp, const Attribute& attr, const Variant& data);
    cali_err  end(Experiment* exp, const Attribute& attr);
    cali_err  set(Experiment* exp, const Attribute& attr, const Variant& data);
    cali_err  set_path(Experiment* exp, const Attribute& attr, size_t n, const Variant data[]);

    /// \}
    /// \name Memory region tracking (single experiment)
    /// \{

    void      memory_region_begin(Experiment* exp, const void* ptr, const char* label, size_t elem_size, size_t ndim, const size_t dims[]);
    void      memory_region_end(Experiment* exp, const void* ptr);

    /// \}
    /// \name Blackboard access
    /// \{

    Variant   exchange(Experiment* exp, const Attribute& attr, const Variant& data);
    Entry     get(Experiment* exp, const Attribute& attr);

    /// \}
    /// \name Metadata access (single experiment)
    /// \{

    std::vector<Entry> get_globals(Experiment* exp);

    /// \}

    //
    // --- Direct metadata / data access API
    //

    std::vector<Entry> get_globals();
    
    /// \}
    /// \name Explicit snapshot record manipulation
    /// \{

    void      make_entrylist(size_t n, 
                             const Attribute  attr[], 
                             const Variant    data[], 
                             SnapshotRecord&  list,
                             cali::Node*      parent = nullptr);
    void      make_entrylist(const Attribute& attr, 
                             size_t           n,
                             const Variant    data[], 
                             SnapshotRecord&  list);

    // --- Metadata Access Interface

    /// \}
    /// \name Context tree manipulation and metadata access
    /// \{

    size_t    num_attributes() const;

    Attribute get_attribute(cali_id_t id) const;
    Attribute get_attribute(const std::string& name) const;

    std::vector<Attribute> get_attributes() const;

    Attribute create_attribute(const std::string& name,
                               cali_attr_type     type,
                               int                prop = CALI_ATTR_DEFAULT,
                               int                meta = 0,
                               const Attribute*   meta_attr = nullptr,
                               const Variant*     meta_data = nullptr);
    
    /// \brief Return node by id
    Node*     node(cali_id_t id) const;

    /// \brief Get or create tree path with data from given nodes in given order 
    Node*     make_tree_entry(size_t n, const Node* nodelist[], Node* parent = nullptr);

    /// \brief Get or create tree entry with given attribute/value pair
    Node*     make_tree_entry(const Attribute& attr, const Variant& value, Node* parent = nullptr);

    /// \}
    /// \name Experiment API
    /// \{

    Experiment* create_experiment(const char* name, const RuntimeConfig& cfg);

    std::vector<Experiment*> get_experiments();

    Experiment* get_experiment(cali_id_t id);
    // Experiment* get_experiment(const char* name);

    /// \}

    // --- Caliper API access
    
    Caliper();
    
    ~Caliper()
        { }

    Caliper(const Caliper&) = default;

    Caliper& operator = (const Caliper&) = default;

    bool is_signal() const { return m_is_signal; };

    operator bool () const;

    static Caliper instance();
    
    static void    release();
    
    static Caliper sigsafe_instance();

    static bool    is_initialized();

    /// \brief Add a list of available caliper services.
    static void    add_services(const CaliperService*);

    /// \brief Add a function that is called during %Caliper initialization.
    static void    add_init_hook(void(*fn)());
    
    friend struct GlobalData;
};

} // namespace cali
