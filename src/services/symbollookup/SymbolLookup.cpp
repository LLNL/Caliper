// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// SymbolLookup.cpp
// Caliper symbol lookup service

#include "caliper/CaliperService.h"

#include "caliper/Caliper.h"
#include "../../caliper/MemoryPool.h"

#include "caliper/common/Log.h"
#include "caliper/common/Node.h"
#include "caliper/common/RuntimeConfig.h"

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
    static const ConfigSet::Entry s_configdata[];

    struct SymbolAttributes {
        Attribute file_attr;
        Attribute line_attr;
        Attribute func_attr;
        Attribute loc_attr;
        Attribute mod_attr;
    };

    ConfigSet m_config;

    bool m_lookup_functions;
    bool m_lookup_sourceloc;
    bool m_lookup_file;
    bool m_lookup_line;
    bool m_lookup_mod;
    
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

    void make_symbol_attributes(Caliper* c, const Attribute& attr) {
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
        sym_attribs.loc_attr  =
            c->create_attribute("sourceloc#" + attr.name(),
                                CALI_TYPE_STRING, CALI_ATTR_DEFAULT);
        sym_attribs.mod_attr  =
            c->create_attribute("module#" + attr.name(),
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
                    Log(0).stream() << "Symbollookup: Address attribute \""
                                    << s << "\" not found!" << std::endl;
            }
        }

        if (vec.empty())
            Log(1).stream() << "Symbollookup: No address attributes found." 
                            << std::endl;

        for (const Attribute& a : vec)
            make_symbol_attributes(c, a);
    }
    
    void add_symbol_attributes(const Entry& e, 
                               const SymbolAttributes& sym_attr,
                               MemoryPool& mempool,
                               std::vector<Attribute>& attr, 
                               std::vector<Variant>&   data) {
        std::vector<Statement*> statements;
        SymtabAPI::Function* function = 0;
        std::string          modname  = "UNKNOWN";

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
            if (ret && m_lookup_mod)
                modname = symtab->name();
            
            ++m_num_lookups;
        }

        if (m_lookup_sourceloc) {
            std::string filename = "UNKNOWN";
            uint64_t    lineno   = 0;

            if (ret_line && statements.size() > 0) {
                filename = statements.front()->getFile();
                lineno   = statements.front()->getLine();
            }

            filename.append(":");
            filename.append(std::to_string(lineno));

            char* tmp_s = static_cast<char*>(mempool.allocate(filename.size()+1));
            std::copy(filename.begin(), filename.end(), tmp_s);
            tmp_s[filename.size()] = '\0';

            attr.push_back(sym_attr.loc_attr);
            data.push_back(Variant(CALI_TYPE_STRING, tmp_s, filename.size()));
        }

        if (m_lookup_file) {
            std::string filename = "UNKNOWN";

            if (ret_line && statements.size() > 0) {
                filename = statements.front()->getFile();
            }

            char* tmp_s = static_cast<char*>(mempool.allocate(filename.size()+1));
            std::copy(filename.begin(), filename.end(), tmp_s);
            tmp_s[filename.size()] = '\0';

            attr.push_back(sym_attr.file_attr);
            data.push_back(Variant(CALI_TYPE_STRING, tmp_s, filename.size()));
        }

        if (m_lookup_line) {
            uint64_t lineno = 0;

            if (ret_line && statements.size() > 0) {
                lineno = statements.front()->getLine();
            }

            attr.push_back(sym_attr.line_attr);
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

        if (m_lookup_mod) {
            char* tmp_f = static_cast<char*>(mempool.allocate(modname.size()+1));
            std::copy(modname.begin(), modname.end(), tmp_f);
            tmp_f[modname.size()] = '\0';

            attr.push_back(sym_attr.mod_attr);
            data.push_back(Variant(CALI_TYPE_STRING, tmp_f, modname.size()));            
        }

        if ((m_lookup_functions && !ret_func) ||
            ((m_lookup_sourceloc || m_lookup_file || m_lookup_line) && !ret_line))
            ++m_num_failed; // not locked, doesn't matter too much if it's slightly off
    }

    void process_snapshot(Caliper* c, std::vector<Entry>& rec) {
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
            cali_id_t sym_attr_id = it.first.id();
            
            for (const Entry& e : rec)
                if (e.is_reference()) {
                    for (const cali::Node* node = e.node(); node; node = node->parent()) 
                        if (node->attribute() == sym_attr_id)
                            add_symbol_attributes(Entry(node), it.second, mempool, attr, data);
                } else if (e.attribute() == sym_attr_id) {
                    add_symbol_attributes(e, it.second, mempool, attr, data);
                }
        }

        // reverse vectors to restore correct hierarchical order
        std::reverse(attr.begin(), attr.end());
        std::reverse(data.begin(), data.end());

        // Add entries to snapshot. Strings are copied here, temporary mempool will be free'd

        Node* node = nullptr;
        
        for (size_t i = 0; i < attr.size(); ++i)
            if (attr[i].store_as_value())
                rec.push_back(Entry(attr[i], data[i]));
            else
                node = c->make_tree_entry(attr[i], data[i], node);

        if (node)
            rec.push_back(Entry(node));
    }

    // some final log output; print warning if we didn't find an address attribute
    void finish_log(Caliper* c, Channel* chn) {
        Log(1).stream() << chn->name()   << ": Symbollookup: Performed " 
                        << m_num_lookups << " address lookups, "
                        << m_num_failed  << " failed." 
                        << std::endl;
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

    SymbolLookup(Caliper* c, Channel* chn)
        : m_lookup(0)
        {
            ConfigSet config =
                chn->config().init("symbollookup", s_configdata);
            
            m_addr_attr_names  = config.get("attributes").to_stringlist(",:");

            m_lookup_functions = config.get("lookup_functions").to_bool();
            m_lookup_sourceloc = config.get("lookup_sourceloc").to_bool();
            m_lookup_file      = config.get("lookup_file").to_bool();
            m_lookup_line      = config.get("lookup_line").to_bool();
            m_lookup_mod       = config.get("lookup_module").to_bool();
        }

public:

    static void symbollookup_register(Caliper* c, Channel* chn) {
        SymbolLookup* instance = new SymbolLookup(c, chn);

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

        Log(1).stream() << chn->name() << ": Registered symbollookup service" << std::endl;
    }
};

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
      "Perform source location (combined filename/linenumber) lookup",
      "Perform source location (combined filename/linenumber) lookup",
    },
    { "lookup_file", CALI_TYPE_BOOL, "false",
      "Perform source file name lookup",
      "Perform source file name lookup",
    },
    { "lookup_line", CALI_TYPE_BOOL, "false",
      "Perform source line number lookup",
      "Perform source line number lookup",
    },
    { "lookup_module", CALI_TYPE_BOOL, "false",
      "Perform module lookup",
      "Perform module lookup",
    },
    ConfigSet::Terminator
};
    
} // namespace [anonymous]


namespace cali
{

CaliperService symbollookup_service { "symbollookup", ::SymbolLookup::symbollookup_register };

}
