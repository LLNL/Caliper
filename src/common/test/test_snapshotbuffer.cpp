#include "caliper/common/SnapshotBuffer.h"

#include "caliper/common/CompressedSnapshotRecord.h"
#include "caliper/common/Node.h"

#include <gtest/gtest.h>

using namespace cali;

TEST(SnapshotBufferTest, Append) {
    cali_id_t attr_in[3] = { 7, CALI_INV_ID, 42 };
    Variant   data_in[3] = { Variant(CALI_TYPE_INT), Variant(), Variant(1.23) };

    Node* n1 = new Node(1, 1, Variant(CALI_TYPE_STRING, "whee", 4));
    Node* n2 = new Node(2, 2, Variant(-1.0));
    Node* n3 = new Node(3, 2, Variant(42.0));

    n1->append(n2);
    n1->append(n3);

    const Node* node_in[2] = { n2, n3 };

    CompressedSnapshotRecord in_rec_1;

    EXPECT_EQ(in_rec_1.append(1, attr_in, data_in), static_cast<size_t>(0));
    EXPECT_EQ(in_rec_1.append(2, node_in), static_cast<size_t>(0));
    EXPECT_EQ(in_rec_1.append(2, attr_in+1, data_in+1), static_cast<size_t>(0));

    ASSERT_EQ(in_rec_1.num_nodes(), static_cast<size_t>(2));
    ASSERT_EQ(in_rec_1.num_immediates(), static_cast<size_t>(3));

    CompressedSnapshotRecord in_rec_2;

    EXPECT_EQ(in_rec_2.append(1, node_in), static_cast<size_t>(0));
    EXPECT_EQ(in_rec_2.append(2, attr_in, data_in), static_cast<size_t>(0));

    ASSERT_EQ(in_rec_2.num_nodes(), static_cast<size_t>(1));
    ASSERT_EQ(in_rec_2.num_immediates(), static_cast<size_t>(2));

    SnapshotBuffer in_buf;

    in_buf.append(in_rec_1);
    in_buf.append(in_rec_2);

    EXPECT_EQ(in_buf.count(), static_cast<size_t>(2));

    CompressedSnapshotRecordView out_rec[2];

    size_t pos = 0;
    
    for (size_t i = 0; i < 2; ++i)
        out_rec[i] = CompressedSnapshotRecordView(in_buf.data() + pos, &pos);

    ASSERT_EQ(out_rec[0].num_nodes(), static_cast<size_t>(2));
    ASSERT_EQ(out_rec[0].num_immediates(), static_cast<size_t>(3));
    ASSERT_EQ(out_rec[1].num_nodes(), static_cast<size_t>(1));
    ASSERT_EQ(out_rec[1].num_immediates(), static_cast<size_t>(2));

    {
        cali_id_t node_out[2];
        cali_id_t attr_out[3];
        Variant   data_out[3];

        out_rec[0].unpack_nodes(2, node_out);
        out_rec[0].unpack_immediate(3, attr_out, data_out);

        EXPECT_EQ(node_out[0], n2->id());
        EXPECT_EQ(node_out[1], n3->id());

        EXPECT_EQ(attr_out[0], attr_in[0]);
        EXPECT_EQ(attr_out[1], attr_in[1]);
        EXPECT_EQ(attr_out[2], attr_in[2]);

        EXPECT_EQ(data_out[0], data_in[0]);
        EXPECT_EQ(data_out[1], data_in[1]);
        EXPECT_EQ(data_out[2], data_in[2]);
    }

    {
        cali_id_t node_out[1];
        cali_id_t attr_out[2];
        Variant   data_out[2];

        out_rec[1].unpack_nodes(1, node_out);
        out_rec[1].unpack_immediate(2, attr_out, data_out);

        EXPECT_EQ(node_out[0], n2->id());

        EXPECT_EQ(attr_out[0], attr_in[0]);
        EXPECT_EQ(attr_out[1], attr_in[1]);

        EXPECT_EQ(data_out[0], data_in[0]);
        EXPECT_EQ(data_out[1], data_in[1]);
    }
}

TEST(SnapshotBufferTest, Import) {
    cali_id_t attr_in[3] = { 7, CALI_INV_ID, 42 };
    Variant   data_in[3] = { Variant(CALI_TYPE_INT), Variant(), Variant(1.23) };

    Node* n1 = new Node(1, 1, Variant(CALI_TYPE_STRING, "whee", 4));
    Node* n2 = new Node(2, 2, Variant(-1.0));
    Node* n3 = new Node(3, 2, Variant(42.0));

    n1->append(n2);
    n1->append(n3);

    const Node* node_in[2] = { n2, n3 };

    CompressedSnapshotRecord in_rec_1;

    EXPECT_EQ(in_rec_1.append(1, attr_in, data_in), static_cast<size_t>(0));
    EXPECT_EQ(in_rec_1.append(2, node_in), static_cast<size_t>(0));
    EXPECT_EQ(in_rec_1.append(2, attr_in+1, data_in+1), static_cast<size_t>(0));

    ASSERT_EQ(in_rec_1.num_nodes(), static_cast<size_t>(2));
    ASSERT_EQ(in_rec_1.num_immediates(), static_cast<size_t>(3));

    CompressedSnapshotRecord in_rec_2;

    EXPECT_EQ(in_rec_2.append(1, node_in), static_cast<size_t>(0));
    EXPECT_EQ(in_rec_2.append(2, attr_in, data_in), static_cast<size_t>(0));

    ASSERT_EQ(in_rec_2.num_nodes(), static_cast<size_t>(1));
    ASSERT_EQ(in_rec_2.num_immediates(), static_cast<size_t>(2));

    SnapshotBuffer in_buf;

    in_buf.append(in_rec_1);
    in_buf.append(in_rec_2);

    EXPECT_EQ(in_buf.count(), static_cast<size_t>(2));

    SnapshotBuffer out_buf;

    memcpy(out_buf.import(in_buf.size(), in_buf.count()), in_buf.data(), in_buf.size());

    EXPECT_EQ(out_buf.count(), static_cast<size_t>(2));

    CompressedSnapshotRecordView out_rec[2];

    size_t pos = 0;
    
    for (size_t i = 0; i < 2; ++i)
        out_rec[i] = CompressedSnapshotRecordView(out_buf.data() + pos, &pos);

    ASSERT_EQ(out_rec[0].num_nodes(), static_cast<size_t>(2));
    ASSERT_EQ(out_rec[0].num_immediates(), static_cast<size_t>(3));
    ASSERT_EQ(out_rec[1].num_nodes(), static_cast<size_t>(1));
    ASSERT_EQ(out_rec[1].num_immediates(), static_cast<size_t>(2));

    {
        cali_id_t node_out[2];
        cali_id_t attr_out[3];
        Variant   data_out[3];

        out_rec[0].unpack_nodes(2, node_out);
        out_rec[0].unpack_immediate(3, attr_out, data_out);

        EXPECT_EQ(node_out[0], n2->id());
        EXPECT_EQ(node_out[1], n3->id());

        EXPECT_EQ(attr_out[0], attr_in[0]);
        EXPECT_EQ(attr_out[1], attr_in[1]);
        EXPECT_EQ(attr_out[2], attr_in[2]);

        EXPECT_EQ(data_out[0], data_in[0]);
        EXPECT_EQ(data_out[1], data_in[1]);
        EXPECT_EQ(data_out[2], data_in[2]);
    }

    {
        cali_id_t node_out[1];
        cali_id_t attr_out[2];
        Variant   data_out[2];

        out_rec[1].unpack_nodes(1, node_out);
        out_rec[1].unpack_immediate(2, attr_out, data_out);

        EXPECT_EQ(node_out[0], n2->id());

        EXPECT_EQ(attr_out[0], attr_in[0]);
        EXPECT_EQ(attr_out[1], attr_in[1]);

        EXPECT_EQ(data_out[0], data_in[0]);
        EXPECT_EQ(data_out[1], data_in[1]);
    }
}
