// Copyright (c) 2015, Lawrence Livermore National Security, LLC.  
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

///@file  CMakeInfo.cpp
///@brief A Caliper service that collects various cmake information

#include "../CaliperService.h"

#include <Caliper.h>

#include <Log.h>
#include <RuntimeConfig.h>
#include <Variant.h>

#include <util/split.hpp>

#include <sys/utsname.h>
#include <time.h>
#include <unistd.h>

#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

using namespace cali;
using namespace std;

namespace
{

static const ConfigSet::Entry s_configdata[] = {
    { "build_directory", CALI_TYPE_STRING, "",
      "Directory in which CMakeLists.txt can be found",
      "Directory in which CMakeLists.txt can be found"
    },
    ConfigSet::Terminator
};

ConfigSet config;


void read_cmdline(Caliper* c)
{
    Attribute cmdline_attr = 
        c->create_attribute("env.cmdline", CALI_TYPE_STRING, CALI_ATTR_SCOPE_PROCESS);

    // Attempt to open /proc/self/cmdline and read it

    ifstream fs("/proc/self/cmdline");

    std::vector<Variant> args;

    for (std::string arg; std::getline(fs, arg, static_cast<char>(0)); )
        c->begin(cmdline_attr, Variant(CALI_TYPE_STRING, arg.data(), arg.size()));
}
    
void read_uname(Caliper* c)
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
                c->create_attribute(uinfo.attr_name, CALI_TYPE_STRING, CALI_ATTR_SCOPE_PROCESS);

            c->set(attr, Variant(CALI_TYPE_STRING, uinfo.u_val, strlen(uinfo.u_val)));
        }
    }
}

void processCMakeListLine(Caliper* c, std::string arg){
    if( (arg.length()==0)||(arg[0]=='#') || arg[0]=='/') return;  
    std::vector<std::string> segment_list; 
    util::split(arg, '=', back_inserter(segment_list));
    Attribute attr = c->create_attribute(segment_list[0], CALI_TYPE_STRING, CALI_ATTR_SCOPE_PROCESS);
    for(auto curval = segment_list.begin()+1; curval != segment_list.end(); curval++){
        c->set(attr, Variant(CALI_TYPE_STRING, curval->c_str(), curval->length()));
    }
    return;

}

void read_cmakelists(Caliper* c)
{
    string build_directory = config.get("build_directory").to_string();
    string build_cache = build_directory + "/CMakeCache.txt";
    ifstream cml(build_cache.c_str());

   // util::split(extra, ':', back_inserter(extra_list));
   for (std::string arg; std::getline(cml, arg); )
       processCMakeListLine(c,arg);
/**
    for (string& env : extra_list) {
        if (env.empty())
            continue;
        
        Attribute attr =
            c->create_attribute("env."+env, CALI_TYPE_STRING, CALI_ATTR_SCOPE_PROCESS);
        
        char* val = getenv(env.c_str());

        if (val)
            c->set(attr, Variant(CALI_TYPE_STRING, val, strlen(val)));
    }
*/
}
  
void cmake_service_register(Caliper* c)
{
    Log(1).stream() << "Registered env service" << endl;
    Log(1).stream() << "Collecting cmake information" << endl;

    config = RuntimeConfig::init("cmake", s_configdata);

    read_cmakelists(c);
}
  
}

namespace cali
{
    CaliperService CMakeInfoService = { "cmake", ::cmake_service_register };
} // namespace cali
 
