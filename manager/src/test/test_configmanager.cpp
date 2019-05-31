#include "caliper/ConfigManager.hpp"

#include "caliper/ChannelController.h"

#include <gtest/gtest.h>

using namespace cali;

TEST(ConfigManagerTest, ParseErrors) {
    {
        cali::ConfigManager mgr;

        EXPECT_FALSE(mgr.add("foo"));
        EXPECT_TRUE(mgr.error());
        EXPECT_STREQ(mgr.error_msg().c_str(), "Unknown config: foo");
    }

    {
        cali::ConfigManager mgr("  spot(foo  = bar)");

        EXPECT_TRUE(mgr.error());
        EXPECT_STREQ(mgr.error_msg().c_str(), "Unknown argument: foo");

        auto list = mgr.get_all_channels();

        EXPECT_EQ(list.size(), 0);
    }
}

TEST(ConfigManagerTest, ParseConfig) {
    {
        cali::ConfigManager mgr;

        EXPECT_TRUE(mgr.add("runtime_report"));
        EXPECT_FALSE(mgr.error());

        auto list = mgr.get_all_channels();

        ASSERT_EQ(list.size(), 1);
        EXPECT_EQ(std::string("runtime_report"), list.front()->name());
    }

    {
        cali::ConfigManager mgr;

        EXPECT_TRUE(mgr.add(" spot, runtime_report "));
        EXPECT_FALSE(mgr.error());

        auto list = mgr.get_all_channels();

        ASSERT_EQ(list.size(), 2);

        EXPECT_EQ(std::string("spot"), list[0]->name());
        EXPECT_EQ(std::string("runtime_report"), list[1]->name());
    }

    {
        cali::ConfigManager mgr;

        EXPECT_TRUE(mgr.add(" spot  ( output = test.cali ),   runtime_report(output=stdout)  "));
        EXPECT_FALSE(mgr.error());

        auto list = mgr.get_all_channels();

        ASSERT_EQ(list.size(), 2);

        EXPECT_EQ(std::string("spot"), list[0]->name());
        EXPECT_EQ(std::string("runtime_report"), list[1]->name());
    }
}
