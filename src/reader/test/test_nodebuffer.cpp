#include "caliper/common/NodeBuffer.h"

#include "caliper/reader/CaliperMetadataDB.h"

#include "caliper/common/Node.h"

#include <gtest/gtest.h>

#include <set>

using namespace cali;

namespace
{
    void recursive_append_path(const CaliperMetadataAccessInterface& db,
                               const Node* node,
                               NodeBuffer& buf,
                               std::set<cali_id_t>& written_nodes)
    {
        if (!node || node->id() == CALI_INV_ID)
            return;
        if (written_nodes.count(node->id()) > 0)
            return;

        if (node->attribute() < node->id())
            recursive_append_path(db, db.node(node->attribute()), buf, written_nodes);

        recursive_append_path(db, node->parent(), buf, written_nodes);

        if (written_nodes.count(node->id()) > 0)
            return;

        written_nodes.insert(node->id());
        buf.append(node);
    }
    
    void append_path(const CaliperMetadataAccessInterface& db, const Node* node, NodeBuffer& buf)
    {
        std::set<cali_id_t> written_nodes;

        recursive_append_path(db, node, buf, written_nodes);
    }
}

TEST(NodeBufferTest, Append) {
    // Quick NodeBuffer in/out test using some attributes
    
    CaliperMetadataDB in_db;
    
    Attribute in_1_attr = in_db.create_attribute("my.string.attr", CALI_TYPE_STRING, CALI_ATTR_DEFAULT);
    Attribute in_2_attr = in_db.create_attribute("my.int.attr", CALI_TYPE_INT, CALI_ATTR_DEFAULT);

    const Node* in_1 = in_db.node(in_1_attr.id());
    const Node* in_2 = in_db.node(in_2_attr.id());

    ASSERT_NE(in_1, nullptr);
    ASSERT_NE(in_2, nullptr);
    
    NodeBuffer buf;

    ::append_path(in_db, in_1, buf);
    ::append_path(in_db, in_2, buf);

    EXPECT_GE(buf.count(), static_cast<size_t>(2));

    CaliperMetadataDB out_db;
    IdMap idmap;

    auto add_node = [&out_db, &idmap](const NodeBuffer::NodeInfo& info) {
        out_db.merge_node(info.node_id, info.attr_id, info.parent_id, info.value, idmap);
    };

    buf.for_each(add_node);

    Attribute out_1_attr = out_db.get_attribute("my.string.attr");
    Attribute out_2_attr = out_db.get_attribute("my.int.attr");

    ASSERT_NE(out_1_attr, Attribute::invalid);
    ASSERT_NE(out_2_attr, Attribute::invalid);
    
    EXPECT_STREQ(out_1_attr.name().c_str(), "my.string.attr");
    EXPECT_EQ(out_1_attr.type(), CALI_TYPE_STRING);
    EXPECT_EQ(out_1_attr.properties(), in_1_attr.properties());
    
    EXPECT_STREQ(out_2_attr.name().c_str(), "my.int.attr");
    EXPECT_EQ(out_2_attr.type(), CALI_TYPE_INT);    
    EXPECT_EQ(out_2_attr.properties(), in_2_attr.properties());
}

TEST(NodeBufferTest, Import) {
    CaliperMetadataDB in_db;

    Attribute in_1_attr = in_db.create_attribute("my.string.attr", CALI_TYPE_STRING, CALI_ATTR_DEFAULT);
    Attribute in_2_attr = in_db.create_attribute("my.int.attr", CALI_TYPE_INT, CALI_ATTR_DEFAULT);

    const Node* in_1 = in_db.node(in_1_attr.id());
    const Node* in_2 = in_db.node(in_2_attr.id());

    ASSERT_NE(in_1, nullptr);
    ASSERT_NE(in_2, nullptr);
    
    NodeBuffer in_buf;

    ::append_path(in_db, in_1, in_buf);
    ::append_path(in_db, in_2, in_buf);

    EXPECT_GE(in_buf.count(), static_cast<size_t>(2));

    NodeBuffer out_buf;

    memcpy(out_buf.import(in_buf.size(), in_buf.count()), in_buf.data(), in_buf.size());

    EXPECT_EQ(out_buf.count(), in_buf.count());

    CaliperMetadataDB out_db;
    IdMap idmap;

    auto add_node = [&out_db, &idmap](const NodeBuffer::NodeInfo& info) {
        out_db.merge_node(info.node_id, info.attr_id, info.parent_id, info.value, idmap);
    };

    out_buf.for_each(add_node);

    Attribute out_1_attr = out_db.get_attribute("my.string.attr");
    Attribute out_2_attr = out_db.get_attribute("my.int.attr");

    ASSERT_NE(out_1_attr, Attribute::invalid);
    ASSERT_NE(out_2_attr, Attribute::invalid);
    
    EXPECT_STREQ(out_1_attr.name().c_str(), "my.string.attr");
    EXPECT_EQ(out_1_attr.type(), CALI_TYPE_STRING);
    EXPECT_EQ(out_1_attr.properties(), in_1_attr.properties());
    
    EXPECT_STREQ(out_2_attr.name().c_str(), "my.int.attr");
    EXPECT_EQ(out_2_attr.type(), CALI_TYPE_INT);    
    EXPECT_EQ(out_2_attr.properties(), in_2_attr.properties());    
}
