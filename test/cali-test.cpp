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

// A basic Caliper instrumentation demo / test file

#include <caliper/cali.h>

#include <caliper/Caliper.h>
#include <caliper/SnapshotRecord.h>

#include <caliper/common/RuntimeConfig.h>
#include <caliper/common/Variant.h>

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>


void begin_foo_op()
{
    // Begin "foo"->"fooing" and keep it alive past the end of the current C++ scope
    cali::Annotation("foo").begin("fooing");
}

void end_foo_op()
{
    // Explicitly end innermost level of "foo" annotation
    cali::Annotation("foo").end();
}

void make_hierarchy_1(cali::Experiment* exp)
{
    cali::Caliper   c;    
    cali::Attribute attr = c.create_attribute("misc.hierarchy", CALI_TYPE_STRING);

    cali::Variant   data[3] = {
        { CALI_TYPE_STRING, "h1_l0", 5 },
        { CALI_TYPE_STRING, "h1_l1", 5 },
        { CALI_TYPE_STRING, "h1_l2", 5 }
    };

    c.set_path(exp, attr, 3, data);
}

void make_hierarchy_2(cali::Experiment* exp)
{
    cali::Caliper   c;
    cali::Attribute attr = c.create_attribute("misc.hierarchy", CALI_TYPE_STRING);

    cali::Variant   data[3] = {
        { CALI_TYPE_STRING, "h2_l0", 5 },
        { CALI_TYPE_STRING, "h2_l1", 5 },
        { CALI_TYPE_STRING, "h2_l2", 5 }
    };

    c.set_path(exp, attr, 3, data);
}

void test_blob()
{
    // An annotation with a user-defined datatype

    struct my_weird_type {
        unsigned u = 42;
        char     c = 'c';
        float    f = 42.42;
    } my_weird_elem;

    cali::Annotation::Guard 
        g_mydata( cali::Annotation("mydata").set(CALI_TYPE_USR, &my_weird_elem, sizeof(my_weird_elem)) );
}

void test_annotation_copy()
{
    cali::Annotation ann("copy_ann_1");

    ann.begin("outer");

    {
        std::vector<cali::Annotation> vec;
        
        vec.push_back(ann);
        vec.push_back(cali::Annotation("copy_ann_2"));

        for (cali::Annotation& a : vec) {
            a.begin("inner");
            a.end();
        }
    }
    
    ann.end();    
}

void test_attribute_metadata()
{
    cali::Caliper   c;

    cali::Attribute meta_attr[2] = {
        c.create_attribute("meta-string", CALI_TYPE_STRING),
        c.create_attribute("meta-int",    CALI_TYPE_INT)
    };
    cali::Variant   meta_data[2] = {
        cali::Variant(CALI_TYPE_STRING, "metatest", 8),
        cali::Variant(42)
    };

    cali::Attribute attr =
        c.create_attribute("metadata-test-attr", CALI_TYPE_INT, CALI_ATTR_DEFAULT,
                           2, meta_attr, meta_data);

    c.set(attr, cali::Variant(1337));

    if (attr.get(c.get_attribute("meta-int")).to_int() != 42)
        std::cout << "Attribute metadata mismatch";
    
    c.end(attr);
}

void test_uninitialized()
{
    // Call end() on uninitialized annotation object

    cali::Annotation("cali-test.uninitialized").end();
}

void test_end_mismatch()
{
    // simulate end() stack error

    cali::Annotation a("cali-test.end-mismatch");

    a.begin(1);
    a.end();
    a.end();
}

void test_escaping()
{
    cali::Annotation w("weird\\attribute = what,?");
    w.begin("crazy \\string\\=1,2,3=");
    w.begin("=42");
    w.begin(",noattribute=novalue,");

    w.end();
    w.end();
    w.end();
}

void test_hierarchy()
{
    cali::RuntimeConfig cfg;
    
    cali::Caliper       c;
    cali::Experiment*   exp =
        c.create_experiment("hierarchy.experiment", cfg);
    
    cali::Annotation h("hierarchy");

    h.set(1);
    make_hierarchy_1(exp);
    h.set(2);
    make_hierarchy_2(exp);
    h.end();
}

void test_cross_scope()
{
    begin_foo_op();
    end_foo_op();
}

void test_attr_prop_preset()
{
    cali::Annotation::Guard
        g( cali::Annotation("test-prop-preset").set(true) );
}

void test_aggr_warnings()
{
    cali::Caliper c;
    
    // make a separate experiment for this
    const char* cfg_profile[][2] = {
        { "CALI_SERVICES_ENABLE",      "aggregate" },
        { "CALI_CALIPER_CONFIG_CHECK", "false"     },
        { nullptr, nullptr }
    };
    
    cali::RuntimeConfig cfg;

    cfg.allow_read_env(false);
    cfg.define_profile("test_aggregate_warnings", cfg_profile);
    cfg.set("CALI_CONFIG_PROFILE", "test_aggregate_warnings");
    
    cali::Experiment* exp =
        c.create_experiment("test_aggregate_warnings", cfg);
    
    // create an immediate attribute with double type: should create warning if used in aggregation key
    cali::Attribute d  = c.create_attribute("aw.dbl",   CALI_TYPE_DOUBLE, CALI_ATTR_ASVALUE);

    cali::Attribute i1 = c.create_attribute("aw.int.1", CALI_TYPE_INT,  CALI_ATTR_ASVALUE);
    cali::Attribute i2 = c.create_attribute("aw.int.2", CALI_TYPE_INT,  CALI_ATTR_ASVALUE);
    cali::Attribute i3 = c.create_attribute("aw.int.3", CALI_TYPE_INT,  CALI_ATTR_ASVALUE);
    cali::Attribute i4 = c.create_attribute("aw.int.4", CALI_TYPE_UINT, CALI_ATTR_ASVALUE);
    cali::Attribute i5 = c.create_attribute("aw.int.5", CALI_TYPE_UINT, CALI_ATTR_ASVALUE);

    uint64_t largeval = 0xFFFFFFFFFFFFFFFF;

    // make a snapshot with "-1, -2, -3" entries. this should cause the aggregation key 
    // getting too long, as negative values aren't be compressed well currently
    cali::Attribute attr[6] = { d, i1, i2, i3, i4, i5 };
    cali::Variant   data[6] = { 
        cali::Variant(1.0), 
        cali::Variant(-1), 
        cali::Variant(-2), 
        cali::Variant(-3),
        cali::Variant(CALI_TYPE_UINT, &largeval, sizeof(uint64_t)), 
        cali::Variant(CALI_TYPE_UINT, &largeval, sizeof(uint64_t))
    };

    cali::SnapshotRecord::FixedSnapshotRecord<16> info_data;
    cali::SnapshotRecord info(info_data);

    c.make_entrylist(6, attr, data, info);
    c.push_snapshot(exp, CALI_SCOPE_THREAD | CALI_SCOPE_PROCESS, &info);
}

std::ostream& print_padded(std::ostream& os, const char* string, int fieldlen)
{
    const char* whitespace =
        "                                        "
        "                                        "
        "                                        "; // 120 spaces

    int slen = strlen(string);

    os << string;

    if (slen < fieldlen)
        os << whitespace + (120 - std::min(120, fieldlen-slen));
    
    return os;
}

void test_instance()
{
    if (cali::Caliper::is_initialized() == true) {
        std::cout << "cali-test: Caliper::is_initialized() failed uninitialized condition"
                  << std::endl;
        return;
    }
    if (cali_is_initialized() != 0) {
        std::cout << "cali-test: cali_is_initialized() failed uninitialized condition "
                  << std::endl;
        return;
    }
    
    cali_init();

    if (cali::Caliper::is_initialized() == false) {
        std::cout << "cali-test: Caliper::is_initialized() failed initialized condition"
                  << std::endl;
        return;
    }
    if (cali_is_initialized() == 0) {
        std::cout << "cali-test: cali_is_initialized() failed initialized condition "
                  << std::endl;
        return;
    }

    std::cout << "Caliper instance test OK" << std::endl;
}

void test_config_after_init()
{
    cali_config_set("CALI_SERVICES_ENABLE", "debug");
}

int main(int argc, char* argv[])
{
    // instance test has to run before Caliper initialization

    test_instance();
    
    cali_config_preset("CALI_CALIPER_ATTRIBUTE_PROPERTIES", "test-prop-preset=asvalue:process_scope");

    CALI_CXX_MARK_FUNCTION;
        
    const struct testcase_info_t {
        const char*  name;
        void        (*fn)();
    } testcases[] = {
        { "blob",                     test_blob               },
        { "annotation-copy",          test_annotation_copy    },
        { "attribute-metadata",       test_attribute_metadata },
        { "uninitialized-annotation", test_uninitialized      },
        { "end-mismatch",             test_end_mismatch       },
        { "escaping",                 test_escaping           },
        { "aggr-warnings",            test_aggr_warnings      },
        { "cross-scope",              test_cross_scope        },
        { "attribute-prop-preset",    test_attr_prop_preset   },
        { "config-after-init",        test_config_after_init  },
        { 0, 0 }
    };
    
    {
        cali::Annotation::Guard
            g( cali::Annotation("cali-test").begin("checking") );

        // check for missing/misspelled command line test cases
        for (int a = 1; a < argc; ++a) {
            const testcase_info_t* t = testcases;
        
            for ( ; t->name && 0 != strcmp(t->name, argv[a]); ++t)
                ;

            if (!t->name)
                std::cerr << "test \"" << argv[a] << "\" not found" << std::endl;
        }
    }
    
    cali::Annotation::Guard
        g( cali::Annotation("cali-test").begin("testing") );
    
    for (const testcase_info_t* t = testcases; t->fn; ++t) {
        if (argc > 1) {
            int a = 1;
            
            for ( ; a < argc && 0 != strcmp(t->name, argv[a]); ++a)
                ;
            
            if (a == argc)
                continue;
        }

        cali::Annotation::Guard
            g( cali::Annotation("cali-test.test").begin(t->name) );

        print_padded(std::cout, t->name, 28) << " ... ";
        (*(t->fn))();
        std::cout << "done" << std::endl;
    }
}
