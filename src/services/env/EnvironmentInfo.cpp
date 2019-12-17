// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// EnvironmentInfo.cpp
// A Caliper service that collects various environment information

#include "caliper/CaliperService.h"

#include "caliper/Caliper.h"

#include "caliper/common/Log.h"
#include "caliper/common/RuntimeConfig.h"
#include "caliper/common/Variant.h"

#include <sys/utsname.h>
#include <time.h>
#include <unistd.h>

#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace cali;
using namespace std;

namespace
{

static const ConfigSet::Entry s_configdata[] = {
    { "extra", CALI_TYPE_STRING, "",
      "List of environment variables to add to the Caliper blackboard",
      "List of environment variables to add to the Caliper blackboard"
    },
    ConfigSet::Terminator
};

void read_cmdline(Caliper* c, Channel* chn, ConfigSet& config)
{
    Attribute cmdline_attr = 
        c->create_attribute("env.cmdline", CALI_TYPE_STRING, CALI_ATTR_GLOBAL);

    // Attempt to open /proc/self/cmdline and read it

    ifstream fs("/proc/self/cmdline");

    std::vector<Variant> args;

    for (std::string arg; std::getline(fs, arg, static_cast<char>(0)); )
        c->begin(cmdline_attr, Variant(CALI_TYPE_STRING, arg.data(), arg.size()));
}
    
void read_uname(Caliper* c, Channel* chn, ConfigSet& config)
{
    struct utsname u;

    if (uname(&u) == 0) {
        const struct uname_attr_info_t { 
            const char* attr_name;
            const char* u_val;
        } uname_attr_info[] = {
            { "env.os.sysname", u.sysname },
            { "env.os.release", u.release },
            { "env.os.version", u.version },
            { "env.machine",    u.machine }
        };

        for (const uname_attr_info_t& uinfo : uname_attr_info) {
            Attribute attr = 
                c->create_attribute(uinfo.attr_name, CALI_TYPE_STRING, CALI_ATTR_GLOBAL);

            c->set(attr, Variant(CALI_TYPE_STRING, uinfo.u_val, strlen(uinfo.u_val)));
        }
    }
}

void read_time(Caliper* c, Channel* chn, ConfigSet& config)
{
    Attribute starttime_attr = 
        c->create_attribute("env.starttime", CALI_TYPE_STRING, CALI_ATTR_GLOBAL);

    time_t       t = time(NULL);
    struct tm *tmp = localtime(&t);

    if (!tmp)
        return;

    char   buf[64];
    size_t len = strftime(buf, sizeof(buf)-1, "%a %d %b %Y %H:%M:%S %z", tmp);

    c->set(starttime_attr, Variant(CALI_TYPE_STRING, buf, len));
}

void read_hostname(Caliper* c, Channel* chn, ConfigSet& config)
{
    Attribute hostname_attr =
        c->create_attribute("env.hostname", CALI_TYPE_STRING, CALI_ATTR_SCOPE_PROCESS);

    char buf[256];

    if (gethostname(buf, sizeof(buf)-1) == 0)
        c->set(hostname_attr, Variant(CALI_TYPE_STRING, buf, strlen(buf)));
}

void read_extra(Caliper* c, Channel* chn, ConfigSet& config)
{
    vector<string> extra_list = config.get("extra").to_stringlist(",:");

    for (string& env : extra_list) {
        if (env.empty())
            continue;
        
        Attribute attr =
            c->create_attribute("env."+env, CALI_TYPE_STRING, CALI_ATTR_GLOBAL);
        
        char* val = getenv(env.c_str());

        if (val)
            c->set(attr, Variant(CALI_TYPE_STRING, val, strlen(val)));
    }
}
  
void environment_service_register(Caliper* c, Channel* chn)
{
    Log(1).stream() << chn->name() << ": Registered env service." << std::endl;

    ConfigSet config = chn->config().init("env", s_configdata);

    read_cmdline(c, chn, config);
    read_uname(c, chn, config);
    read_time(c, chn, config);
    read_hostname(c, chn, config);
    read_extra(c, chn, config);
}
  
}

namespace cali
{

CaliperService env_service = { "env", ::environment_service_register };

} // namespace cali
