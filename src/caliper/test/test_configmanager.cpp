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
        EXPECT_STREQ(mgr.error_msg().c_str(), "Unknown option: foo");

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

    TestController(const char* name, const config_map_t& initial_cfg, const cali::ConfigManager::Options& o)
        : ChannelController(name, 0, initial_cfg),
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

    std::string get_query(const char* level, const std::map<std::string, std::string>& in, bool aliases = true) const {
        return opts.build_query(level, in, aliases);
    }

    static cali::ChannelController* create(const char* name, const config_map_t& initial_cfg, const cali::ConfigManager::Options& opts) {
        return new TestController(name, initial_cfg, opts);
    }

    ~TestController()
        { }
};

const char* testcontroller_spec =
    "{"
    " \"name\"        : \"testcontroller\","
    " \"description\" : \"Test controller for ConfigManager unit tests\","
    " \"categories\"  : [ \"testcategory\" ],"
    " \"defaults\"    : { \"defaultopt\": \"true\" },"
    " \"config\"      : "
    " {"
    "  \"CALI_CHANNEL_CONFIG_CHECK\"  : \"false\","
    "  \"CALI_CHANNEL_FLUSH_ON_EXIT\" : \"false\""
    " },"
    " \"options\" : "
    " ["
    "  {"
    "   \"name\": \"boolopt\","
    "   \"type\": \"bool\","
    "   \"description\": \"A boolean option\","
    "   \"query\": "
    "    ["
    "     { \"level\": \"local\", \"group by\": \"g\", \"let\": \"x=scale(y,2)\", \"select\": "
    "       [ { \"expr\": \"sum(x)\", \"as\": X, \"unit\": \"Foos\" } ]"
    "     }"
    "    ]"
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
    "   \"name\": \"defaultopt\","
    "   \"type\": \"bool\","
    "   \"description\": \"Yet another boolean option again\""
    "  },"
    "  {"
    "   \"name\": \"stringopt\","
    "   \"type\": \"string\","
    "   \"description\": \"A string option\""
    "  },"
    "  {"
    "   \"name\": \"intopt\","
    "   \"type\": \"int\","
    "   \"description\": \"An integer option\""
    "  }"
    " ]"
    "}";

const char* test_option_spec =
    "["
    " {  \"name\"     : \"global_boolopt\","
    "    \"category\" : \"testcategory\","
    "    \"type\"     : \"bool\""
    " },"
    " {  \"name\"     : \"invisible_opt\","
    "    \"category\" : \"not_the_testcategory\","
    "    \"type\"     : \"bool\""
    " }"
    "]";

} // namespace [anonymous]

TEST(ConfigManagerTest, Options)
{
    const ConfigManager::ConfigInfo testcontroller_info { testcontroller_spec, TestController::create, nullptr };

    {
        ConfigManager mgr;
        mgr.add_option_spec(test_option_spec);
        EXPECT_FALSE(mgr.error()) << mgr.error_msg();
        mgr.add_config_spec(testcontroller_info);
        EXPECT_FALSE(mgr.error()) << mgr.error_msg();

        auto configs = mgr.available_config_specs();
        std::sort(configs.begin(), configs.end());

        std::string expected_configs[] = { "event-trace", "runtime-report", "testcontroller" };

        EXPECT_TRUE(std::includes(configs.begin(), configs.end(),
                                std::begin(expected_configs), std::end(expected_configs)));
    }

    {
        ConfigManager mgr;
        mgr.add_option_spec(test_option_spec);
        mgr.add_config_spec(testcontroller_info);

        EXPECT_EQ(mgr.check("testcontroller(boolopt=bla,stringopt=hi)"), "Invalid value \"bla\" for boolopt");
        EXPECT_EQ(mgr.check("testcontroller(invisible_opt)"), "Unknown option: invisible_opt");

        mgr.add("testcontroller ( global_boolopt=true,  boolopt, another_opt=false,stringopt=hi)");

        EXPECT_FALSE(mgr.error()) << mgr.error_msg();
        auto cP = mgr.get_channel("testcontroller");
        ASSERT_TRUE(cP);
        auto tP = std::dynamic_pointer_cast<TestController>(cP);
        ASSERT_TRUE(tP);

        EXPECT_TRUE(tP->is_set("boolopt"));
        EXPECT_TRUE(tP->is_set("global_boolopt"));
        EXPECT_TRUE(tP->is_set("another_opt"));
        EXPECT_FALSE(tP->is_set("not_set_opt"));
        EXPECT_TRUE(tP->is_set("stringopt"));

        EXPECT_TRUE(tP->is_enabled("defaultopt"));
        EXPECT_TRUE(tP->is_enabled("boolopt"));
        EXPECT_TRUE(tP->is_enabled("global_boolopt"));
        EXPECT_FALSE(tP->is_enabled("anotheropt"));
        EXPECT_TRUE(tP->is_enabled("stringopt"));
        EXPECT_FALSE(tP->is_enabled("not_set_opt"));

        EXPECT_EQ(tP->get_opt("stringopt"), std::string("hi"));

        std::vector<std::string> list { "boolopt", "global_boolopt", "defaultopt" };
        EXPECT_TRUE(tP->enabled_opts_list_matches(list));
    }

    {
        ConfigManager mgr;
        mgr.add_option_spec(test_option_spec);
        mgr.add_config_spec(testcontroller_info);

        mgr.set_default_parameter("stringopt", "set_default_parameter");
        mgr.set_default_parameter("intopt", "4242");
        mgr.set_default_parameter_for_config("testcontroller", "defaultopt", "false");

        mgr.add("testcontroller (intopt=42), boolopt");

        EXPECT_FALSE(mgr.error()) << mgr.error_msg();
        auto cP = mgr.get_channel("testcontroller");
        ASSERT_TRUE(cP);
        auto tP = std::dynamic_pointer_cast<TestController>(cP);
        ASSERT_TRUE(tP);

        EXPECT_TRUE(tP->is_enabled("boolopt"));

        EXPECT_FALSE(tP->is_enabled("defaultopt"));
        EXPECT_EQ(tP->get_opt("intopt"),    std::string("42"));
        EXPECT_EQ(tP->get_opt("stringopt"), std::string("set_default_parameter"));
    }
}


TEST(ConfigManagerTest, BuildQuery)
{
    const ConfigManager::ConfigInfo testcontroller_info { testcontroller_spec, TestController::create, nullptr };

    {
        ConfigManager mgr;
        mgr.add_config_spec(testcontroller_info);
        mgr.add("testcontroller(boolopt)");

        auto cP = mgr.get_channel("testcontroller");
        ASSERT_TRUE(cP);
        auto tP = std::dynamic_pointer_cast<TestController>(cP);
        ASSERT_TRUE(tP);

        EXPECT_TRUE(tP->is_enabled("boolopt"));

        std::string q1 = tP->get_query("local", {
                { "select", "me" },
                { "format", "expand" },
                { "let", "a=first(b,c)" },
                { "where", "xyz=42" },
                { "group by", "z"}
            });
        const char* expect =
            " let a=first(b,c),x=scale(y,2)"
            " select me,sum(x) as \"X\" unit \"Foos\""
            " group by z,g"
            " where xyz=42"
            " format expand";

        EXPECT_STREQ(q1.c_str(), expect);

        std::string q2 = tP->get_query("local", {
                { "select", "me" },
                { "format", "expand" },
            }, false /* no aliases */);
        expect =
            " let x=scale(y,2)"
            " select me,sum(x)"
            " group by g"
            " format expand";

        EXPECT_STREQ(q2.c_str(), expect);
    }


    {
        ConfigManager mgr;
        mgr.add_config_spec(testcontroller_info);
        mgr.add("testcontroller(another_opt)");

        auto cP = mgr.get_channel("testcontroller");
        ASSERT_TRUE(cP);
        auto tP = std::dynamic_pointer_cast<TestController>(cP);
        ASSERT_TRUE(tP);

        EXPECT_FALSE(tP->is_enabled("boolopt"));

        std::string q3 = tP->get_query("local", {
                { "select", "me" },
                { "format", "expand" },
                { "let", "a=first(b,c)" },
                { "where", "xyz=42" },
                { "group by", "z"}
            });
        const char* expect =
            " let a=first(b,c)"
            " select me"
            " group by z"
            " where xyz=42"
            " format expand";

        EXPECT_STREQ(q3.c_str(), expect);

        std::string q4 = tP->get_query("local", {
                { "select", "me" },
                { "format", "expand" },
            });
        expect =
            " select me"
            " format expand";

        EXPECT_STREQ(q4.c_str(), expect);
    }
}

TEST(ConfigManagerTest, LoadCmd_SingleConfig) {
    // test parsing of a single config spec with the load() command inside add()

    ConfigManager mgr;
    mgr.add("load(\"test_single_config.json\"), testcontroller");

    EXPECT_FALSE(mgr.error()) << mgr.error_msg();
    auto cP = mgr.get_channel("testcontroller");
    EXPECT_TRUE(cP);
}

TEST(ConfigManagerTest, LoadCmd_ConfigList) {
    // test parsing of a config spec list with the load() command inside add()

    ConfigManager mgr;
    mgr.add("load(test_config_list.json)");

    EXPECT_FALSE(mgr.error()) << mgr.error_msg();
    EXPECT_EQ(mgr.get_documentation_for_spec("testcontroller_b"), std::string("testcontroller_b\n Test controller B"));
}

TEST(ConfigManagerTest, LoadCmd_ConfigAndOptions) {
    // test parsing of a config and options list with load()

    ConfigManager mgr;
    mgr.load("test_config_and_options.json");
    EXPECT_FALSE(mgr.error()) << mgr.error_msg();

    const std::string expect =
        "testcontroller"
        "\n A test controller"
        "\n  Options:"
        "\n   testoption"
        "\n    A test option";

    EXPECT_EQ(mgr.get_documentation_for_spec("testcontroller"), expect);
}
