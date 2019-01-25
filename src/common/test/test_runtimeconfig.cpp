#include "caliper/common/RuntimeConfig.h"

#include "gtest/gtest.h"

using namespace cali;

namespace
{

const ConfigSet::Entry test_configdata[] = { 
    { "string_val",  CALI_TYPE_STRING, "string-default",
      "Description for the string config entry",
      "Long description for the string config entry"
    },
    { "list_val",    CALI_TYPE_STRING, "first, second, \"third,but not fourth\"",
      "Description of the list config entry",
      "Long description for the list config entry"
    },
    { "int_val",     CALI_TYPE_INT,    "1337",
      "Description for the int config entry",
      "Long description for the int config entry"
    },
    { "another_int", CALI_TYPE_INT,    "4242",
      "Description for another int config entry",
      "Long description for another int config entry"
    },
    
    ConfigSet::Terminator
};

} // namespace [anonymous]


TEST(RuntimeConfigTest, ConfigFile) {
    RuntimeConfig cfg;
    
    cfg.set("CALI_CONFIG_FILE", "caliper-common_test.config");
    
    cfg.preset("CALI_TEST_STRING_VAL", "wrong value!");
    cfg.set("CALI_TEST_INT_VAL", "42");
    
    ConfigSet config = cfg.init("test", ::test_configdata);

    EXPECT_EQ(config.get("string_val").to_string(), std::string("profile1 string from file"));
    EXPECT_EQ(config.get("int_val").to_int(), 42);
    EXPECT_EQ(config.get("another_int").to_int(), 4242);
}

TEST(RuntimeConfigTest, ConfigFileProfile2) {
    RuntimeConfig cfg;
    
    cfg.preset("CALI_CONFIG_FILE", "caliper-common_test.config");
    cfg.set("CALI_CONFIG_PROFILE", "file-profile2");
    
    ConfigSet config = cfg.init("test", ::test_configdata);

    EXPECT_EQ(config.get("string_val").to_string(), std::string("string-default"));
    EXPECT_EQ(config.get("int_val").to_int(), 42);
}
