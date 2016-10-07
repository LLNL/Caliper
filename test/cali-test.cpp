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

#include <Annotation.h>
#include <Caliper.h>

#include <cali_macros.h>

#include <RuntimeConfig.h>
#include <Variant.h>

#include <cstdlib>
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

void make_hierarchy_1()
{
    cali::Caliper   c;    
    cali::Attribute attr = c.create_attribute("misc.hierarchy", CALI_TYPE_STRING);

    cali::Variant   data[3] = {
        { CALI_TYPE_STRING, "h1_l0", 5 },
        { CALI_TYPE_STRING, "h1_l1", 5 },
        { CALI_TYPE_STRING, "h1_l2", 5 }
    };

    c.set_path(attr, 3, data);
}

void make_hierarchy_2()
{
    cali::Caliper   c;
    cali::Attribute attr = c.create_attribute("misc.hierarchy", CALI_TYPE_STRING);

    cali::Variant   data[3] = {
        { CALI_TYPE_STRING, "h2_l0", 5 },
        { CALI_TYPE_STRING, "h2_l1", 5 },
        { CALI_TYPE_STRING, "h2_l2", 5 }
    };

    c.set_path(attr, 3, data);
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
    cali::Annotation h("hierarchy");

    h.set(1);
    make_hierarchy_1();
    h.set(2);
    make_hierarchy_2();
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

int main(int argc, char* argv[])
{
    cali::RuntimeConfig::preset("CALI_CALIPER_ATTRIBUTE_PROPERTIES", "test-prop-preset=asvalue:process_scope");

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
        { "cross-scope",              test_cross_scope        },
        { "attribute-prop-preset",    test_attr_prop_preset   },
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
