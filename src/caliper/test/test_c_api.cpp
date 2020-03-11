#include "caliper/caliper-config.h"

#include "caliper/cali.h"

#include <gtest/gtest.h>

TEST(C_API_Test, CaliperVersion) {
    EXPECT_STREQ(cali_caliper_version(), CALIPER_VERSION);
}
