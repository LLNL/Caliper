#include "caliper/cali.h"
#include "caliper/Caliper.h"

#include "caliper/common/RuntimeConfig.h"

#include <gtest/gtest.h>

using namespace cali;

TEST(ExperimentAPITest, MultiExperiment) {
    RuntimeConfig cfg;

    cfg.set("CALI_CALIPER_CONFIG_CHECK", "false");
    cfg.allow_read_env(false);

    Caliper     c;

    Experiment* exp_a =
        c.create_experiment("exp.m.a", cfg);
    Experiment* exp_b =
        c.create_experiment("exp.m.b", cfg);

    Attribute   attr_global =
        c.create_attribute("multiexp.global", CALI_TYPE_INT, CALI_ATTR_DEFAULT);
    Attribute   attr_local  =
        c.create_attribute("multiexp.local",  CALI_TYPE_INT, CALI_ATTR_DEFAULT);

    c.begin(attr_global, Variant(42));
    
    c.begin(exp_a, attr_local, Variant(1144));
    c.begin(exp_b, attr_local, Variant(4411));

    EXPECT_EQ(c.get(exp_a, attr_global).value().to_int(), 42);
    EXPECT_EQ(c.get(exp_b, attr_global).value().to_int(), 42);
    EXPECT_EQ(c.get(exp_a, attr_local ).value().to_int(), 1144);
    EXPECT_EQ(c.get(exp_b, attr_local ).value().to_int(), 4411);

    Experiment* exp_default = c.get_experiment(0);

    EXPECT_TRUE(c.get(exp_default, attr_local).is_empty());
    EXPECT_EQ(c.get(exp_default, attr_global).value().to_int(), 42);

    cali_id_t exp_a_id = exp_a->id();
    cali_id_t exp_b_id = exp_b->id();

    c.delete_experiment(exp_a);

    int exp_a_count = 0, exp_b_count = 0;

    for (const Experiment* exp : c.get_all_experiments()) {
        if (exp->id() == exp_a_id)
            ++exp_a_count;
        if (exp->id() == exp_b_id)
            ++exp_b_count;
    }

    EXPECT_EQ(exp_a_count, 0);
    EXPECT_EQ(exp_b_count, 1);

    c.delete_experiment(exp_b);
}

TEST(ExperimentAPITest, C_API) {
    const char* cfg[][2] = {
        { "CALI_CALIPER_CONFIG_CHECK",  "false" },
        { nullptr, nullptr }
    };

    cali_configset_t cfgset =
        cali_create_configset(cfg);

    cali_id_t exp_a_id =
        cali_create_experiment("exp.c_api.a", 0, cfgset);
    cali_id_t exp_b_id =
        cali_create_experiment("exp.c_api.b", 0, cfgset);
    cali_id_t exp_c_id =
        cali_create_experiment("exp.c_api.c", CALI_EXPERIMENT_LEAVE_INACTIVE, cfgset);

    cali_delete_configset(cfgset);

    ASSERT_NE(exp_a_id, CALI_INV_ID);
    ASSERT_NE(exp_b_id, CALI_INV_ID);
    ASSERT_NE(exp_c_id, CALI_INV_ID);

    cali_begin_int_byname("exp.c_api.all", 7744);

    cali_deactivate_experiment(exp_b_id);

    EXPECT_NE(cali_experiment_is_active(exp_a_id), 0);
    EXPECT_EQ(cali_experiment_is_active(exp_b_id), 0);
    EXPECT_EQ(cali_experiment_is_active(exp_c_id), 0);

    cali_begin_int_byname("exp.c_api.not_b", 4477);

    cali_id_t attr_a = cali_find_attribute("exp.c_api.all");
    cali_id_t attr_b = cali_find_attribute("exp.c_api.not_b");

    ASSERT_NE(attr_a, CALI_INV_ID);
    ASSERT_NE(attr_b, CALI_INV_ID);

    /* exp_a should have both values */

    {
        unsigned char rec[60];
        size_t len =
            cali_experiment_pull_snapshot(exp_a_id, CALI_SCOPE_THREAD, 60, rec);

        ASSERT_NE(len, 0);
        ASSERT_LT(len, 60);

        cali_variant_t val_a =
            cali_find_first_in_snapshot(rec, attr_a, nullptr);
        cali_variant_t val_b =
            cali_find_first_in_snapshot(rec, attr_b, nullptr);

        EXPECT_EQ(cali_variant_to_int(val_a, nullptr), 7744);
        EXPECT_EQ(cali_variant_to_int(val_b, nullptr), 4477);
    }
    
    /* exp_b should only have "all" */
    
    {
        unsigned char rec[60];
        size_t len =
            cali_experiment_pull_snapshot(exp_b_id, CALI_SCOPE_THREAD, 60, rec);

        ASSERT_NE(len, 0);
        ASSERT_LT(len, 60);

        cali_variant_t val_a =
            cali_find_first_in_snapshot(rec, attr_a, nullptr);
        cali_variant_t val_b =
            cali_find_first_in_snapshot(rec, attr_b, nullptr);

        EXPECT_EQ(cali_variant_to_int(val_a, nullptr), 7744);
        EXPECT_TRUE(cali_variant_is_empty(val_b));
    }

    /* exp_c should have nothing */
    
    {
        unsigned char rec[60];
        size_t len =
            cali_experiment_pull_snapshot(exp_c_id, CALI_SCOPE_THREAD, 60, rec);

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

    cali_activate_experiment(exp_b_id);

    EXPECT_NE(cali_experiment_is_active(exp_b_id), 0);

    cali_end(attr_a);

    /* check if "all" is closed on experiment b now */
    
    {
        unsigned char rec[60];
        size_t len =
            cali_experiment_pull_snapshot(exp_b_id, CALI_SCOPE_THREAD, 60, rec);

        ASSERT_NE(len, 0);
        ASSERT_LT(len, 60);

        cali_variant_t val_a =
            cali_find_first_in_snapshot(rec, attr_a, nullptr);

        EXPECT_TRUE(cali_variant_is_empty(val_a));
    }

    cali_delete_experiment(exp_a_id);
    cali_delete_experiment(exp_b_id);
    cali_delete_experiment(exp_c_id);

    Caliper c;

    EXPECT_EQ(c.get_experiment(exp_a_id), nullptr);
    EXPECT_EQ(c.get_experiment(exp_b_id), nullptr);
    EXPECT_EQ(c.get_experiment(exp_c_id), nullptr);
}
