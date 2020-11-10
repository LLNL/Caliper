// Tests for Adiak import/export services

#include "caliper/cali.h"
#include "caliper/Caliper.h"

#include <gtest/gtest.h>

#include <adiak.hpp>
#include <adiak_tool.h>

using namespace cali;

namespace
{

void extract_cb(const char* name, adiak_category_t cat, const char* subcategory, adiak_value_t* val, adiak_datatype_t* t, void* usr_val) {
    std::map<std::string, std::string> *res = static_cast< std::map<std::string, std::string>* >(usr_val);

    switch (t->dtype) {
    case adiak_int:
        (*res)[name] = std::to_string(val->v_int);
        break;
    case adiak_string:
        (*res)[name] = std::string(static_cast<const char*>(val->v_ptr));
        break;
    default:
        break;
    }
}

}

TEST(AdiakServiceTest, AdiakImport)
{
    cali_id_t import_chn_id =
        cali::create_channel("adiak_import", 0, {
                { "CALI_SERVICES_ENABLE", "adiak_import" }
            });

    adiak::value("import.int", 42);
    adiak::value("import.str", "import");

    std::vector<int> vec { 1, 4, 16 };
    adiak::value("import.vec", vec);

    Caliper  c;
    Channel* chn = c.get_channel(import_chn_id);

    ASSERT_NE(chn, nullptr);

    chn->events().pre_flush_evt(&c, chn, nullptr);

    Attribute int_attr = c.get_attribute("import.int");
    Attribute str_attr = c.get_attribute("import.str");
    Attribute vec_attr = c.get_attribute("import.vec");

    EXPECT_EQ(int_attr.type(), CALI_TYPE_INT);
    EXPECT_EQ(str_attr.type(), CALI_TYPE_STRING);
    EXPECT_EQ(vec_attr.type(), CALI_TYPE_STRING);

    EXPECT_TRUE(int_attr.is_global());

    Attribute adk_type_attr = c.get_attribute("adiak.type");

    EXPECT_EQ(int_attr.get(adk_type_attr).to_string(), std::string("int"));
    EXPECT_EQ(str_attr.get(adk_type_attr).to_string(), std::string("string"));
    EXPECT_EQ(vec_attr.get(adk_type_attr).to_string(), std::string("list of int"));

    EXPECT_EQ(c.get(chn, int_attr).value().to_int(), 42);
    EXPECT_EQ(c.get(chn, str_attr).value().to_string(), std::string("import"));
    EXPECT_EQ(c.get(chn, vec_attr).value().to_string(), std::string("{1,4,16}"));
}


#ifdef ADIAK_HAVE_LONGLONG
TEST(AdiakServiceTest, AdiakImportLonglong)
{
    cali_id_t import_chn_id =
        cali::create_channel("adiak_import", 0, {
                { "CALI_SERVICES_ENABLE", "adiak_import" }
            });

    long long llv = -9876543210;

    adiak::value("import.i64", llv);

    unsigned long long ullv = 0xFFFFFFFFFFAA;

    std::vector<unsigned long long> vec { 1, 4, ullv };
    adiak::value("import.vec", vec);

    Caliper  c;
    Channel* chn = c.get_channel(import_chn_id);

    ASSERT_NE(chn, nullptr);

    chn->events().pre_flush_evt(&c, chn, nullptr);

    Attribute i64_attr = c.get_attribute("import.i64");
    Attribute vec_attr = c.get_attribute("import.vec");

    EXPECT_EQ(i64_attr.type(), CALI_TYPE_INT);
    EXPECT_EQ(vec_attr.type(), CALI_TYPE_STRING);

    EXPECT_TRUE(i64_attr.is_global());

    Attribute adk_type_attr = c.get_attribute("adiak.type");

    EXPECT_EQ(i64_attr.get(adk_type_attr).to_string(), std::string("long long"));
    EXPECT_EQ(vec_attr.get(adk_type_attr).to_string(), std::string("list of int"));

    EXPECT_EQ(c.get(chn, i64_attr).value().to_int64(), llv);
    EXPECT_EQ(c.get(chn, vec_attr).value().to_string(), std::string("{1,4,281474976710570}"));
}
#endif

TEST(AdiakServiceTest, AdiakImportCategoryFilter)
{
    cali_id_t import_chn_id =
        cali::create_channel("adiak_import", 0, {
                { "CALI_SERVICES_ENABLE",         "adiak_import" },
                { "CALI_ADIAK_IMPORT_CATEGORIES", "424242,12345" }
            });

    adiak_namevalue("do.not.import", adiak_general, "none", "%d", 23);
    adiak_namevalue("do.import.1", static_cast<adiak_category_t>(424242), "import.category", "%d", 42);
    adiak_namevalue("do.import.2", static_cast<adiak_category_t>(12345), "import.category", "%s", "hi");

    Caliper  c;
    Channel* chn = c.get_channel(import_chn_id);

    ASSERT_NE(chn, nullptr);

    chn->events().pre_flush_evt(&c, chn, nullptr);

    Attribute do_import_attr_1 = c.get_attribute("do.import.1");
    Attribute do_import_attr_2 = c.get_attribute("do.import.2");
    Attribute do_not_import_attr = c.get_attribute("do.not.import");

    EXPECT_EQ(do_import_attr_1.type(), CALI_TYPE_INT);
    EXPECT_EQ(do_import_attr_2.type(), CALI_TYPE_STRING);
    EXPECT_EQ(do_not_import_attr, Attribute::invalid);

    EXPECT_TRUE(do_import_attr_1.is_global());
    EXPECT_TRUE(do_import_attr_2.is_global());

    Attribute adk_type_attr = c.get_attribute("adiak.type");
    Attribute adk_caty_attr = c.get_attribute("adiak.category");
    Attribute adk_scat_attr = c.get_attribute("adiak.subcategory");

    EXPECT_EQ(do_import_attr_1.get(adk_type_attr).to_string(), std::string("int"));
    EXPECT_EQ(do_import_attr_1.get(adk_caty_attr).to_int(), 424242);
    EXPECT_EQ(do_import_attr_1.get(adk_scat_attr).to_string(), std::string("import.category"));

    EXPECT_EQ(c.get(chn, do_import_attr_1).value().to_int(), 42);
    EXPECT_EQ(c.get(chn, do_import_attr_2).value().to_string(), std::string("hi"));
}

TEST(AdiakServiceTest, AdiakExport)
{
    cali_id_t export_chn_id =
        cali::create_channel("adiak_export", 0, {
                { "CALI_SERVICES_ENABLE", "adiak_export" }
            });

    cali_set_global_int_byname("export.int", 42);
    cali_set_global_string_byname("export.str", "export");

    Caliper  c;
    Channel* chn = c.get_channel(export_chn_id);

    ASSERT_NE(chn, nullptr);

    chn->events().pre_flush_evt(&c, chn, nullptr);

    std::map<std::string, std::string> res;

    adiak_list_namevals(1, adiak_category_all, ::extract_cb, &res);

    EXPECT_STREQ(res["export.int"].c_str(), "42");
    EXPECT_STREQ(res["export.str"].c_str(), "export");
}

