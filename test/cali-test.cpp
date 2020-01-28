// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// This file contains tests for some more esoteric Caliper quirks and features

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

void test_blob()
{
    // An annotation with a user-defined datatype

    struct my_weird_type {
        unsigned u = 42;
        char     c = 'c';
        float    f = 42.42;
    } my_weird_elem;

    cali::Annotation::Guard
        g_mydata( cali::Annotation("mydata").begin(CALI_TYPE_USR, &my_weird_elem, sizeof(my_weird_elem)) );
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

    // create an immediate attribute with double type: should create warning if used in aggregation key
    cali::Attribute d  = c.create_attribute("aw.dbl",   CALI_TYPE_DOUBLE, CALI_ATTR_ASVALUE);

    cali::Attribute i1 = c.create_attribute("aw.int.1", CALI_TYPE_INT,  CALI_ATTR_ASVALUE);
    cali::Attribute i2 = c.create_attribute("aw.int.2", CALI_TYPE_INT,  CALI_ATTR_ASVALUE);
    cali::Attribute i3 = c.create_attribute("aw.int.3", CALI_TYPE_INT,  CALI_ATTR_ASVALUE);
    cali::Attribute i4 = c.create_attribute("aw.int.4", CALI_TYPE_UINT, CALI_ATTR_ASVALUE);
    cali::Attribute i5 = c.create_attribute("aw.int.5", CALI_TYPE_UINT, CALI_ATTR_ASVALUE);

    uint64_t largeval = 0xFFFFFFFFFFFFFFFF;

    //   make a snapshot with "-1, -2, -3" entries. this should cause the aggregation key
    // getting too long, as negative values aren't be compressed well currently

    cali_id_t attr[6] = {
        d.id(),  i1.id(),   i2.id(),
        i3.id(), i4.id(),   i5.id()
    };
    cali_variant_t data[6] = {
        cali_make_variant_from_double(1.0),
        cali_make_variant_from_int(-1),
        cali_make_variant_from_int(-2),
        cali_make_variant_from_int(-3),
        cali_make_variant_from_uint(largeval),
        cali_make_variant_from_uint(largeval)
    };

    cali_id_t chn_id =
        cali::create_channel("test_aggregate_warnings", 0, {
                { "CALI_SERVICES_ENABLE",      "aggregate" },
                { "CALI_AGGREGATE_KEY",        "function,aw.dbl,aw.int.1,aw.int.2,aw.int.3,aw.int.4,aw.int.5" },
                { "CALI_CHANNEL_CONFIG_CHECK", "false"     }
            });

    cali_channel_push_snapshot(chn_id, CALI_SCOPE_THREAD | CALI_SCOPE_PROCESS,
                               6, attr, data);

    cali_delete_channel(chn_id);
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

void test_nesting_error()
{
    cali::Annotation a("test.nesting-error.a", CALI_ATTR_NESTED);
    cali::Annotation b("test.nesting-error.b", CALI_ATTR_NESTED);

    a.begin(11);
    b.begin(22);
    a.end();
    b.end();
}

void test_unclosed_region()
{
    cali::Annotation a("test.unclosed_region", CALI_ATTR_DEFAULT);

    a.begin(101);
    a.begin(202);
    a.end();
}

int main(int argc, char* argv[])
{
    cali_config_preset("CALI_CALIPER_ATTRIBUTE_PROPERTIES", "test-prop-preset=asvalue:process_scope");

    // instance test has to run before Caliper initialization

    test_instance();

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
        { "nesting-error",            test_nesting_error      },
        { "unclosed-region",          test_unclosed_region    },
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
