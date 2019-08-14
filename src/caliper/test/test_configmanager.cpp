#include "caliper/ConfigManager.h"

#include "caliper/ChannelController.h"

#include <gtest/gtest.h>

#include <algorithm>

using namespace cali;

TEST(ConfigManagerTest, ParseErrors) {
    {
        cali::ConfigManager mgr;

        EXPECT_FALSE(mgr.add("foo"));
        EXPECT_TRUE(mgr.error());
        EXPECT_STREQ(mgr.error_msg().c_str(), "Unknown config or parameter: foo");
    }
    
    {
        cali::ConfigManager mgr("  event-trace(foo  = bar)");

        EXPECT_TRUE(mgr.error());
        EXPECT_STREQ(mgr.error_msg().c_str(), "Unknown argument: foo");

        auto list = mgr.get_all_channels();

        EXPECT_EQ(list.size(), 0);
    }

    {
        cali::ConfigManager mgr(" runtime-report(output=)");
        
        EXPECT_TRUE(mgr.error());
        EXPECT_STREQ(mgr.error_msg().c_str(), "Expected value after \"output=\"");
    }

    {
        cali::ConfigManager mgr("event-trace(output=stdout");

        EXPECT_TRUE(mgr.error());
        EXPECT_STREQ(mgr.error_msg().c_str(), "Expected ')'");
    }

    EXPECT_STREQ(cali::ConfigManager::check_config_string("foo").c_str(),
                 "Unknown config or parameter: foo");
    EXPECT_STREQ(cali::ConfigManager::check_config_string("event-trace,").c_str(),
                 "Unknown config or parameter: ");
}

TEST(ConfigManagerTest, ParseConfig) {
    {
        cali::ConfigManager mgr;

        EXPECT_TRUE(mgr.add("runtime-report"));
        EXPECT_FALSE(mgr.error());

        auto list = mgr.get_all_channels();

        ASSERT_EQ(list.size(), 1);
        EXPECT_EQ(std::string("runtime-report"), list.front()->name());
    }

    {
        cali::ConfigManager mgr;

        EXPECT_TRUE(mgr.add(" event-trace, runtime-report "));
        EXPECT_FALSE(mgr.error());

        auto list = mgr.get_all_channels();

        ASSERT_EQ(list.size(), 2);

        EXPECT_EQ(std::string("event-trace"), list[0]->name());
        EXPECT_EQ(std::string("runtime-report"), list[1]->name());
    }

    {
        cali::ConfigManager mgr;

        EXPECT_TRUE(mgr.add(" event-trace  ( output = test.cali ),   runtime-report(output=stdout,mpi=false, profile=mpi:cuda ) "));
        EXPECT_FALSE(mgr.error());

        auto list = mgr.get_all_channels();

        ASSERT_EQ(list.size(), 2);

        EXPECT_EQ(std::string("event-trace"), list[0]->name());
        EXPECT_EQ(std::string("runtime-report"), list[1]->name());
    }

    EXPECT_TRUE(cali::ConfigManager::check_config_string("runtime-report(profile=cupti),event-trace").empty());
}

TEST(ConfigManagerTest, ParseEmptyConfig) {
    {
        cali::ConfigManager mgr;
        mgr.add("");

        EXPECT_FALSE(mgr.error());
    }

    {
        cali::ConfigManager mgr;
        mgr.add("  ");

        EXPECT_FALSE(mgr.error());        
    }
}

TEST(ConfigManagerTest, AvailableConfigs) {
    auto configs = cali::ConfigManager::available_configs();
    std::sort(configs.begin(), configs.end());
    
    std::string expected_configs[] = { "event-trace", "runtime-report" };

    EXPECT_TRUE(std::includes(configs.begin(), configs.end(),
                              std::begin(expected_configs), std::end(expected_configs)));
}

