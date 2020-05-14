#include "caliper/caliper-config.h"

#include "../../interface/c_fortran/wrapBufferedRegionProfile.h"

#include "caliper/cali.h"

#include <unistd.h>

#include <gtest/gtest.h>

TEST(C_Wrapper, BufferedRegionProfile) {
    cali_BufferedRegionProfile rp;
    cali_BufferedRegionProfile_new(&rp);

    cali_BufferedRegionProfile_start(&rp);

    CALI_MARK_BEGIN("wrap.rp.outer");
    CALI_MARK_BEGIN("wrap.rp.inner");
    usleep(20000);
    CALI_MARK_END("wrap.rp.inner");
    usleep(10000);
    CALI_MARK_END("wrap.rp.outer");

    cali_BufferedRegionProfile_stop(&rp);

    cali_BufferedRegionProfile_fetch_exclusive_region_times(&rp);
    double e_tot = cali_BufferedRegionProfile_total_profiling_time(&rp);
    double e_reg = cali_BufferedRegionProfile_total_region_time(&rp);
    double e_out = cali_BufferedRegionProfile_region_time(&rp, "wrap.rp.outer");
    double e_inn = cali_BufferedRegionProfile_region_time(&rp, "wrap.rp.inner");

    cali_BufferedRegionProfile_fetch_inclusive_region_times(&rp);
    double i_tot = cali_BufferedRegionProfile_total_profiling_time(&rp);
    double i_reg = cali_BufferedRegionProfile_total_region_time(&rp);
    double i_out = cali_BufferedRegionProfile_region_time(&rp, "wrap.rp.outer");
    double i_inn = cali_BufferedRegionProfile_region_time(&rp, "wrap.rp.inner");

    cali_BufferedRegionProfile_delete(&rp);

    EXPECT_GT(e_inn, 0.0);
    EXPECT_GT(e_out, 0.0);
    EXPECT_GT(e_inn, e_out);
    EXPECT_GE(e_reg, e_inn + e_out);
    EXPECT_GE(e_tot, e_reg);

    EXPECT_FLOAT_EQ(e_inn, i_inn);
    EXPECT_FLOAT_EQ(e_reg, i_reg);
    EXPECT_FLOAT_EQ(e_tot, i_tot);

    EXPECT_GT(i_inn, 0.0);
    EXPECT_GT(i_out, i_inn);
    EXPECT_GE(i_reg, i_out);
    EXPECT_GE(i_tot, i_reg);
}
