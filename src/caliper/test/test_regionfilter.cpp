#include "../RegionFilter.h"

#include <gtest/gtest.h>

using namespace cali;

TEST(RegionFilterTest, IncludeExclude) {
    auto p = RegionFilter::from_config(
            " \" exact match\", match(\"matchme\"), startswith(start, mpi_))",
            "\"start exclude\" ,startswith(mpi_exclude)"
        );

    ASSERT_TRUE(p.second.empty());

    RegionFilter f(p.first);

    EXPECT_TRUE  (f.pass( " exact match"   ));
    EXPECT_FALSE (f.pass( "some random string" ));
    EXPECT_TRUE  (f.pass( "matchme"        ));
    EXPECT_TRUE  (f.pass( "starts with the magic word" ));
    EXPECT_FALSE (f.pass( "start exclude"  ));
    EXPECT_TRUE  (f.pass( "mpi_include_me" ));
    EXPECT_FALSE (f.pass( "mpi_exclude_me" ));
    EXPECT_FALSE (f.pass( "sta"            ));
}

TEST(RegionFilterTest, IncludeOnly) {
    auto p = RegionFilter::from_config(
            " \" exact match\", match(\"matchme\"), startswith(start, mpi_))",
            "    "
        );

    ASSERT_TRUE(p.second.empty());

    RegionFilter f(p.first);

    EXPECT_TRUE  (f.pass( " exact match"   ));
    EXPECT_FALSE (f.pass( "some random string" ));
    EXPECT_TRUE  (f.pass( "matchme"        ));
    EXPECT_TRUE  (f.pass( "starts with the magic word" ));
    EXPECT_TRUE  (f.pass( "mpi_include_me" ));
    EXPECT_FALSE (f.pass( "sta"            ));
}

TEST(RegionFilterTest, ExcludeOnly) {
    auto p = RegionFilter::from_config(
            "  ",
            "\" exclude\" ,startswith(mpi_exclude)"
        );

    ASSERT_TRUE(p.second.empty());

    RegionFilter f(p.first);

    EXPECT_TRUE  (f.pass( "some random string" ));
    EXPECT_FALSE (f.pass( " exclude"       ));
    EXPECT_TRUE  (f.pass( "mpi_include_me" ));
    EXPECT_FALSE (f.pass( "mpi_exclude_me" ));
    EXPECT_TRUE  (f.pass( "mpi"            ));
}

TEST(RegionFilterTest, IncludeRegex) {
    auto p = RegionFilter::from_config(" regex(\".*match\") ", "");

    ASSERT_TRUE(p.second.empty());

    RegionFilter f(p.first);

    EXPECT_TRUE  (f.pass( "i should match" ));
    EXPECT_FALSE (f.pass( "i should match not" ));
    EXPECT_FALSE (f.pass( "me neither" ));
}

TEST(RegionFilterTest, ParseError) {
    auto p = RegionFilter::from_config("match(bar, foo, startswith(fox)", "");

    ASSERT_FALSE(p.second.empty());
    EXPECT_STREQ(p.second.c_str(), "in match(): missing ')'");
}
