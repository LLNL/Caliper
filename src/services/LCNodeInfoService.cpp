// Copyright (c) 2015-2024, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// LCNodeInfo.cpp
//   A Caliper service that collects information from the /etc/node_info.json
// file on TOSS4 systems

#include "caliper/CaliperService.h"

#include "Services.h"

#include "caliper/Caliper.h"

#include "caliper/common/Log.h"
#include "caliper/common/RuntimeConfig.h"
#include "caliper/common/StringConverter.h"
#include "caliper/common/Variant.h"

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace cali;

namespace
{

const char* lcnodeinfo_service_spec = R"json(
{
 "name"        : "lcnodeinfo",
 "description" : "Read node information from /etc/node_info.json",
 "config"      :
 [
  { "name"        : "filename",
    "description" : "The JSON file to read",
    "type"        : "string",
    "value"       : "/etc/node_info.json"
  },
  { "name"        : "keys",
    "description" : "List of JSON dict keys to read",
    "type"        : "string",
    "value"       : "host.name,host.cluster,host.os"
  }
 ]
}
)json";

std::pair<bool, StringConverter>
find_key(const std::vector<std::string>& path, const std::map<std::string, StringConverter>& dict)
{
    if (path.empty())
        return std::make_pair(false, StringConverter());

    auto it = dict.find(path.front());
    if (it == dict.end())
        return std::make_pair(false, StringConverter());

    if (path.size() == 1)
        return std::make_pair(true, it->second);

    StringConverter ret = it->second;
    for (auto path_it = path.begin()+1; path_it != path.end(); ++path_it) {
        auto sub_dict = ret.rec_dict();
        it = sub_dict.find(*path_it);
        if (it == sub_dict.end())
            return std::make_pair(false, StringConverter());
        ret = it->second;
    }

    return std::make_pair(true, ret);
}

void
add_entry(Caliper* c, Channel* channel, const std::string& name, const std::string& val)
{
    Attribute attr =
        c->create_attribute(name.c_str(), CALI_TYPE_STRING,
            CALI_ATTR_UNALIGNED | CALI_ATTR_GLOBAL | CALI_ATTR_SKIP_EVENTS);

    c->set(channel, attr, Variant(CALI_TYPE_STRING, val.data(), val.size()));
}

unsigned
add_entries(Caliper* c, Channel* channel, const std::string& key, const StringConverter& val)
{
    unsigned num_entries = 0;

    std::string name("nodeinfo.");
    name.append(key);

    bool is_dict = false;
    auto dict = val.rec_dict(&is_dict);

    if (is_dict) {
        std::string prefix = name.append(".");
        for (const auto& p : dict) {
            name = prefix;
            name.append(p.first);
            add_entry(c, channel, name, p.second.to_string());
            ++num_entries;
        }
    } else {
        add_entry(c, channel, name, val.to_string());
        ++num_entries;
    }

    return num_entries;
}

void
read_nodeinfo(Caliper* c, Channel* channel, const std::vector<std::string>& keys, const std::string& filename)
{
    std::ifstream is(filename.c_str(), std::ios::ate);

    if (!is) {
        Log(1).stream() << channel->name() << ": lcnodeinfo: Cannot open "
            << filename << ", quitting\n";
        return;
    }

    auto size = is.tellg();
    std::string str(size, '\0');
    is.seekg(0);
    if (!is.read(&str[0], size)) {
        Log(0).stream() << channel->name() << ": lcnodeinfo: Cannot read "
            << filename << ", quitting\n";
        return;
    }

    if (Log::verbosity() >= 2) {
        Log(2).stream() << channel->name() << ": lcnodeinfo: "
            << size << " bytes read from " << filename << std::endl;
    }

    bool ok = false;
    auto top = StringConverter(str).rec_dict(&ok);

    if (!ok) {
        Log(0).stream() << channel->name() << ": lcnodeinfo: Cannot parse top-level dict in "
            << filename << ", quitting\n";
        return;
    }

    unsigned num_entries = 0;

    for (const std::string& key : keys) {
        std::vector<std::string> path = StringConverter(key).to_stringlist(".");
        auto ret = find_key(path, top);
        if (ret.first)
            num_entries += add_entries(c, channel, key, ret.second);
        else
            Log(1).stream() << channel->name() << ": lcnodeinfo: Key "
                << key << " not found\n";
    }

    Log(1).stream() << channel->name() << ": lcnodeinfo: Added "
        << num_entries << " entries\n";
}

void
lcnodeinfo_register(Caliper*, Channel* channel)
{
    ConfigSet cfg =
        services::init_config_from_spec(channel->config(), lcnodeinfo_service_spec);

    std::vector<std::string> keys = cfg.get("keys").to_stringlist();
    std::string filename = cfg.get("filename").to_string();

    if (keys.empty()) {
        Log(1).stream() << channel->name() << ": lcnodeinfo: "
            << "No keys provided, quitting\n";
        return;
    }

    channel->events().post_init_evt.connect([keys,filename](Caliper* c, Channel* channel){
            read_nodeinfo(c, channel, keys, filename);
        });
}

}

namespace cali
{

CaliperService lcnodeinfo_service { ::lcnodeinfo_service_spec, ::lcnodeinfo_register };

}