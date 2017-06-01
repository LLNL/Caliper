#include "../CompressedSnapshotRecord.h"

#include "Node.h"

#include <gtest/gtest.h>

using namespace cali;

//
// --- tests
//

TEST(CompressedSnapshotRecordTest, Append) {
    cali_id_t attr_in[3] = { 7, CALI_INV_ID, 42 };
    Variant   data_in[3] = { Variant(CALI_TYPE_INT), Variant(), Variant(1.23) };

    Node* n1 = new Node(1, 1, Variant(CALI_TYPE_STRING, "whee", 4));
    Node* n2 = new Node(2, 2, Variant(-1.0));
    Node* n3 = new Node(3, 2, Variant(42.0));

    n1->append(n2);
    n1->append(n3);

    const Node* node_in[2] = { n2, n3 };

    CompressedSnapshotRecord rec;

    EXPECT_EQ(rec.append(1, attr_in, data_in), static_cast<size_t>(0));
    EXPECT_EQ(rec.append(2, node_in), static_cast<size_t>(0));
    EXPECT_EQ(rec.append(2, attr_in+1, data_in+1), static_cast<size_t>(0));

    ASSERT_EQ(rec.num_nodes(), static_cast<size_t>(2));
    ASSERT_EQ(rec.num_immediates(), static_cast<size_t>(3));

    cali_id_t node_out[2];
    cali_id_t attr_out[3];
    Variant   data_out[3];

    CompressedSnapshotRecordView view(rec.view());

    view.unpack_nodes(2, node_out);
    view.unpack_immediate(3, attr_out, data_out);

    EXPECT_EQ(node_out[0], n2->id());
    EXPECT_EQ(node_out[1], n3->id());

    EXPECT_EQ(attr_out[0], attr_in[0]);
    EXPECT_EQ(attr_out[1], attr_in[1]);
    EXPECT_EQ(attr_out[2], attr_in[2]);

    EXPECT_EQ(data_out[0], data_in[0]);
    EXPECT_EQ(data_out[1], data_in[1]);
    EXPECT_EQ(data_out[2], data_in[2]);

    delete n3;
    delete n2;
    delete n1;
}

TEST(CompressedSnapshotRecordTest, Decode) {
    cali_id_t attr_in[3] = { 7, CALI_INV_ID, 42 };
    Variant   data_in[3] = { Variant(CALI_TYPE_INT), Variant(), Variant(1.23) };

    Node* n1 = new Node(1, 1, Variant(CALI_TYPE_STRING, "whee", 4));
    Node* n2 = new Node(2, 2, Variant(-1.0));
    Node* n3 = new Node(3, 2, Variant(42.0));

    n1->append(n2);
    n1->append(n3);

    const Node* node_in[2] = { n2, n3 };

    CompressedSnapshotRecord rec;

    EXPECT_EQ(rec.append(1, attr_in, data_in), static_cast<size_t>(0));
    EXPECT_EQ(rec.append(2, node_in), static_cast<size_t>(0));
    EXPECT_EQ(rec.append(2, attr_in+1, data_in+1), static_cast<size_t>(0));

    ASSERT_EQ(rec.num_nodes(), static_cast<size_t>(2));
    ASSERT_EQ(rec.num_immediates(), static_cast<size_t>(3));

    cali_id_t node_out[2];
    cali_id_t attr_out[3];
    Variant   data_out[3];

    size_t pos;
    CompressedSnapshotRecordView view(rec.data(), &pos);

    ASSERT_EQ(view.num_nodes(), rec.num_nodes());
    ASSERT_EQ(view.num_immediates(), rec.num_immediates());

    view.unpack_nodes(2, node_out);
    view.unpack_immediate(3, attr_out, data_out);

    EXPECT_EQ(node_out[0], n2->id());
    EXPECT_EQ(node_out[1], n3->id());

    EXPECT_EQ(attr_out[0], attr_in[0]);
    EXPECT_EQ(attr_out[1], attr_in[1]);
    EXPECT_EQ(attr_out[2], attr_in[2]);

    EXPECT_EQ(data_out[0], data_in[0]);
    EXPECT_EQ(data_out[1], data_in[1]);
    EXPECT_EQ(data_out[2], data_in[2]);

    delete n3;
    delete n2;
    delete n1;
}
