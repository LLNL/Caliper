#include "caliper/cali.h"
#include "caliper/Caliper.h"

#include "caliper/common/RuntimeConfig.h"

#include <gtest/gtest.h>

using namespace cali;

TEST(ChannelAPITest, MultiChannel) {
    RuntimeConfig cfg;

    cfg.set("CALI_CALIPER_CONFIG_CHECK", "false");
    cfg.allow_read_env(false);

    Caliper     c;

    Channel* chn_a =
        c.create_channel("chn.m.a", cfg);
    Channel* chn_b =
        c.create_channel("chn.m.b", cfg);

    Attribute   attr_global =
        c.create_attribute("multichn.global", CALI_TYPE_INT, CALI_ATTR_DEFAULT);
    Attribute   attr_local  =
        c.create_attribute("multichn.local",  CALI_TYPE_INT, CALI_ATTR_DEFAULT);

    c.begin(attr_global, Variant(42));
    
    c.begin(chn_a, attr_local, Variant(1144));
    c.begin(chn_b, attr_local, Variant(4411));

    EXPECT_EQ(c.get(chn_a, attr_global).value().to_int(), 42);
    EXPECT_EQ(c.get(chn_b, attr_global).value().to_int(), 42);
    EXPECT_EQ(c.get(chn_a, attr_local ).value().to_int(), 1144);
    EXPECT_EQ(c.get(chn_b, attr_local ).value().to_int(), 4411);

    Channel* chn_default = c.get_channel(0);

    EXPECT_TRUE(c.get(chn_default, attr_local).is_empty());
    EXPECT_EQ(c.get(chn_default, attr_global).value().to_int(), 42);

    cali_id_t chn_a_id = chn_a->id();
    cali_id_t chn_b_id = chn_b->id();

    c.delete_channel(chn_a);

    int chn_a_count = 0, chn_b_count = 0;

    for (const Channel* chn : c.get_all_channels()) {
        if (chn->id() == chn_a_id)
            ++chn_a_count;
        if (chn->id() == chn_b_id)
            ++chn_b_count;
    }

    EXPECT_EQ(chn_a_count, 0);
    EXPECT_EQ(chn_b_count, 1);

    c.delete_channel(chn_b);
}

TEST(ChannelAPITest, C_API) {
    const char* cfg[][2] = {
        { "CALI_CALIPER_CONFIG_CHECK",  "false" },
        { nullptr, nullptr }
    };

    cali_configset_t cfgset =
        cali_create_configset(cfg);

    cali_id_t chn_a_id =
        cali_create_channel("chn.c_api.a", 0, cfgset);
    cali_id_t chn_b_id =
        cali_create_channel("chn.c_api.b", 0, cfgset);
    cali_id_t chn_c_id =
        cali_create_channel("chn.c_api.c", CALI_CHANNEL_LEAVE_INACTIVE, cfgset);

    cali_delete_configset(cfgset);

    ASSERT_NE(chn_a_id, CALI_INV_ID);
    ASSERT_NE(chn_b_id, CALI_INV_ID);
    ASSERT_NE(chn_c_id, CALI_INV_ID);

    cali_begin_int_byname("chn.c_api.all", 7744);

    cali_deactivate_channel(chn_b_id);

    EXPECT_NE(cali_channel_is_active(chn_a_id), 0);
    EXPECT_EQ(cali_channel_is_active(chn_b_id), 0);
    EXPECT_EQ(cali_channel_is_active(chn_c_id), 0);

    cali_begin_int_byname("chn.c_api.not_b", 4477);

    cali_id_t attr_a = cali_find_attribute("chn.c_api.all");
    cali_id_t attr_b = cali_find_attribute("chn.c_api.not_b");

    ASSERT_NE(attr_a, CALI_INV_ID);
    ASSERT_NE(attr_b, CALI_INV_ID);

    /* chn_a should have both values */

    {
        unsigned char rec[60];
        size_t len =
            cali_channel_pull_snapshot(chn_a_id, CALI_SCOPE_THREAD, 60, rec);

        ASSERT_NE(len, 0);
        ASSERT_LT(len, 60);

        cali_variant_t val_a =
            cali_find_first_in_snapshot(rec, attr_a, nullptr);
        cali_variant_t val_b =
            cali_find_first_in_snapshot(rec, attr_b, nullptr);

        EXPECT_EQ(cali_variant_to_int(val_a, nullptr), 7744);
        EXPECT_EQ(cali_variant_to_int(val_b, nullptr), 4477);
    }
    
    /* chn_b should only have "all" */
    
    {
        unsigned char rec[60];
        size_t len =
            cali_channel_pull_snapshot(chn_b_id, CALI_SCOPE_THREAD, 60, rec);

        ASSERT_NE(len, 0);
        ASSERT_LT(len, 60);

        cali_variant_t val_a =
            cali_find_first_in_snapshot(rec, attr_a, nullptr);
        cali_variant_t val_b =
            cali_find_first_in_snapshot(rec, attr_b, nullptr);

        EXPECT_EQ(cali_variant_to_int(val_a, nullptr), 7744);
        EXPECT_TRUE(cali_variant_is_empty(val_b));
    }

    /* chn_c should have nothing */
    
    {
        unsigned char rec[60];
        size_t len =
            cali_channel_pull_snapshot(chn_c_id, CALI_SCOPE_THREAD, 60, rec);

        ASSERT_NE(len, 0);
        ASSERT_LT(len, 60);

        cali_variant_t val_a =
            cali_find_first_in_snapshot(rec, attr_a, nullptr);
        cali_variant_t val_b =
            cali_find_first_in_snapshot(rec, attr_b, nullptr);

        EXPECT_TRUE(cali_variant_is_empty(val_a));
        EXPECT_TRUE(cali_variant_is_empty(val_b));
    }

    cali_end(attr_b);

    cali_activate_channel(chn_b_id);

    EXPECT_NE(cali_channel_is_active(chn_b_id), 0);

    cali_end(attr_a);

    /* check if "all" is closed on channel b now */
    
    {
        unsigned char rec[60];
        size_t len =
            cali_channel_pull_snapshot(chn_b_id, CALI_SCOPE_THREAD, 60, rec);

        ASSERT_NE(len, 0);
        ASSERT_LT(len, 60);

        cali_variant_t val_a =
            cali_find_first_in_snapshot(rec, attr_a, nullptr);

        EXPECT_TRUE(cali_variant_is_empty(val_a));
    }

    cali_delete_channel(chn_a_id);
    cali_delete_channel(chn_b_id);
    cali_delete_channel(chn_c_id);

    Caliper c;

    EXPECT_EQ(c.get_channel(chn_a_id), nullptr);
    EXPECT_EQ(c.get_channel(chn_b_id), nullptr);
    EXPECT_EQ(c.get_channel(chn_c_id), nullptr);
}
