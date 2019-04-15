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
