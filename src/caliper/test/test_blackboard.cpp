#include "../Blackboard.h"

#include "caliper/common/CompressedSnapshotRecord.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include <gtest/gtest.h>

using namespace cali;

TEST(BlackboardTest, BasicFunctionality) {
    Caliper c;

    Attribute attr_ref =
        c.create_attribute("bb.gs.ref", CALI_TYPE_INT, CALI_ATTR_DEFAULT);
    Attribute attr_imm =
        c.create_attribute("bb.gs.imm", CALI_TYPE_INT, CALI_ATTR_ASVALUE);
    Attribute attr_uns =
        c.create_attribute("bb.gs.uns", CALI_TYPE_INT, CALI_ATTR_ASVALUE);
    Attribute attr_hidden =
        c.create_attribute("bb.gs.h",   CALI_TYPE_INT, CALI_ATTR_ASVALUE | CALI_ATTR_HIDDEN);

    Node* node_p = c.make_tree_entry(attr_ref, Variant(42));
    Node* node_c = c.make_tree_entry(attr_ref, Variant(24), node_p);

    Blackboard bb;

    //
    // --- basic get/set
    //

    EXPECT_TRUE(bb.get(attr_imm).empty());
    EXPECT_EQ(bb.get_node(attr_ref), nullptr);

    bb.set(attr_ref, node_c);
    EXPECT_EQ(bb.get_node(attr_ref)->id(), node_c->id());
    bb.set(attr_ref, node_p);
    EXPECT_EQ(bb.get_node(attr_ref)->id(), node_p->id());

    bb.set(attr_imm, Variant(1122));
    EXPECT_EQ(bb.get(attr_imm).to_int(), 1122);
    bb.set(attr_hidden, Variant(2211));
    EXPECT_EQ(bb.get(attr_hidden).to_int(), 2211);

    EXPECT_EQ(bb.count(), 4);

    //
    // --- unset
    //

    bb.unset(attr_ref);
    EXPECT_EQ(bb.get_node(attr_ref), nullptr);
    bb.set(attr_ref, node_c);
    EXPECT_EQ(bb.get_node(attr_ref), node_c);

    bb.set(attr_uns, Variant(3344));
    EXPECT_EQ(bb.get(attr_uns).to_int(), 3344);
    bb.unset(attr_uns);
    EXPECT_TRUE(bb.get(attr_uns).empty());

    EXPECT_EQ(bb.count(), 8);

    //
    // --- snapshot
    //
    
    CompressedSnapshotRecord rec;

    bb.snapshot(&rec);

    CompressedSnapshotRecordView view = rec.view();

    EXPECT_EQ(view.num_nodes(), 1);
    EXPECT_EQ(view.num_immediates(), 1);

    {
        cali_id_t node_id;
        view.unpack_nodes(1, &node_id);

        EXPECT_EQ(node_id, node_c->id());
    }

    {
        cali_id_t attr_id;
        Variant   data;

        view.unpack_immediate(1, &attr_id, &data);

        EXPECT_EQ(attr_id, attr_imm.id());
        EXPECT_EQ(data.to_int(), 1122);
    }

    EXPECT_EQ(bb.num_skipped_entries(), 0);

    bb.print_statistics(std::cout) << std::endl;
}

TEST(BlackboardTest, Exchange) {
    Caliper c;

    Attribute attr_imm =
        c.create_attribute("bb.ex.imm", CALI_TYPE_INT, CALI_ATTR_ASVALUE);

    Blackboard bb;

    EXPECT_TRUE(bb.exchange(attr_imm, Variant(42)).empty());
    EXPECT_EQ(bb.exchange(attr_imm, Variant(24)).to_int(), 42);
    EXPECT_EQ(bb.get(attr_imm).to_int(), 24);

    EXPECT_EQ(bb.num_skipped_entries(), 0);
}

TEST(BlackboardTest, Overflow) {
    Caliper    c;
    Blackboard bb;

    for (int i = 0; i < 1100; ++i) {
        Attribute attr =
            c.create_attribute(std::string("bb.ov.")+std::to_string(i), CALI_TYPE_INT, CALI_ATTR_ASVALUE);

        bb.set(attr, Variant(i));
    }

    EXPECT_GT(bb.num_skipped_entries(), 0);

    for (int i = 0; i < 1100; ++i) {
        Attribute attr =
            c.get_attribute(std::string("bb.ov.")+std::to_string(i));

        bb.unset(attr);
    }

    {
        Attribute attr =
            c.get_attribute("bb.ov.42");

        bb.set(attr, Variant(1142));
        EXPECT_EQ(bb.get(attr).to_int(), 1142);
    }

    bb.print_statistics(std::cout) << std::endl;
}

TEST(BlackboardTest, Snapshot) {
    Caliper c;

    Attribute attr_ref =
        c.create_attribute("bb.sn.ref", CALI_TYPE_INT, CALI_ATTR_DEFAULT);
    Attribute attr_imm =
        c.create_attribute("bb.sn.imm", CALI_TYPE_INT, CALI_ATTR_ASVALUE);
    Attribute attr_hidden =
        c.create_attribute("bb.sn.h",   CALI_TYPE_INT, CALI_ATTR_ASVALUE | CALI_ATTR_HIDDEN);

    Node* node_p = c.make_tree_entry(attr_ref, Variant(42));
    Node* node_c = c.make_tree_entry(attr_ref, Variant(24), node_p);

    Blackboard bb;

    //
    // --- basic get/set
    //

    EXPECT_TRUE(bb.get(attr_imm).empty());
    EXPECT_EQ(bb.get_node(attr_ref), nullptr);

    bb.set(attr_ref, node_c);
    EXPECT_EQ(bb.get_node(attr_ref)->id(), node_c->id());

    bb.set(attr_imm, Variant(1122));
    EXPECT_EQ(bb.get(attr_imm).to_int(), 1122);
    
    bb.set(attr_hidden, Variant(2211));
    EXPECT_EQ(bb.get(attr_hidden).to_int(), 2211);

    //
    // --- snapshot
    //
    
    SnapshotRecord::FixedSnapshotRecord<8> snapshot_data;
    SnapshotRecord rec(snapshot_data);

    bb.snapshot(&rec);

    EXPECT_EQ(rec.num_nodes(), 1);
    EXPECT_EQ(rec.num_immediate(), 1);

    EXPECT_EQ(rec.get(attr_ref).value().to_int(), 24);
    EXPECT_EQ(rec.get(attr_imm).value().to_int(), 1122);

    EXPECT_EQ(bb.num_skipped_entries(), 0);
}
