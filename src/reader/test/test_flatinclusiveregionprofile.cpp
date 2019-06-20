#include "caliper/reader/FlatInclusiveRegionProfile.h"

#include "caliper/reader/CaliperMetadataDB.h"

#include "caliper/common/Node.h"

#include <gtest/gtest.h>

using namespace cali;

TEST(FlatInclusiveRegionProfileTest, NestedRegion) {
    CaliperMetadataDB db;

    Attribute metric_attr =
        db.create_attribute("metric.attr", CALI_TYPE_INT, CALI_ATTR_ASVALUE);
    Attribute reg_a_attr =
        db.create_attribute("reg_a", CALI_TYPE_STRING, CALI_ATTR_NESTED);
    Attribute reg_b_attr =
        db.create_attribute("reg_b", CALI_TYPE_STRING, CALI_ATTR_NESTED);
    Attribute reg_c_attr =
        db.create_attribute("reg_c", CALI_TYPE_STRING, CALI_ATTR_DEFAULT);

    IdMap idmap;

    const Node* a_node =
        db.merge_node(200, reg_a_attr.id(), CALI_INV_ID, Variant(CALI_TYPE_STRING, "a", 2), idmap);
    const Node* b_node =
        db.merge_node(201, reg_b_attr.id(), 200,         Variant(CALI_TYPE_STRING, "b", 2), idmap);
    const Node* c_node =
        db.merge_node(202, reg_c_attr.id(), 201,         Variant(CALI_TYPE_STRING, "c", 2), idmap);

    FlatInclusiveRegionProfile rp(db, "metric.attr");

    rp(db,
       { Entry(a_node), Entry(metric_attr.id(), Variant(2))    } );
    rp(db,
       { Entry(b_node), Entry(metric_attr.id(), Variant(40))   } );
    rp(db,
       { Entry(c_node), Entry(metric_attr.id(), Variant(100)) } );
    rp(db,
       { Entry(metric_attr.id(), Variant(1000)) });
    rp(db,
       { Entry(b_node) });

    std::map<std::string, double> reg_profile;
    double total_reg = 0.0;
    double total = 0.0;

    std::tie(reg_profile, total_reg, total) = rp.result();

    EXPECT_DOUBLE_EQ(total_reg, 142.0);
    EXPECT_DOUBLE_EQ(total, 1142.0);

    EXPECT_EQ(reg_profile.size(), static_cast<decltype(reg_profile.size())>(2));

    EXPECT_DOUBLE_EQ(reg_profile["a"], 142.0);
    EXPECT_DOUBLE_EQ(reg_profile["b"], 140.0);
}


TEST(FlatInclusiveRegionProfileTest, GivenRegion) {
    CaliperMetadataDB db;

    Attribute metric_attr =
        db.create_attribute("metric.attr", CALI_TYPE_INT, CALI_ATTR_ASVALUE);
    Attribute reg_a_attr =
        db.create_attribute("reg_a", CALI_TYPE_STRING, CALI_ATTR_NESTED);
    Attribute reg_b_attr =
        db.create_attribute("reg_b", CALI_TYPE_STRING, CALI_ATTR_NESTED);
    Attribute reg_c_attr =
        db.create_attribute("reg_c", CALI_TYPE_STRING, CALI_ATTR_DEFAULT);

    IdMap idmap;
    
    const Node* c_node =
        db.merge_node(200, reg_c_attr.id(), CALI_INV_ID, Variant(CALI_TYPE_STRING, "c", 2), idmap);
    const Node* a_node =
        db.merge_node(201, reg_a_attr.id(), 200,         Variant(CALI_TYPE_STRING, "a", 2), idmap);
    const Node* b_node =
        db.merge_node(202, reg_b_attr.id(), 201,         Variant(CALI_TYPE_STRING, "b", 2), idmap);

    FlatInclusiveRegionProfile rp(db, "metric.attr", "reg_c");

    rp(db,
       { Entry(a_node), Entry(metric_attr.id(), Variant(2))   } );
    rp(db,
       { Entry(b_node), Entry(metric_attr.id(), Variant(40))  } );
    rp(db,
       { Entry(c_node), Entry(metric_attr.id(), Variant(100)) } );
    rp(db,
       { Entry(metric_attr.id(), Variant(1000)) });
    rp(db,
       { Entry(b_node) });

    std::map<std::string, double> reg_profile;
    double total_reg = 0.0;
    double total = 0.0;

    std::tie(reg_profile, total_reg, total) = rp.result();

    EXPECT_DOUBLE_EQ(total_reg, 142.0);
    EXPECT_DOUBLE_EQ(total, 1142.0);

    EXPECT_EQ(reg_profile.size(), static_cast<decltype(reg_profile.size())>(1));

    EXPECT_DOUBLE_EQ(reg_profile["c"], 142.0);
}
