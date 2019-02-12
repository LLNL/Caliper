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

// InstLookup.cpp
// Caliper instruction lookup service

#include "caliper/CaliperService.h"

#include "x86_util.h"

#include "caliper/Caliper.h"
#include "../../caliper/MemoryPool.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/common/Log.h"
#include "caliper/common/Node.h"
#include "caliper/common/RuntimeConfig.h"

#include <Symtab.h>
#include <LineInformation.h>
#include <CodeObject.h> // parseAPI
#include <InstructionDecoder.h> // instructionAPI
#include <Function.h>
#include <AddrLookup.h>

#include <algorithm>
#include <map>
#include <memory>
#include <mutex>

using namespace cali;

using namespace Dyninst;
using namespace ParseAPI;
using namespace SymtabAPI;
using namespace InstructionAPI;

namespace
{

class InstLookup
{
    static const ConfigSet::Entry s_configdata[];

    struct InstAttributes {
        Attribute op_attr;
        Attribute read_size_attr;
        Attribute write_size_attr;
    };

    bool m_instruction_type;

    std::map<Attribute, InstAttributes> m_sym_attr_map;
    std::mutex m_sym_attr_mutex;

    std::vector<std::string> m_addr_attr_names;

    bool m_arch_set = false;
    Architecture m_arch;
    unsigned int m_inst_length;
    SymtabCodeSource *m_sts;
    std::mutex     m_lookup_mutex;

    unsigned m_num_lookups;
    unsigned m_num_failed;

    //
    // --- methods
    //

    void make_inst_attributes(Caliper* c, const Attribute& attr) {
        struct InstAttributes sym_attribs;

        sym_attribs.op_attr = 
            c->create_attribute("instruction.op#" + attr.name(), 
                                CALI_TYPE_STRING, CALI_ATTR_DEFAULT);

        sym_attribs.read_size_attr = 
            c->create_attribute("instruction.read_size#" + attr.name(), 
                                CALI_TYPE_UINT, CALI_ATTR_DEFAULT);
        sym_attribs.write_size_attr = 
            c->create_attribute("instruction.write_size#" + attr.name(), 
                                CALI_TYPE_UINT, CALI_ATTR_DEFAULT);

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

        {
            std::lock_guard<std::mutex>
                g(m_lookup_mutex);

            void *inst_raw = nullptr;
            if(m_sts->isValidAddress(address))
            {
                inst_raw = m_sts->getPtrToInstruction(address);

                if(inst_raw)
                {
                    // Get and decode instruction
                    InstructionDecoder dec(inst_raw, m_inst_length, m_arch);
                    Instruction inst = dec.decode();
                    Operation op = inst.getOperation();
                    entryID eid = op.getID();
                    
                    // Extract semantics
                    uint64_t read_size = 0;
                    uint64_t write_size = 0;
                    std::string inst_name;

                    inst_name = NS_x86::entryNames_IAPI[eid];

                    if(inst.readsMemory()) {
                        read_size = getReadSize(inst);
                    }
                    if(inst.writesMemory()) {
                        read_size = getWriteSize(inst);
                    }

                    // Hack to put string on heap
                    char* tmp_s = static_cast<char*>(mempool.allocate(inst_name.size()+1));
                    std::copy(inst_name.begin(), inst_name.end(), tmp_s);
                    tmp_s[inst_name.size()] = '\0';

                    // Add attributes and data
                    attr.push_back(sym_attr.op_attr);
                    attr.push_back(sym_attr.read_size_attr);
                    attr.push_back(sym_attr.write_size_attr);

                    data.push_back(Variant(CALI_TYPE_STRING, tmp_s, inst_name.size()));
                    data.push_back(Variant(read_size));
                    data.push_back(Variant(write_size));
                }
            }
        }
        
    }

    void process_snapshot(Caliper* c, std::vector<Entry>& rec) {
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
            cali_id_t sym_attr_id = it.first.id();

            for (const Entry& e : rec)
                if (e.node()) {
                    for (const cali::Node* node = e.node(); node; node = node->parent()) 
                        if (node->attribute() == sym_attr_id)
                            add_inst_attributes(Entry(node), it.second, mempool, attr, data);
                } else if (e.attribute() == sym_attr_id) {
                    add_inst_attributes(e, it.second, mempool, attr, data);
                }
        }

        // reverse vectors to restore correct hierarchical order
        std::reverse(attr.begin(), attr.end());
        std::reverse(data.begin(), data.end());

        // Add entries to snapshot. Strings are copied here, temporary mempool will be free'd
        Node* node = nullptr;

        for (size_t i = 0; i < attr.size(); ++i)
            if (attr[i].store_as_value())
                rec.push_back(Entry(attr[i].id(), data[i]));
            else
                node = c->make_tree_entry(attr[i], data[i], node);

        if (node)
            rec.push_back(Entry(node));
    }

    // some final log output; print warning if we didn't find an address attribute
    void finish_log(Caliper* c, Channel* chn) {
        Log(1).stream() << chn->name()   <<  ": Instlookup: Performed " 
                        << m_num_lookups << " address lookups, "
                        << m_num_failed  << " failed." 
                        << std::endl;
    }

    void init_lookup() {

        std::lock_guard<std::mutex> 
            g(m_lookup_mutex);

        std::string self_bin("/proc/self/exe");

        if (!m_arch_set) {
            // TODO: how to use dynamic symtab instead?
            Symtab *tab;

            int sym_success = Symtab::openFile(tab, self_bin);
            if(!sym_success)
                Log(0).stream() << "Instlookup: Could not create symtab object"
                                << std::endl;

            m_arch = tab->getArchitecture();

            m_arch_set = true;
        }

        if (!m_sts) {
            m_sts = new SymtabCodeSource((char*)self_bin.c_str());

            if(!m_sts)
                Log(0).stream() << "Instlookup: Could not create symtab code source"
                                << std::endl;
        }

        m_inst_length = InstructionDecoder::maxInstructionLength;
    }

    InstLookup(Caliper* c, Channel* chn)
        : m_sts(nullptr)
        {
            ConfigSet config =
                chn->config().init("instlookup", s_configdata);
            
            m_addr_attr_names  = config.get("attributes").to_stringlist(",:");
            m_instruction_type = config.get("instruction_type").to_bool();
        }

public:

    static void instlookup_register(Caliper* c, Channel* chn) {
        InstLookup* instance = new InstLookup(c, chn);
        
        chn->events().pre_flush_evt.connect(
            [instance](Caliper* c, Channel* chn, const SnapshotRecord* info){
                instance->check_attributes(c);
                instance->init_lookup();
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
        
        Log(1).stream() << chn->name() << ": Registered instlookup service" << std::endl;
    }
};

const ConfigSet::Entry InstLookup::s_configdata[] = {
    { "attributes", CALI_TYPE_STRING, "",
      "List of address attributes for which to perform inst lookup",
      "List of address attributes for which to perform inst lookup",
    },
    ConfigSet::Terminator
};
    
} // namespace


namespace cali
{

CaliperService instlookup_service { "instlookup", ::InstLookup::instlookup_register };

}
