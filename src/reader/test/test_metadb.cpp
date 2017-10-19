#include "caliper/reader/CaliperMetadataDB.h"

#include "caliper/common/Node.h"

#include <gtest/gtest.h>

using namespace cali;

TEST(MetaDBTest, MergeSnapshotFromDB) {
    CaliperMetadataDB db1;

    Attribute str_attr = 
        db1.create_attribute("str.attr", CALI_TYPE_STRING, CALI_ATTR_DEFAULT);
    Attribute int_attr = 
        db1.create_attribute("int.attr", CALI_TYPE_INT,    CALI_ATTR_ASVALUE);

    IdMap idmap;

    const Node* a_in = 
        db1.merge_node(200, str_attr.id(), CALI_INV_ID, Variant(CALI_TYPE_STRING, "a", 1), idmap);
    const Node* b_in = 
        db1.merge_node(201, str_attr.id(), 200,         Variant(CALI_TYPE_STRING, "b", 2), idmap);

    cali_id_t attr_in = int_attr.id();
    Variant   data_in(42);

    CaliperMetadataDB db2;

    EntryList list_out = 
        db2.merge_snapshot(1, &b_in, 1, &attr_in, &data_in, db1);

    Attribute str_attr_out = db2.get_attribute("str.attr");
    Attribute int_attr_out = db2.get_attribute("int.attr");

    EXPECT_EQ(str_attr_out.type(), CALI_TYPE_STRING);
    EXPECT_EQ(int_attr_out.type(), CALI_TYPE_INT);

    EXPECT_NE(str_attr.node(), str_attr_out.node());
    EXPECT_NE(int_attr.node(), int_attr_out.node());

    ASSERT_EQ(list_out.size(), 2);

    // assume order is nodes, immediate

    const Node* b_out = list_out[0].node();

    ASSERT_NE(b_out, nullptr);
    ASSERT_NE(b_out->parent(), nullptr);
    EXPECT_EQ(b_out->attribute(), str_attr_out.id());
    EXPECT_EQ(b_out->data().to_string(), "b");
    EXPECT_EQ(b_out->parent()->attribute(), str_attr_out.id());
    EXPECT_EQ(b_out->parent()->data().to_string(), "a");

    EXPECT_EQ(list_out[1].attribute(), int_attr_out.id());
    EXPECT_EQ(list_out[1].value().to_int(), 42);

    EXPECT_NE(b_out, b_in);
}
