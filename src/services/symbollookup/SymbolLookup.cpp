// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// SymbolLookup.cpp
// Caliper symbol lookup service

#include "Lookup.h"

#include "caliper/CaliperService.h"

#include "caliper/Caliper.h"
#include "../../caliper/MemoryPool.h"

#include "caliper/common/Log.h"
#include "caliper/common/Node.h"
#include "caliper/common/RuntimeConfig.h"

#include <algorithm>
#include <iterator>
#include <map>
#include <memory>
#include <mutex>
#include <unordered_map>

using namespace cali;
using namespace symbollookup;

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

    Lookup   m_lookup;

    std::unordered_map<uintptr_t, Lookup::Result> m_lookup_cache;
    std::mutex m_lookup_cache_mutex;

    unsigned m_num_lookups;
    unsigned m_num_cached;
    unsigned m_num_failed;

    //
    // --- methods
    //

    void make_symbol_attributes(Caliper* c, const Attribute& attr) {
        struct SymbolAttributes sym_attribs;

        sym_attribs.file_attr =
            c->create_attribute("source.file#" + attr.name(),
                                CALI_TYPE_STRING, CALI_ATTR_SKIP_EVENTS);
        sym_attribs.line_attr =
            c->create_attribute("source.line#" + attr.name(),
                                CALI_TYPE_UINT,   CALI_ATTR_SKIP_EVENTS);
        sym_attribs.func_attr =
            c->create_attribute("source.function#" + attr.name(),
                                CALI_TYPE_STRING, CALI_ATTR_SKIP_EVENTS);
        sym_attribs.loc_attr  =
            c->create_attribute("sourceloc#" + attr.name(),
                                CALI_TYPE_STRING, CALI_ATTR_SKIP_EVENTS);
        sym_attribs.mod_attr  =
            c->create_attribute("module#" + attr.name(),
                                CALI_TYPE_STRING, CALI_ATTR_SKIP_EVENTS);

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

        int what = 0;

        if (m_lookup_functions)
            what |= Lookup::Name;
        if (m_lookup_file || m_lookup_sourceloc)
            what |= Lookup::File;
        if (m_lookup_line || m_lookup_sourceloc)
            what |= Lookup::Line;
        if (m_lookup_mod)
            what |= Lookup::Module;

        uintptr_t addr = e.value().to_uint();

        Lookup::Result result;
        bool found_in_cache = false;

        {
            std::lock_guard<std::mutex>
                g(m_lookup_cache_mutex);

            auto it = m_lookup_cache.find(addr);
            if (it != m_lookup_cache.end()) {
                found_in_cache = true;
                result = it->second;
                ++m_num_cached;
            }
        }

        if (!found_in_cache) {
            result = m_lookup.lookup(e.value().to_uint(), what);

            std::lock_guard<std::mutex>
                g(m_lookup_cache_mutex);

            m_lookup_cache.insert(std::make_pair(addr, result));
        }

        ++m_num_lookups;

        if (!result.success)
            ++m_num_failed;

        if (m_lookup_sourceloc) {
            std::string sourceloc = result.file;
            sourceloc.append(":");
            sourceloc.append(std::to_string(result.line));

            char* tmp_s = static_cast<char*>(mempool.allocate(sourceloc.size()+1));
            std::copy(sourceloc.begin(), sourceloc.end(), tmp_s);
            tmp_s[sourceloc.size()] = '\0';

            attr.push_back(sym_attr.loc_attr);
            data.push_back(Variant(tmp_s));
        }

        if (m_lookup_file) {
            char* tmp_s = static_cast<char*>(mempool.allocate(result.file.size()+1));
            std::copy(result.file.begin(), result.file.end(), tmp_s);
            tmp_s[result.file.size()] = '\0';

            attr.push_back(sym_attr.file_attr);
            data.push_back(Variant(tmp_s));
        }

        if (m_lookup_line) {
            attr.push_back(sym_attr.line_attr);
            data.push_back(cali_make_variant_from_uint(static_cast<uint64_t>(result.line)));
        }

        if (m_lookup_functions) {
            char* tmp_f = static_cast<char*>(mempool.allocate(result.name.size()+1));
            std::copy(result.name.begin(), result.name.end(), tmp_f);
            tmp_f[result.name.size()] = '\0';

            attr.push_back(sym_attr.func_attr);
            data.push_back(Variant(tmp_f));
        }

        if (m_lookup_mod) {
            char* tmp_f = static_cast<char*>(mempool.allocate(result.module.size()+1));
            std::copy(result.module.begin(), result.module.end(), tmp_f);
            tmp_f[result.module.size()] = '\0';

            attr.push_back(sym_attr.mod_attr);
            data.push_back(Variant(tmp_f));
        }
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
                        << m_num_cached  << " cached, "
                        << m_num_failed  << " failed."
                        << std::endl;
    }

    void init_lookup() {
        m_num_lookups = 0;
        m_num_failed  = 0;
    }

    SymbolLookup(Caliper* c, Channel* chn)
        : m_num_lookups(0), m_num_cached(0), m_num_failed(0)
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
