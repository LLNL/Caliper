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
        cali::ConfigManager::argmap_t extra_kv_pairs;

        EXPECT_TRUE(mgr.add(" event-trace, runtime-report, aggregate_across_ranks=false, foo=bar , blagarbl ", extra_kv_pairs));
        EXPECT_FALSE(mgr.error());

        auto list = mgr.get_all_channels();

        ASSERT_EQ(list.size(), 2);

        EXPECT_EQ(std::string("event-trace"), list[0]->name());
        EXPECT_EQ(std::string("runtime-report"), list[1]->name());

        ASSERT_EQ(extra_kv_pairs.size(), 2);
        EXPECT_EQ(extra_kv_pairs["foo"], std::string("bar"));
        EXPECT_EQ(extra_kv_pairs["blagarbl"], std::string());
    }

    {
        cali::ConfigManager mgr;

        EXPECT_TRUE(mgr.add(" event-trace  ( output = test.cali ),   runtime-report(output=stdout,aggregate_across_ranks=false ) "));
        EXPECT_FALSE(mgr.error());

        auto list = mgr.get_all_channels();

        ASSERT_EQ(list.size(), 2);

        EXPECT_EQ(std::string("event-trace"), list[0]->name());
        EXPECT_EQ(std::string("runtime-report"), list[1]->name());
    }

    EXPECT_TRUE(cali::ConfigManager::check_config_string("runtime-report,event-trace").empty());
    EXPECT_TRUE(cali::ConfigManager::check_config_string("runtime-report(profile.cuda=false,io.bytes=false),event-trace").empty());
    EXPECT_TRUE(cali::ConfigManager::check_config_string("runtime-report,event-trace,foo=bar", true).empty());

    EXPECT_EQ(cali::ConfigManager::check_config_string("runtime-report,profile.mpi=bla"), std::string("runtime-report: Invalid value \"bla\" for profile.mpi"));
    EXPECT_EQ(cali::ConfigManager::check_config_string("runtime-report,event-trace(trace.mpi=bla)"), std::string("event-trace: Invalid value \"bla\" for trace.mpi"));
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

