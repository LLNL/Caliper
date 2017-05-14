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

/// \file SymbolLookup.cpp
/// \brief Caliper symbol lookup service

#include "../CaliperService.h"

#include "Caliper.h"
#include "MemoryPool.h"
#include "SnapshotRecord.h"

#include "Log.h"
#include "Node.h"
#include "RuntimeConfig.h"

#include "util/split.hpp"

#include <Symtab.h>
#include <LineInformation.h>
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

namespace
{

class SymbolLookup
{
    static std::unique_ptr<SymbolLookup> s_instance;
    static const ConfigSet::Entry        s_configdata[];

    struct SymbolAttributes {
        Attribute file_attr;
        Attribute line_attr;
        Attribute func_attr;
    };

    ConfigSet m_config;

    bool m_lookup_functions;
    bool m_lookup_sourceloc;

    std::map<Attribute, SymbolAttributes> m_sym_attr_map;
    std::mutex m_sym_attr_mutex;

    std::vector<std::string> m_addr_attr_names;

    AddressLookup* m_lookup;
    std::mutex     m_lookup_mutex;

    unsigned m_num_lookups;
    unsigned m_num_failed;

    //
    // --- methods
    //

    void check_attribute(Caliper* c, const Attribute& attr) {
        auto it = std::find(m_addr_attr_names.begin(), m_addr_attr_names.end(), 
                            attr.name());

        if (it == m_addr_attr_names.end())
            return;

        // it's an address attribute, create symbol attributes for it and add it to the map

        struct SymbolAttributes sym_attribs;

        sym_attribs.file_attr = 
            c->create_attribute("source.file#" + attr.name(), 
                                CALI_TYPE_STRING, CALI_ATTR_DEFAULT);
        sym_attribs.line_attr =
            c->create_attribute("source.line#" + attr.name(), 
                                CALI_TYPE_UINT,   CALI_ATTR_DEFAULT);
        sym_attribs.func_attr = 
            c->create_attribute("source.function#" + attr.name(),
                                CALI_TYPE_STRING, CALI_ATTR_DEFAULT);

        std::lock_guard<std::mutex>
            g(m_sym_attr_mutex);

        m_sym_attr_map.insert(std::make_pair(attr, sym_attribs));
    }

    
    void add_symbol_attributes(const Entry& e, 
                               const SymbolAttributes& sym_attr,
                               MemoryPool& mempool,
                               std::vector<Attribute>& attr, 
                               std::vector<Variant>&   data) {
        std::vector<Statement*> statements;
        SymtabAPI::Function* function = 0;

        bool     ret_line = false;
        bool     ret_func = false;

        uint64_t address  = e.value().to_uint();

        {
            std::lock_guard<std::mutex>
                g(m_lookup_mutex);

            if (!m_lookup)
                return;

            Symtab* symtab;
            Offset  offset;

            bool ret = m_lookup->getOffset(address, symtab, offset);

            if (ret && m_lookup_sourceloc)
                ret_line = symtab->getSourceLines(statements, offset);
            if (ret && m_lookup_functions)
                ret_func = symtab->getContainingFunction(offset, function);

            ++m_num_lookups;
        }

        if (m_lookup_sourceloc) {
            std::string filename = "UNKNOWN";
            uint64_t    lineno   = 0;

            if (ret_line && statements.size() > 0) {
                filename = statements.front()->getFile();
                lineno   = statements.front()->getLine();
            }

            char* tmp_s = static_cast<char*>(mempool.allocate(filename.size()+1));
            std::copy(filename.begin(), filename.end(), tmp_s);
            tmp_s[filename.size()] = '\0';

            attr.push_back(sym_attr.file_attr);
            attr.push_back(sym_attr.line_attr);

            data.push_back(Variant(CALI_TYPE_STRING, tmp_s, filename.size()));
            data.push_back(Variant(CALI_TYPE_UINT,   &lineno, sizeof(uint64_t)));
        }

        if (m_lookup_functions) {
            std::string funcname = "UNKNOWN";

            if (ret_func && function) {
                auto it = function->pretty_names_begin();

                if (it != function->pretty_names_end())
                    funcname = *it;
            }

            char* tmp_f = static_cast<char*>(mempool.allocate(funcname.size()+1));
            std::copy(funcname.begin(), funcname.end(), tmp_f);
            tmp_f[funcname.size()] = '\0';

            attr.push_back(sym_attr.func_attr);
            data.push_back(Variant(CALI_TYPE_STRING, tmp_f, funcname.size()));
        }

        if ((m_lookup_functions && !ret_func) || (m_lookup_sourceloc && !ret_line))
            ++m_num_failed; // not locked, doesn't matter too much if it's slightly off
    }

    void process_snapshot(Caliper* c, SnapshotRecord* snapshot) {
        if (m_sym_attr_map.empty())
            return;

        // make local symbol attribute map copy for threadsafe access

        std::map<Attribute, SymbolAttributes> sym_map;

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

        // unpack nodes, check for address attributes, and perform symbol lookup
        for (auto it : sym_map) {
            Entry e = snapshot->get(it.first);

            if (e.node()) {
                for (const Node* node = e.node(); node; node = node->parent()) 
                    if (node->attribute() == it.first.id())
                        add_symbol_attributes(Entry(node), it.second, mempool, attr, data);
            } else if (e.is_immediate()) {
                add_symbol_attributes(e, it.second, mempool, attr, data);
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
        Log(1).stream() << "Symbollookup: Performed " 
                        << m_num_lookups << " address lookups, "
                        << m_num_failed  << " failed." 
                        << std::endl;

        if (m_addr_attr_names.size() != m_sym_attr_map.size())
            for (const std::string& attrname : m_addr_attr_names) {
                Attribute attr = c->get_attribute(attrname);
                bool found = false;

                if (attr != Attribute::invalid) {
                    auto it = m_sym_attr_map.find(attr);
                    found = (it != m_sym_attr_map.end());
                }

                if (!found) 
                    Log(1).stream() << "Symbollookup: Address attribute " 
                                    << attrname << " not found!" 
                                    << std::endl;
            }
    }

    void init_lookup() {
        std::lock_guard<std::mutex> 
            g(m_lookup_mutex);

        if (!m_lookup) {
            m_lookup = AddressLookup::createAddressLookup();

            if (!m_lookup)
                Log(0).stream() << "Symbollookup: Could not create address lookup object"
                                << std::endl;

            m_lookup->refresh();
        }
    }

    static void create_attr_cb(Caliper* c, const Attribute& attr) {
        s_instance->check_attribute(c, attr);
    }

    static void post_init_cb(Caliper* c) {
        for (const std::string& s : s_instance->m_addr_attr_names) {
            Attribute attr = c->get_attribute(s);

            if (attr != Attribute::invalid)
                s_instance->check_attribute(c, attr);
        }
    }

    static void pre_flush_cb(Caliper*, const SnapshotRecord*) {
        s_instance->init_lookup();
    }

    static void pre_flush_snapshot_cb(Caliper* c, SnapshotRecord* snapshot) {
        s_instance->process_snapshot(c, snapshot);
    }

    static void finish_cb(Caliper* c) {
        s_instance->finish_log(c);
    }

    void register_callbacks(Caliper* c) {
        c->events().post_init_evt.connect(post_init_cb);
        c->events().create_attr_evt.connect(create_attr_cb);
        c->events().pre_flush_evt.connect(pre_flush_cb);
        c->events().pre_flush_snapshot.connect(pre_flush_snapshot_cb);
        c->events().finish_evt.connect(finish_cb);
    }

    SymbolLookup(Caliper* c)
        : m_config(RuntimeConfig::init("symbollookup", s_configdata)),
          m_lookup(0)
        {
            util::split(m_config.get("attributes").to_string(), ':',
                        std::back_inserter(m_addr_attr_names));

            m_lookup_functions = m_config.get("lookup_functions").to_bool();
            m_lookup_sourceloc = m_config.get("lookup_sourceloc").to_bool();

            if (m_addr_attr_names.empty()) {
                Log(1).stream() << "symbolookup: no address attributes given" << std::endl;
                return;
            }

            register_callbacks(c);

            Log(1).stream() << "Registered symbollookup service" << std::endl;
        }

public:

    static void create(Caliper* c) {
        s_instance.reset(new SymbolLookup(c));
    }
};

std::unique_ptr<SymbolLookup> SymbolLookup::s_instance { nullptr };

const ConfigSet::Entry SymbolLookup::s_configdata[] = {
    { "attributes", CALI_TYPE_STRING, "",
      "List of address attributes for which to perform symbol lookup",
      "List of address attributes for which to perform symbol lookup",
    },
    { "lookup_functions", CALI_TYPE_BOOL, "true",
      "Perform function name lookup",
      "Perform function name lookup",
    },
    { "lookup_sourceloc", CALI_TYPE_BOOL, "true",
      "Perform source location (filename/linenumber) lookup",
      "Perform source location (filename/linenumber) lookup",
    },
    ConfigSet::Terminator
};
    
} // namespace


namespace cali
{
    CaliperService symbollookup_service { "symbollookup", &(::SymbolLookup::create) };
}
