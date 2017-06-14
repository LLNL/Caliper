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

/// \file Cupti.cpp
/// \brief Implementation of Cupti service

#include "../CaliperService.h"

#include <Caliper.h>

#include <Log.h>
#include <RuntimeConfig.h>

#include <util/split.hpp>

#include <cupti.h>

#include <iterator>
#include <vector>

namespace cali
{
    extern Attribute class_nested_attr;
}

using namespace cali;

namespace
{
    //
    // --- Data
    //

    ConfigSet              config;
    const ConfigSet::Entry configdata[] = {
        { "callback_domains", CALI_TYPE_STRING, "runtime:driver",
          "List of CUDA callback domains to capture",
          "List of CUDA callback domains to capture. Possible values:\n"
          "  runtime :  Capture CUDA runtime API calls\n"
          "  driver  :  Capture CUDA driver calls\n"
          "  none    :  Don't capture callbacks"
        },
        { "record_symbol", CALI_TYPE_BOOL, "true",
          "Record symbol name (kernel) for CUPTI callbacks",
          "Record symbol name (kernel) for CUPTI callbacks"
        },
        { "record_context", CALI_TYPE_BOOL, "true",
          "Record CUDA context ID",
          "Record CUDA context ID"
        },
        ConfigSet::Terminator
    };

    const struct CallbackDomainInfo {
        CUpti_CallbackDomain domain;
        const char* name;
    } callback_domains[] = {
        { CUPTI_CB_DOMAIN_RUNTIME_API, "runtime"  },
        { CUPTI_CB_DOMAIN_DRIVER_API,  "driver"   },
        { CUPTI_CB_DOMAIN_INVALID,     "none"     },
        { CUPTI_CB_DOMAIN_INVALID,     0          }
    };

    struct CuptiServiceInfo {
        Attribute runtime_attr;
        Attribute driver_attr;
        Attribute context_attr;
        Attribute symbol_attr;

        bool      record_context;
        bool      record_symbol;
    }                      cupti_info;

    CUpti_SubscriberHandle subscriber;

    unsigned               cb_count;

    //
    // --- Helper functions
    // 

    void
    print_cupti_error(std::ostream& os, CUptiResult err, const char* func)
    {
        const char* errstr;

        cuptiGetResultString(err, &errstr);
        
        os << "cupti: " << func << ": error: " << errstr << std::endl;
    }

    // 
    // --- CUPTI Callback handling
    // 

    void CUPTIAPI
    cupti_callback(void* userdata,
                   CUpti_CallbackDomain domain,
                   CUpti_CallbackId     cbid,
                   const CUpti_CallbackData* cbInfo)
    {
        ++cb_count;

        Caliper   c;
        Attribute attr;

        switch (domain) {
        case CUPTI_CB_DOMAIN_RUNTIME_API:
            attr = cupti_info.runtime_attr;
            break;
        case CUPTI_CB_DOMAIN_DRIVER_API:
            attr = cupti_info.driver_attr;
            break;
        default:
            return;
        }

        if (cupti_info.record_context) {
            uint64_t ctx = cbInfo->contextUid;
            Entry    e   = c.get(cupti_info.context_attr);

            if (e.is_empty() || e.value().to_uint() != ctx)
                c.set(cupti_info.context_attr, Variant(ctx));
        }

        if (cbInfo->callbackSite == CUPTI_API_ENTER) {
            if (cupti_info.record_symbol && cbInfo->symbolName) {
                Variant v_sname(CALI_TYPE_STRING, cbInfo->symbolName, strlen(cbInfo->symbolName));
                c.set(cupti_info.symbol_attr, v_sname);
            }

            Variant v_fname(CALI_TYPE_STRING, cbInfo->functionName, strlen(cbInfo->functionName));
            c.begin(attr, v_fname);
        } else if (cbInfo->callbackSite == CUPTI_API_EXIT) {
            c.end(attr);
            
            if (cupti_info.record_symbol && cbInfo->symbolName)
                c.end(cupti_info.symbol_attr);
        }
    }

    void
    finish_cb(Caliper* c) 
    {
        Log(2).stream() << "cupti: processed " << cb_count << " CUDA API callbacks" << std::endl;

        cuptiUnsubscribe(subscriber);
    }

    void 
    post_init_cb(Caliper* c)
    {
        Variant v_true(true);

        cupti_info.runtime_attr = 
            c->create_attribute("cupti.runtimeAPI", CALI_TYPE_STRING, CALI_ATTR_DEFAULT,
                                1, &class_nested_attr, &v_true);
        cupti_info.driver_attr =
            c->create_attribute("cupti.driverAPI", CALI_TYPE_STRING, CALI_ATTR_DEFAULT,
                                1, &class_nested_attr, &v_true);
        
        cupti_info.context_attr = 
            c->create_attribute("cupti.contextID", CALI_TYPE_UINT, CALI_ATTR_SKIP_EVENTS);
        cupti_info.symbol_attr = 
            c->create_attribute("cupti.symbolName", CALI_TYPE_STRING, CALI_ATTR_SKIP_EVENTS);

        cupti_info.record_context = config.get("record_context").to_bool();
        cupti_info.record_symbol = config.get("record_symbol").to_bool();
    }

    bool 
    register_callback_domains() 
    {
        CUptiResult res = 
            cuptiSubscribe(&subscriber, 
                           (CUpti_CallbackFunc) cupti_callback, 
                           &cupti_info);

        if (res != CUPTI_SUCCESS) {
            print_cupti_error(Log(0).stream(), res, "cuptiSubscribe");
            return false;
        }

        std::vector<std::string> cb_domain_names;

        util::split(config.get("callback_domains").to_string(), ':', 
                    std::back_inserter(cb_domain_names));

        for (const std::string& s : cb_domain_names) {
            const CallbackDomainInfo* cbinfo = callback_domains;

            for ( ; cbinfo->name && s != cbinfo->name; ++cbinfo)
                ;

            if (!cbinfo->name) {
                Log(0).stream() << "cupti: warning: Unknown callback domain \"" 
                                << s << "\"" << std::endl;
                continue;
            }

            if (cbinfo->domain != CUPTI_CB_DOMAIN_INVALID) {
                res = cuptiEnableDomain(1, subscriber, cbinfo->domain);

                if (res != CUPTI_SUCCESS) {
                    print_cupti_error(Log(0).stream(), res, "cuptiEnableDomain");
                    return false;
                }
            }
        }

        return true;
    }

    void 
    cuptiservice_initialize(Caliper* c) 
    {
        config = RuntimeConfig::init("cupti", configdata);
        cb_count = 0;

        if (!register_callback_domains())
            return;

        c->events().post_init_evt.connect(&post_init_cb);
        c->events().finish_evt.connect(&finish_cb);

        Log(1).stream() << "Registered cupti service" << std::endl;
    }

} // namespace 

namespace cali
{
    CaliperService cupti_service = { "cupti", ::cuptiservice_initialize };
}
