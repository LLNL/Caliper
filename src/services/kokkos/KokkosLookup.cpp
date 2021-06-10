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
#include "caliper/SnapshotRecord.h"
// #include "../../caliper/MemoryPool.h"

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

using namespace cali;

#if 0
namespace {

struct NamedPointer {
   std::uintptr_t ptr;
   std::string name;
   std::uint64_t size;
   const cali::kokkos::SpaceHandle space;
};


} // end namespace [anonymous]

namespace std {
  template<>
  struct less<NamedPointer> {
    static std::less<std::uintptr_t> comp;
    bool operator()(const NamedPointer& n1, const NamedPointer& n2) const{
      return comp(n1.ptr, n2.ptr);
    }
  };

  std::less<std::uintptr_t> std::less<NamedPointer>::comp;

} //end namespace std
#endif

namespace
{

using namespace cali;

// std::set<NamedPointer> tracked_pointers;

class KokkosLookup
{
    static const ConfigSet::Entry s_configdata[];

    unsigned   m_num_spaces = 0;
    unsigned   m_num_copies = 0;

    Attribute  m_space_attr { Attribute::invalid };
    Attribute  m_size_attr  { Attribute::invalid };
    Attribute  m_dst_attr   { Attribute::invalid };
    Attribute  m_src_attr   { Attribute::invalid };

    Channel*   m_channel;

#if 0
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

        auto lookup = tracked_pointers.find(NamedPointer{address,"",0,cali::kokkos::SpaceHandle{}});
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
#endif

    // some final log output; print warning if we didn't find an address attribute
    void finish_log(Caliper* c, Channel* chn) {
        // Log(1).stream() << chn->name()   << ": Kokkoslookup: Performed "
        //                 << m_num_lookups << " address lookups, "
        //                 << m_num_failed  << " failed."
        //                 << std::endl;

        Log(1).stream() << chn->name()  << ": KokkosLookup: Tracked "
                        << m_num_spaces << " spaces, "
                        << m_num_copies << " copies."
                        << std::endl;
    }

    void kokkos_allocate(const cali::kokkos::SpaceHandle handle, const char* name, const void* const ptr, size_t size) {
        Caliper c;
        Variant v_space(handle.name);

        c.memory_region_begin(m_channel, ptr, name, 1, 1, &size, 1, &m_space_attr, &v_space);
        ++m_num_spaces;
    }

    void kokkos_deallocate(const void* const ptr) {
        Caliper c;
        c.memory_region_end(ptr);
    }

    void kokkos_deepcopy(const void* dst, const void* src, uint64_t size) {
        Caliper c;

        cali_id_t attr[3] = { m_dst_attr.id(), m_src_attr.id(), m_size_attr.id() };
        Variant   data[3] = {
            Variant(CALI_TYPE_ADDR, &dst, sizeof(void*)),
            Variant(CALI_TYPE_ADDR, &src, sizeof(void*)),
            Variant(cali_make_variant_from_uint(size))
        };

        SnapshotRecord info(0, nullptr, 3, attr, data);
        c.push_snapshot(m_channel, &info);

        ++m_num_copies;
    }

    void make_attributes(Caliper* c) {
        m_space_attr =
            c->create_attribute("kokkos.space", CALI_TYPE_STRING, CALI_ATTR_SKIP_EVENTS);

        Attribute class_mem = c->get_attribute("class.memoryaddress");
        Variant   v_true(true);

        m_size_attr =
            c->create_attribute("kokkos.size", CALI_TYPE_UINT,
                CALI_ATTR_ASVALUE | CALI_ATTR_SKIP_EVENTS);

        m_src_attr =
            c->create_attribute("kokkos.src.addr", CALI_TYPE_ADDR,
                CALI_ATTR_ASVALUE | CALI_ATTR_SKIP_EVENTS,
                1, &class_mem, &v_true);
        m_dst_attr =
            c->create_attribute("kokkos.dst.addr", CALI_TYPE_ADDR,
                CALI_ATTR_ASVALUE | CALI_ATTR_SKIP_EVENTS,
                1, &class_mem, &v_true);
    }

    KokkosLookup(Caliper* c, Channel* chn)
            : m_channel(chn)
        {
            ConfigSet config =
                chn->config().init("kokkoslookup", s_configdata);

            make_attributes(c);
        }

public:

    static void kokkoslookup_register(Caliper* c, Channel* chn) {
        auto* instance = new KokkosLookup(c, chn);

        kokkosp_callbacks.kokkosp_allocate_callback.connect([instance](const cali::kokkos::SpaceHandle handle, const char* name, const void* const ptr, const uint64_t size){
            instance->kokkos_allocate(handle, name, ptr, size);
            // tracked_pointers.insert(NamedPointer{reinterpret_cast<std::uintptr_t>(ptr), std::string(name), size, handle});
        });

        kokkosp_callbacks.kokkosp_deallocate_callback.connect([instance](const cali::kokkos::SpaceHandle, const char*, const void* const ptr, const uint64_t){
            instance->kokkos_deallocate(ptr);
            // tracked_pointers.insert(NamedPointer{reinterpret_cast<std::uintptr_t>(ptr), std::string(name), size, handle});
        });

        kokkosp_callbacks.kokkosp_begin_deep_copy_callback.connect([instance](const cali::kokkos::SpaceHandle, const char*, const void* dst_ptr,
            const cali::kokkos::SpaceHandle, const char*, const void* src_ptr,
            const uint64_t size){
                instance->kokkos_deepcopy(dst_ptr, src_ptr, size);
            });
#if 0
        chn->events().pre_flush_evt.connect(
            [instance](Caliper* c, Channel* chn, const SnapshotRecord* info){
                instance->check_attributes(c);
            });
        chn->events().postprocess_snapshot.connect(
            [instance](Caliper* c, Channel* chn, std::vector<Entry>& rec){
                instance->process_snapshot(c, rec);
            });
#endif
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
