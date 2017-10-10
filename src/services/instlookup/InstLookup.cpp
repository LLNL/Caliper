// Copyright (c) 2017, Lawrence Livermore National Security, LLC.  
// Produced at the Lawrence Livermore National Laboratory.
//
// This file is part of Caliper.
// Written by Alfredo Gimenez, gimenez1@llnl.gov.
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

/// \file InstLookup.cpp
/// \brief Caliper instruction lookup service

#include "../CaliperService.h"

#include "caliper/Caliper.h"
#include "../../caliper/MemoryPool.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/common/Log.h"
#include "caliper/common/Node.h"
#include "caliper/common/RuntimeConfig.h"

#include "caliper/common/util/split.hpp"

#include <Symtab.h>
#include <LineInformation.h>
#include <CodeObject.h> // parseAPI
#include <InstructionDecoder.h> // instructionAPI
#include <Function.h>
#include <AddrLookup.h>

#include <algorithm>
#include <iterator>
#include <map>
#include <memory>
#include <mutex>

using namespace cali;

using namespace Dyninst;
using namespace SymtabAPI;
using namespace InstructionAPI;

namespace
{

class InstLookup
{
    static std::unique_ptr<InstLookup> s_instance;
    static const ConfigSet::Entry        s_configdata[];

    struct InstAttributes {
        Attribute type_attr;
    };

    ConfigSet m_config;

    bool m_instruction_type;

    std::map<Attribute, InstAttributes> m_sym_attr_map;
    std::mutex m_sym_attr_mutex;

    std::vector<std::string> m_addr_attr_names;

    AddressLookup* m_lookup;
    std::mutex     m_lookup_mutex;

    unsigned m_num_lookups;
    unsigned m_num_failed;

    //
    // --- methods
    //

    void make_inst_attributes(Caliper* c, const Attribute& attr) {
        struct InstAttributes sym_attribs;

        sym_attribs.type_attr = 
            c->create_attribute("instruction.type#" + attr.name(), 
                                CALI_TYPE_STRING, CALI_ATTR_DEFAULT);

        std::lock_guard<std::mutex>
            g(m_sym_attr_mutex);

        m_sym_attr_map.insert(std::make_pair(attr, sym_attribs));
    }

    void check_attributes(Caliper* c) {
        std::vector<Attribute> vec;

        if (m_addr_attr_names.empty()) {
            vec = c->find_attributes_with(c->get_attribute("class.symboladdress"));
        } else {
            for (const std::string& s : m_addr_attr_names) {
                Attribute attr = c->get_attribute(s);

                if (attr != Attribute::invalid)
                    vec.push_back(attr);
                else 
                    Log(0).stream() << "Instlookup: Address attribute \""
                                    << s << "\" not found!" << std::endl;
            }
        }

        if (vec.empty())
            Log(1).stream() << "Instlookup: No address attributes found." 
                            << std::endl;

        for (const Attribute& a : vec)
            make_inst_attributes(c, a);
    }
    
    void add_inst_attributes(const Entry& e, 
                               const InstAttributes& sym_attr,
                               MemoryPool& mempool,
                               std::vector<Attribute>& attr, 
                               std::vector<Variant>&   data) {

        uint64_t address  = e.value().to_uint();

        // TODO: get instruction semantics and add to `attr` and `data`
    }

    void process_snapshot(Caliper* c, SnapshotRecord* snapshot) {
        if (m_sym_attr_map.empty())
            return;

        // make local inst attribute map copy for threadsafe access

        std::map<Attribute, InstAttributes> sym_map;

        {
            std::lock_guard<std::mutex>
                g(m_sym_attr_mutex);

            sym_map = m_sym_attr_map;
        }

        std::vector<Attribute> attr;
        std::vector<Variant>   data;

        // Bit of a hack: Use mempool to allocate temporary memory for strings.
        // Should be fixed with string database in Caliper runtime.
        MemoryPool mempool(64 * 1024);

        // unpack nodes, check for address attributes, and perform inst lookup
        for (auto it : sym_map) {
            Entry e = snapshot->get(it.first);

            if (e.node()) {
                for (const cali::Node* node = e.node(); node; node = node->parent()) 
                    if (node->attribute() == it.first.id())
                        add_inst_attributes(Entry(node), it.second, mempool, attr, data);
            } else if (e.is_immediate()) {
                add_inst_attributes(e, it.second, mempool, attr, data);
            }
        }

        // reverse vectors to restore correct hierarchical order
        std::reverse(attr.begin(), attr.end());
        std::reverse(data.begin(), data.end());

        // Add entries to snapshot. Strings are copied here, temporary mempool will be free'd
        if (attr.size() > 0)
            c->make_entrylist(attr.size(), attr.data(), data.data(), *snapshot);
    }

    // some final log output; print warning if we didn't find an address attribute
    void finish_log(Caliper* c) {
        Log(1).stream() << "Instlookup: Performed " 
                        << m_num_lookups << " address lookups, "
                        << m_num_failed  << " failed." 
                        << std::endl;
    }

    void init_lookup() {

        // TODO: initialize instructionAPI lookups
        
        std::lock_guard<std::mutex> 
            g(m_lookup_mutex);

        if (!m_lookup) {
            m_lookup = AddressLookup::createAddressLookup();

            if (!m_lookup)
                Log(0).stream() << "Instlookup: Could not create address lookup object"
                                << std::endl;

            m_lookup->refresh();
        }
    }

    static void pre_flush_cb(Caliper* c, const SnapshotRecord*) {
        s_instance->check_attributes(c);
        s_instance->init_lookup();
    }

    static void postprocess_snapshot_cb(Caliper* c, SnapshotRecord* snapshot) {
        s_instance->process_snapshot(c, snapshot);
    }

    static void finish_cb(Caliper* c) {
        s_instance->finish_log(c);
    }

    void register_callbacks(Caliper* c) {
        c->events().pre_flush_evt.connect(pre_flush_cb);
        c->events().postprocess_snapshot.connect(postprocess_snapshot_cb);
        c->events().finish_evt.connect(finish_cb);
    }

    InstLookup(Caliper* c)
        : m_config(RuntimeConfig::init("instlookup", s_configdata)),
          m_lookup(0)
        {
            util::split(m_config.get("attributes").to_string(), ':',
                        std::back_inserter(m_addr_attr_names));

            m_instruction_type = m_config.get("instruction_type").to_bool();

            register_callbacks(c);

            Log(1).stream() << "Registered instlookup service" << std::endl;
        }

public:

    static void create(Caliper* c) {
        s_instance.reset(new InstLookup(c));
    }
};

std::unique_ptr<InstLookup> InstLookup::s_instance { nullptr };

const ConfigSet::Entry InstLookup::s_configdata[] = {
    { "attributes", CALI_TYPE_STRING, "",
      "List of address attributes for which to perform inst lookup",
      "List of address attributes for which to perform inst lookup",
    },
    { "instruction_type", CALI_TYPE_BOOL, "true",
      "Look up the type of instruction (memory read/write, compute)",
      "Look up the type of instruction (memory read/write, compute)",
    },
    ConfigSet::Terminator
};
    
} // namespace


namespace cali
{
    CaliperService instlookup_service { "instlookup", &(::InstLookup::create) };
}
