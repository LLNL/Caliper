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

// KokkosLookup.cpp
// Caliper kokkos variable lookup service

#include "caliper/CaliperService.h"

#include "caliper/Caliper.h"
#include "../../caliper/MemoryPool.h"

#include "caliper/common/Log.h"
#include "caliper/common/Node.h"
#include "caliper/common/RuntimeConfig.h"

#include <algorithm>
#include <set>
#include <iterator>
#include <map>
#include <cstring>
#include <mutex>
#include <cstdint>

#include "types.hpp"

struct NamedPointer {
   std::uintptr_t ptr;
   std::string name;
   std::uint64_t size;
   const SpaceHandle space;
};

namespace std {
  template<>
  struct less<NamedPointer> {
    static std::less<std::uintptr_t> comp;
    bool operator()(const NamedPointer& n1, const NamedPointer& n2) const{
      return comp(n1.ptr, n2.ptr);
    }
  };

  std::less<std::uintptr_t> std::less<NamedPointer>::comp;

};

std::set<NamedPointer> tracked_pointers;

using namespace cali;

namespace
{

class KokkosLookup
{
    static const ConfigSet::Entry s_configdata[];

    struct KokkosAttributes {
        Attribute variable_name_attr;
        Attribute variable_size_attr;
        Attribute variable_space_attr;
    };

    ConfigSet m_config;

    std::map<Attribute, KokkosAttributes> m_sym_attr_map;
    std::mutex m_sym_attr_mutex;

    std::vector<std::string> m_addr_attr_names;

    unsigned m_num_lookups;
    unsigned m_num_failed;

    void make_kokkos_attributes(Caliper* c, const Attribute& attr) {
        struct KokkosAttributes sym_attribs;

        sym_attribs.variable_name_attr = 
            c->create_attribute("kokkos.variable_name#" + attr.name(), 
                                CALI_TYPE_STRING, CALI_ATTR_DEFAULT);
        sym_attribs.variable_size_attr = 
            c->create_attribute("kokkos.variable_size#" + attr.name(), 
                                CALI_TYPE_INT, CALI_ATTR_DEFAULT);
        sym_attribs.variable_space_attr = 
            c->create_attribute("kokkos.variable_space#" + attr.name(), 
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
                    Log(0).stream() << "Kokkoslookup: Address attribute \""
                                    << s << "\" not found!" << std::endl;
            }
        }

        if (vec.empty())
            Log(1).stream() << "Kokkoslookup: No address attributes found." 
                            << std::endl;

        for (const Attribute& a : vec)
            make_kokkos_attributes(c, a);
    }
    
    void add_kokkos_attributes(const Entry& e, 
                               const KokkosAttributes& sym_attr,
                               MemoryPool& mempool,
                               std::vector<Attribute>& attr, 
                               std::vector<Variant>&   data) {

        uint64_t address  = e.value().to_uint();
        
        auto lookup = tracked_pointers.find(NamedPointer{address,"",0,SpaceHandle{}});
        if(lookup != tracked_pointers.end()){

          attr.push_back(sym_attr.variable_size_attr); 
          data.push_back(lookup->size);

          attr.push_back(sym_attr.variable_name_attr); 
          data.push_back(Variant(CALI_TYPE_STRING,lookup->name.c_str(),lookup->name.size()));


          attr.push_back(sym_attr.variable_space_attr); 
          data.push_back(Variant(CALI_TYPE_STRING,lookup->space.name,strnlen(lookup->space.name,64)));
        }

    }

    void process_snapshot(Caliper* c, std::vector<Entry>& rec) {
        if (m_sym_attr_map.empty())
            return;

        // make local kokkos attribute map copy for threadsafe access

        std::map<Attribute, KokkosAttributes> sym_map;

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

        // unpack nodes, check for address attributes, and perform kokkos lookup
        for (auto it : sym_map) {
            cali_id_t sym_attr_id = it.first.id();
            
            for (const Entry& e : rec)
                if (e.is_reference()) {
                    for (const cali::Node* node = e.node(); node; node = node->parent()) 
                        if (node->attribute() == sym_attr_id)
                            add_kokkos_attributes(Entry(node), it.second, mempool, attr, data);
                } else if (e.attribute() == sym_attr_id) {
                    add_kokkos_attributes(e, it.second, mempool, attr, data);
                }
        }

        // reverse vectors to restore correct hierarchical order
        std::reverse(attr.begin(), attr.end());
        std::reverse(data.begin(), data.end());

        // Add entries to snapshot. Strings are copied here, temporary mempool will be free'd

        Node* node = nullptr;
        
        for (size_t i = 0; i < attr.size(); ++i)
            if (attr[i].store_as_value())
                rec.emplace_back(Entry(attr[i], data[i]));
            else
                node = c->make_tree_entry(attr[i], data[i], node);

        if (node)
            rec.emplace_back(Entry(node));
    }

    // some final log output; print warning if we didn't find an address attribute
    void finish_log(Caliper* c, Channel* chn) {
        Log(1).stream() << chn->name()   << ": Kokkoslookup: Performed " 
                        << m_num_lookups << " address lookups, "
                        << m_num_failed  << " failed." 
                        << std::endl;
    }

    KokkosLookup(Caliper* c, Channel* chn)
        
        {
            ConfigSet config =
                chn->config().init("kokkoslookup", s_configdata);
            
            m_addr_attr_names  = config.get("attributes").to_stringlist(",:");
        }

public:

    static void kokkoslookup_register(Caliper* c, Channel* chn) {

        auto* instance = new KokkosLookup(c, chn);
        kokkosp_callbacks.kokkosp_allocate_callback.connect([&](const SpaceHandle handle, const char* name, const void* const ptr, const uint64_t size){
            
            tracked_pointers.insert(NamedPointer{reinterpret_cast<std::uintptr_t>(ptr), std::string(name), size, handle});
        });

        chn->events().pre_flush_evt.connect(
            [instance](Caliper* c, Channel* chn, const SnapshotRecord* info){
                instance->check_attributes(c);
            });
        chn->events().postprocess_snapshot.connect(
            [instance](Caliper* c, Channel* chn, std::vector<Entry>& rec){
                instance->process_snapshot(c, rec);
            });
        chn->events().finish_evt.connect(
            [instance](Caliper* c, Channel* chn){
                instance->finish_log(c, chn);
                delete instance;
            });

        Log(1).stream() << chn->name() << ": Registered kokkoslookup service" << std::endl;
    }
};

const ConfigSet::Entry KokkosLookup::s_configdata[] = {
    { "attributes", CALI_TYPE_STRING, "",
      "List of address attributes for which to perform kokkos lookup",
      "List of address attributes for which to perform kokkos lookup",
    },
    ConfigSet::Terminator
};
    
} // namespace [anonymous]


namespace cali
{

CaliperService kokkoslookup_service { "kokkoslookup", ::KokkosLookup::kokkoslookup_register };

}
