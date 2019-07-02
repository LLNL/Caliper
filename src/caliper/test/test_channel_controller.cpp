#include "caliper/cali.h"
#include "caliper/ChannelController.h"

#include "caliper/Caliper.h"

#include <gtest/gtest.h>

using namespace cali;

TEST(ChannelControllerTest, ChannelController)
{
    struct TestCC : public ChannelController {
        bool     saw_create_callback = false;
        Channel* the_channel = nullptr;

        void on_create(Caliper*, Channel* chn) {
            saw_create_callback = true;
            the_channel = chn;
        }

        TestCC()
            : ChannelController("testCC", 0, {
                    { "CALI_CHANNEL_FLUSH_ON_EXIT", "false" },
                    { "CALI_CHANNEL_CONFIG_CHECK",  "false" }
                })
            { }
    };

    TestCC testCC;

    EXPECT_FALSE(testCC.is_active());
    EXPECT_FALSE(testCC.saw_create_callback);

    testCC.stop(); // shouldn't do anything

    testCC.start();

    EXPECT_TRUE(testCC.is_active());
    EXPECT_TRUE(testCC.saw_create_callback);

    ASSERT_NE(testCC.the_channel, nullptr);
    EXPECT_EQ(testCC.the_channel->name(), std::string("testCC"));

    testCC.stop();

    EXPECT_FALSE(testCC.is_active());
}

TEST(ChannelControllerTest, DestroyChannel)
{
    struct DestroyTestCC : public ChannelController {
        void destruct() {
            if (is_active())
                Caliper::instance().delete_channel(channel());
        }

        bool channel_is_null() {
            return channel() == nullptr;
        }

        DestroyTestCC()
            : ChannelController("DestroyTestCC", 0, {
                    { "CALI_CHANNEL_FLUSH_ON_EXIT", "false" },
                    { "CALI_CHANNEL_CONFIG_CHECK",  "false" }
                })
            { }
    };

    DestroyTestCC testCC;
    DestroyTestCC testCCref(testCC);

    EXPECT_FALSE(testCC.is_active());
    EXPECT_FALSE(testCCref.is_active());
    EXPECT_TRUE(testCC.channel_is_null());
    EXPECT_TRUE(testCCref.channel_is_null());

    testCC.start();

    {
        DestroyTestCC testCCsecondref(testCCref);

        EXPECT_TRUE(testCCsecondref.is_active());
        EXPECT_FALSE(testCCsecondref.channel_is_null());

        // testCCsecondref is being destroyed, but the remaining ones should remain active
    }

    EXPECT_TRUE(testCC.is_active());
    EXPECT_TRUE(testCCref.is_active());
    EXPECT_FALSE(testCC.channel_is_null());
    EXPECT_FALSE(testCCref.channel_is_null());

    testCC.destruct();

    EXPECT_FALSE(testCC.is_active());
    EXPECT_FALSE(testCCref.is_active());
    EXPECT_TRUE(testCC.channel_is_null());
    EXPECT_TRUE(testCCref.channel_is_null());
}
