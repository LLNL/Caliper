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
        c.create_experiment("exp_a", cfg);
    Experiment* exp_b =
        c.create_experiment("exp_b", cfg);

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

    for (const Experiment* exp : c.get_experiments()) {
        if (exp->id() == exp_a_id)
            ++exp_a_count;
        if (exp->id() == exp_b_id)
            ++exp_b_count;
    }

    EXPECT_EQ(exp_a_count, 0);
    EXPECT_EQ(exp_b_count, 1);
}
