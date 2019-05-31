#include "caliper/ConfigManager.h"

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
        cali::ConfigManager mgr("  event-trace(foo  = bar)");

        EXPECT_TRUE(mgr.error());
        EXPECT_STREQ(mgr.error_msg().c_str(), "Unknown argument: foo");

        auto list = mgr.get_all_channels();

        EXPECT_EQ(list.size(), 0);
    }
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

        EXPECT_TRUE(mgr.add(" event-trace  ( output = test.cali ),   runtime-report(output=stdout)  "));
        EXPECT_FALSE(mgr.error());

        auto list = mgr.get_all_channels();

        ASSERT_EQ(list.size(), 2);

        EXPECT_EQ(std::string("event-trace"), list[0]->name());
        EXPECT_EQ(std::string("runtime-report"), list[1]->name());
    }
}
