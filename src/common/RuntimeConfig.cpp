/// @file RuntimeConfig.cpp
/// RuntimeConfig class implementation

#include "RuntimeConfig.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <map>
#include <unordered_map>

using namespace cali;
using namespace std;


namespace 
{
    const std::string prefix { "cali" };

    string config_var_name(const string& name, const string& key) {
        // make uppercase PREFIX_NAMESPACE_KEY string

        string str;

        for ( string s : { prefix, string("_"), name, string("_"), key } )
            str.append(s);

        transform(str.begin(), str.end(), str.begin(), ::toupper);

        return str;
    }
}


//
// --- ConfigSet implementation
//

struct cali::ConfigSetImpl
{
    // --- data

    unordered_map<string, ConfigSet::Entry> m_dict;

    // --- interface

    Variant get(const char* key) const {
        auto it = m_dict.find(key);
        return (it == m_dict.end() ? Variant() : Variant(string(it->second.value)));
    }

    void init(const char* name, const ConfigSet::Entry* list) {
        for (const ConfigSet::Entry* e = list; e && e->key; ++e) {
            ConfigSet::Entry newent = *e;

            // See if there is a config variable set
            char* val = getenv(::config_var_name(name, e->key).c_str());

            newent.value = val ? val : e->value;

            m_dict.emplace(make_pair(string(e->key), newent));
        }
    }
};


//
// --- RuntimeConfig implementation
//

struct RuntimeConfigImpl
{
    // --- data

    static unique_ptr<RuntimeConfigImpl>     s_instance;

    map< string, shared_ptr<ConfigSetImpl> > m_database;


    // --- interface

    Variant get(const char* set, const char* key) const {
        auto it = m_database.find(set);
        return (it == m_database.end() ? Variant() : it->second->get(key));
    }

    shared_ptr<ConfigSetImpl> init(const char* name, const ConfigSet::Entry* list) {
        shared_ptr<ConfigSetImpl> ret { new ConfigSetImpl };

        ret->init(name, list);
        m_database.insert(make_pair(string(name), ret));

        return ret;
    }

    void print(ostream& os) const {
        for ( auto set : m_database )
            for ( auto entry : set.second->m_dict )
                os << "# " << entry.second.descr << '\n' 
                   << ::config_var_name(set.first, entry.second.key) << '=' << entry.second.value << endl;
    }

    static RuntimeConfigImpl* instance() {
        if (!s_instance)
            s_instance.reset(new RuntimeConfigImpl);

        return s_instance.get();
    }
};

unique_ptr<RuntimeConfigImpl> RuntimeConfigImpl::s_instance { nullptr };


//
// --- ConfigSet public interface 
// 

ConfigSet::ConfigSet(const shared_ptr<ConfigSetImpl>& p)
    : mP { p }
{ }

Variant 
ConfigSet::get(const char* key) const 
{
    if (!mP)
        return Variant();

    return mP->get(key);
}


//
// --- RuntimeConfig public interface
//

Variant
RuntimeConfig::get(const char* set, const char* key)
{
    return RuntimeConfigImpl::instance()->get(set, key);
}

ConfigSet 
RuntimeConfig::init(const char* name, const ConfigSet::Entry* list)
{
    return ConfigSet(RuntimeConfigImpl::instance()->init(name, list));
}

void
RuntimeConfig::print(ostream& os)
{
    RuntimeConfigImpl::instance()->print(os);
}
 
