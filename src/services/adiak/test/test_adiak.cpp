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

