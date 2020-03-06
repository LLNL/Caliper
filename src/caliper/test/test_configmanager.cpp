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
    EXPECT_TRUE(cali::ConfigManager::check_config_string("runtime-report,event-trace,foo=bar", true).empty());
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


namespace
{

class TestController : public cali::ChannelController
{
    cali::ConfigManager::Options opts;

    TestController(const cali::ConfigManager::Options& o)
        : ChannelController("testcontroller", 0, {
                { "CALI_CHANNEL_CONFIG_CHECK",  "false" },
                { "CALI_CHANNEL_FLUSH_ON_EXIT", "false" }
            }),
          opts(o)
    { }

public:

    std::string get_opt(const char* name) {
        return opts.get(name).to_string();
    }

    bool is_set(const char* name) {
        return opts.is_set(name);
    }

    bool is_enabled(const char* name) {
        return opts.is_enabled(name);
    }

    bool enabled_opts_list_matches(const std::vector<std::string> list) {
        std::vector<std::string> a(list);
        std::vector<std::string> b(opts.enabled_options());
        std::sort(a.begin(), a.end());
        std::sort(b.begin(), b.end());
        return a == b;
    }

    static cali::ChannelController* create(const cali::ConfigManager::Options& opts) {
        return new TestController(opts);
    }

    ~TestController()
        { }
};

const char* testcontroller_spec =
    "{"
    " \"name\"        : \"testcontroller\","
    " \"description\" : \"Test controller for ConfigManager unit tests\","
    " \"options\" : "
    " ["
    "  {"
    "   \"name\": \"boolopt\","
    "   \"type\": \"bool\","
    "   \"description\": \"A boolean option\""
    "  },"
    "  {"
    "   \"name\": \"another_opt\","
    "   \"type\": \"bool\","
    "   \"description\": \"Another boolean option\""
    "  },"
    "  {"
    "   \"name\": \"not_set_opt\","
    "   \"type\": \"bool\","
    "   \"description\": \"Yet another boolean option\""
    "  },"
    "  {"
    "   \"name\": \"stringopt\","
    "   \"type\": \"string\","
    "   \"description\": \"A boolean option\""
    "  }"
    " ]"
    "}";

const ConfigManager::ConfigInfo  testcontroller_info = { testcontroller_spec, TestController::create, nullptr };
const ConfigManager::ConfigInfo* controller_list[] = { &testcontroller_info, nullptr };

} // namespace [anonymous]

TEST(ConfigManagerTest, Options)
{
    ConfigManager::add_controllers(controller_list);

    {
        auto configs = cali::ConfigManager::available_configs();
        std::sort(configs.begin(), configs.end());

        std::string expected_configs[] = { "event-trace", "runtime-report", "testcontroller" };

        EXPECT_TRUE(std::includes(configs.begin(), configs.end(),
                                std::begin(expected_configs), std::end(expected_configs)));
    }

    EXPECT_EQ(cali::ConfigManager::check_config_string("testcontroller(boolopt=bla,stringopt=hi)"), "Invalid value \"bla\" for boolopt");

    {
        ConfigManager mgr("testcontroller (   boolopt, another_opt=false,stringopt=hi)");

        EXPECT_FALSE(mgr.error()) << mgr.error_msg();
        auto cP = mgr.get_channel("testcontroller");
        ASSERT_TRUE(cP);
        auto tP = std::dynamic_pointer_cast<TestController>(cP);
        ASSERT_TRUE(tP);

        EXPECT_TRUE(tP->is_set("boolopt"));
        EXPECT_TRUE(tP->is_set("another_opt"));
        EXPECT_FALSE(tP->is_set("not_set_opt"));
        EXPECT_TRUE(tP->is_set("stringopt"));

        EXPECT_TRUE(tP->is_enabled("boolopt"));
        EXPECT_FALSE(tP->is_enabled("anotheropt"));
        EXPECT_TRUE(tP->is_enabled("stringopt"));
        EXPECT_FALSE(tP->is_enabled("not_set_opt"));

        EXPECT_EQ(tP->get_opt("stringopt"), std::string("hi"));

        std::vector<std::string> list { "boolopt" };
        EXPECT_TRUE(tP->enabled_opts_list_matches(list));
    }
}
