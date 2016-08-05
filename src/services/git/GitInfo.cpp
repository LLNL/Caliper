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

///@file  EnvironmentInfo.cpp
///@brief A Caliper service that collects various git information

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
#include <string>
#include <iostream>
#include <cstdio>
#include <memory>

using namespace cali;
using namespace std;

namespace
{
std::string deleteFrom(std::string in, std::string replaceThese){ //Dear C++ STL committee
     for(auto charIter = in.begin(); charIter!=in.end(); charIter++){
        if(replaceThese.find(*charIter)!=string::npos){
            in.replace(charIter,charIter+1," ");
        }
     }
     return in;
} //Are you kidding me? We don't have this function anywhere in namespace std?
std::string makeCaliSafe(std::string in){
    return deleteFrom(in, "=\n\r,");
}
//via: http://stackoverflow.com/questions/478898/how-to-execute-a-command-and-get-output-of-command-within-c
std::string shell_exec(std::string scmd) {
    const char* cmd = scmd.c_str();
    std::shared_ptr<FILE> pipe(popen(cmd, "r"), pclose);
    if (!pipe) return "ERROR";
    char buffer[128];
    std::string result = "";
    while (!feof(pipe.get())) {
        if (fgets(buffer, 128, pipe.get()) != NULL){
            result += buffer;
        }
    }
    return result;
}
static const ConfigSet::Entry s_configdata[] = {
    { "repo_dir", CALI_TYPE_STRING, "",
      "Directory containing the version control repository containing this software",
      "Directory containing the version control repository containing this software"
    },
    ConfigSet::Terminator
};

Attribute annot_git_dir;
Attribute annot_git_hash;
Attribute annot_git_message;
Attribute annot_git_name;
Attribute annot_git_date;

ConfigSet config;
  
void read_gitdir(Caliper* c){
    annot_git_dir = c->create_attribute("Git.Directory", CALI_TYPE_STRING,CALI_ATTR_SCOPE_PROCESS);
    annot_git_hash= c->create_attribute("Git.Hash", CALI_TYPE_STRING,CALI_ATTR_SCOPE_PROCESS);
    annot_git_message= c->create_attribute("Git.Message", CALI_TYPE_STRING,CALI_ATTR_SCOPE_PROCESS);
    annot_git_name= c->create_attribute("Git.Commiter", CALI_TYPE_STRING,CALI_ATTR_SCOPE_PROCESS);
    annot_git_date= c->create_attribute("Git.Date", CALI_TYPE_STRING,CALI_ATTR_SCOPE_PROCESS);
    string gitDirectory = makeCaliSafe(config.get("repo_dir").to_string());
    string hash = makeCaliSafe(shell_exec("git --git-dir="+gitDirectory+"/.git log -1 --pretty=%H"));
    string author_name = makeCaliSafe(shell_exec("git --git-dir="+gitDirectory+"/.git log -1 --pretty=%an"));
    string date = makeCaliSafe(shell_exec("git --git-dir="+gitDirectory+"/.git log -1 --pretty=%ad"));
    string message = makeCaliSafe(shell_exec("git --git-dir="+gitDirectory+"/.git log -1 --pretty=%s"));
    c->begin(annot_git_dir,cali::Variant(CALI_TYPE_STRING,gitDirectory.c_str(),gitDirectory.length()));
    c->begin(annot_git_hash,cali::Variant(CALI_TYPE_STRING,hash.c_str(),hash.length()));
    c->begin(annot_git_message,cali::Variant(CALI_TYPE_STRING,message.c_str(),message.length()));
    c->begin(annot_git_name,cali::Variant(CALI_TYPE_STRING,author_name.c_str(),author_name.length()));
    c->begin(annot_git_date,cali::Variant(CALI_TYPE_STRING,date.c_str(),date.length()));
    c->end(annot_git_date);
    c->end(annot_git_name);
    c->end(annot_git_message);
    c->end(annot_git_hash);
    c->end(annot_git_dir);
    //c->begin(annot_git_dir,cali::Variant(CALI_TYPE_STRING,gitDirectory.c_str(),gitDirectory.length()));
    //c->set(annot_git_hash,cali::Variant(CALI_TYPE_STRING,hash.c_str(),hash.length()));
    //c->set(annot_git_message,cali::Variant(CALI_TYPE_STRING,message.c_str(),message.length()));
    //c->set(annot_git_name,cali::Variant(CALI_TYPE_STRING,author_name.c_str(),author_name.length()));
    //c->set(annot_git_date,cali::Variant(CALI_TYPE_STRING,date.c_str(),date.length()));
    //c->end(annot_git_dir);
}

void git_service_register(Caliper* c)
{
    Log(1).stream() << "Registered git service" << endl;
    Log(1).stream() << "Collecting git information" << endl;

    config = RuntimeConfig::init("git", s_configdata);

    read_gitdir(c);
}
  
}

namespace cali
{
    CaliperService git_service = { "git", ::git_service_register };
} // namespace cali
 
