// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// SymbolLookup.cpp
// Caliper symbol lookup service

#include "Lookup.h"

#include "../Services.h"
#include "caliper/CaliperService.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

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
    struct SymbolAttributeInfo {
        Attribute target_attr;
        Attribute file_attr;
        Attribute line_attr;
        Attribute func_attr;
        Attribute loc_attr;
        Attribute mod_attr;
        Attribute sym_node_attr;

        std::unordered_map<uint64_t, Node*> lookup_cache;
        std::mutex lookup_cache_mutex;
    };

    Node m_root_node;

    bool m_lookup_functions;
    bool m_lookup_sourceloc;
    bool m_lookup_file;
    bool m_lookup_line;
    bool m_lookup_mod;

    std::map<Attribute, std::shared_ptr<SymbolAttributeInfo>> m_sym_attr_map;
    std::mutex m_sym_attr_mutex;

    std::vector<std::string> m_addr_attr_names;

    Lookup   m_lookup;

    unsigned m_num_lookups;
    unsigned m_num_cached;
    unsigned m_num_failed;

    //
    // --- methods
    //

    void make_symbol_attributes(Caliper* c, const Attribute& attr) {
        {
            std::lock_guard<std::mutex>
                g(m_sym_attr_mutex);

            if (m_sym_attr_map.count(attr) > 0)
                return;
        }

        auto sym_attribs = std::make_shared<SymbolAttributeInfo>();

        sym_attribs->target_attr = attr;
        sym_attribs->file_attr =
            c->create_attribute("source.file#" + attr.name(),
                                CALI_TYPE_STRING, CALI_ATTR_SKIP_EVENTS);
        sym_attribs->line_attr =
            c->create_attribute("source.line#" + attr.name(),
                                CALI_TYPE_UINT,   CALI_ATTR_SKIP_EVENTS);
        sym_attribs->func_attr =
            c->create_attribute("source.function#" + attr.name(),
                                CALI_TYPE_STRING, CALI_ATTR_SKIP_EVENTS);
        sym_attribs->loc_attr  =
            c->create_attribute("sourceloc#" + attr.name(),
                                CALI_TYPE_STRING, CALI_ATTR_SKIP_EVENTS);
        sym_attribs->mod_attr  =
            c->create_attribute("module#" + attr.name(),
                                CALI_TYPE_STRING, CALI_ATTR_SKIP_EVENTS);
        sym_attribs->sym_node_attr =
            c->create_attribute("symbollookup.node#" + attr.name(),
                                CALI_TYPE_UINT, CALI_ATTR_SKIP_EVENTS | CALI_ATTR_HIDDEN);

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

    Node* perform_lookup(Caliper* c, Entry e, SymbolAttributeInfo& sym_info, Node* parent) {
        if (e.empty())
            return nullptr;

        ++m_num_lookups;

        uintptr_t addr = e.value().to_uint();

        {
            std::lock_guard<std::mutex>
                g(sym_info.lookup_cache_mutex);

            auto it = sym_info.lookup_cache.find(addr);
            if (it != sym_info.lookup_cache.end()) {
                ++m_num_cached;
                return it->second;
            }
        }

        int what = 0;

        if (m_lookup_functions)
            what |= Lookup::Name;
        if (m_lookup_file || m_lookup_sourceloc)
            what |= Lookup::File;
        if (m_lookup_line || m_lookup_sourceloc)
            what |= Lookup::Line;
        if (m_lookup_mod)
            what |= Lookup::Module;

        Lookup::Result result = m_lookup.lookup(addr, what);
        if (!result.success)
            ++m_num_failed;

        Node* node = parent;

        // We go from coarse grained to fine grained info here to speed up tree creation
        if (m_lookup_mod)
            node = c->make_tree_entry(sym_info.mod_attr,  Variant(result.module.c_str()), node);
        if (m_lookup_file)
            node = c->make_tree_entry(sym_info.file_attr, Variant(result.file.c_str()), node);
        if (m_lookup_functions)
            node = c->make_tree_entry(sym_info.func_attr, Variant(result.name.c_str()), node);
        if (m_lookup_line)
            node = c->make_tree_entry(sym_info.line_attr, Variant(static_cast<uint64_t>(result.line)), node);
        if (m_lookup_sourceloc) {
            std::string tmp = result.file + ":" + std::to_string(result.line);
            node = c->make_tree_entry(sym_info.loc_attr, Variant(tmp.c_str()), node);
        }

        if (node == parent)
            return nullptr;

        {
            std::lock_guard<std::mutex>
                g(sym_info.lookup_cache_mutex);
            sym_info.lookup_cache[addr] = node;
        }

        return node;
    }

    Node* find_symbol_node_entry(Caliper* c, Node* node, Attribute sym_node_attr) {
        for (node = node->first_child(); node; node = node->next_sibling())
            if (node->attribute() == sym_node_attr.id())
                return c->node(node->data().to_uint());

        return nullptr;
    }

    Entry get_symbol_entry(Caliper* c, Entry e, SymbolAttributeInfo& sym_info) {
        e = e.get(sym_info.target_attr);

        if (e.is_reference()) {
            Node* sym_node = find_symbol_node_entry(c, e.node(), sym_info.sym_node_attr);

            if (sym_node)
                return Entry(sym_node);

            Entry parent = get_symbol_entry(c, Entry(e.node()->parent()), sym_info);
            sym_node = perform_lookup(c, e, sym_info, parent.empty() ? &m_root_node : parent.node());

            if (sym_node)
                c->make_tree_entry(sym_info.sym_node_attr, Variant(sym_node->id()), e.node());

            e = Entry(sym_node);
        } else if (e.is_immediate()) {
            e = Entry(perform_lookup(c, e, sym_info, &m_root_node));
        }

        return e;
    }

    void process_snapshot(Caliper* c, std::vector<Entry>& rec) {
        if (m_sym_attr_map.empty())
            return;

        // make local symbol attribute map copy for threadsafe access

        std::map<Attribute, std::shared_ptr<SymbolAttributeInfo>> sym_map;

        {
            std::lock_guard<std::mutex>
                g(m_sym_attr_mutex);

            sym_map = m_sym_attr_map;
        }

        std::vector<Entry> result;
        result.reserve(sym_map.size());

        for (auto& it : sym_map) {
            SnapshotView v(rec.size(), rec.data());
            Entry e = get_symbol_entry(c, v.get(it.first), *(it.second));

            if (!e.empty())
                result.push_back(e);
        }

        rec.insert(rec.end(), result.begin(), result.end());
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
        : m_root_node(CALI_INV_ID, CALI_INV_ID, Variant()),
          m_num_lookups(0),
          m_num_cached(0),
          m_num_failed(0)
        {
            ConfigSet config = services::init_config_from_spec(chn->config(), s_spec);

            m_addr_attr_names  = config.get("attributes").to_stringlist(",:");

            m_lookup_functions = config.get("lookup_functions").to_bool();
            m_lookup_sourceloc = config.get("lookup_sourceloc").to_bool();
            m_lookup_file      = config.get("lookup_file").to_bool();
            m_lookup_line      = config.get("lookup_line").to_bool();
            m_lookup_mod       = config.get("lookup_module").to_bool();
        }

public:

    static const char* s_spec;

    static void symbollookup_register(Caliper* c, Channel* chn) {
        SymbolLookup* instance = new SymbolLookup(c, chn);

        chn->events().pre_flush_evt.connect(
            [instance](Caliper* c, Channel* chn, SnapshotView){
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

const char* SymbolLookup::s_spec = R"json(
{
    "name": "symbollookup",
    "description": "Perform symbol name lookup for source code address attributes",
    "config": [
        {   "name": "attributes",
            "description": "List of address attributes for which to perform symbol lookup",
            "type": "string"
        },
        {   "name": "lookup_functions",
            "description": "Perform function name lookup",
            "type": "bool",
            "value": "true"
        },
        {   "name": "lookup_sourceloc",
            "description": "Perform source location lookup (source file name + line number)",
            "type": "bool",
            "value": "true"
        },
        {   "name": "lookup_file",
            "description": "Perform source file name lookup",
            "type": "bool",
            "value": "false"
        },
        {   "name": "lookup_line",
            "description": "Perform source line lookup",
            "type": "bool",
            "value": "false"
        },
        {   "name": "lookup_module",
            "description": "Perform module lookup",
            "type": "bool",
            "value": "true"
        }
    ]
}
)json";

} // namespace [anonymous]


namespace cali
{

CaliperService symbollookup_service { ::SymbolLookup::s_spec, ::SymbolLookup::symbollookup_register };

}
