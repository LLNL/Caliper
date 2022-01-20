#include "../Blackboard.h"

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

    EXPECT_TRUE(bb.get(attr_imm.id()).empty());
    EXPECT_TRUE(bb.get(attr_ref.id()).empty());

    bb.set(attr_ref.id(), Entry(node_c), true);
    EXPECT_EQ(bb.get(attr_ref.id()).node()->id(), node_c->id());
    bb.set(attr_ref.id(), Entry(node_p), true);
    EXPECT_EQ(bb.get(attr_ref.id()).node()->id(), node_p->id());

    bb.set(attr_imm.id(), Entry(attr_imm, Variant(1122)), true) ;
    EXPECT_EQ(bb.get(attr_imm.id()).value().to_int(), 1122);
    bb.set(attr_hidden.id(), Entry(attr_hidden, Variant(2211)), false);
    EXPECT_EQ(bb.get(attr_hidden.id()).value().to_int(), 2211);

    EXPECT_EQ(bb.count(), 4);

    //
    // --- unset
    //

    bb.del(attr_ref.id());
    EXPECT_TRUE(bb.get(attr_ref.id()).empty());
    bb.set(attr_ref.id(), Entry(node_c), true);
    EXPECT_EQ(bb.get(attr_ref.id()).node(), node_c);

    bb.set(attr_uns.id(), Entry(attr_uns, Variant(3344)), true);
    EXPECT_EQ(bb.get(attr_uns.id()).value().to_int(), 3344);
    bb.del(attr_uns.id());
    EXPECT_TRUE(bb.get(attr_uns.id()).empty());

    EXPECT_EQ(bb.count(), 8);

    //
    // --- snapshot
    //

    FixedSizeSnapshotRecord<8> rec;
    bb.snapshot(rec.builder());
    SnapshotView view = rec.view();

    EXPECT_EQ(view.size(), 2);

    ASSERT_FALSE(view.get(attr_ref).empty());
    ASSERT_FALSE(view.get(attr_imm).empty());

    EXPECT_EQ(view.get(attr_ref).node()->id(), node_c->id());
    EXPECT_EQ(view.get(attr_imm).value().to_int(), 1122);

    EXPECT_EQ(bb.num_skipped_entries(), 0);

    bb.print_statistics(std::cout) << std::endl;
}

TEST(BlackboardTest, Exchange) {
    Caliper c;

    Attribute attr_imm =
        c.create_attribute("bb.ex.imm", CALI_TYPE_INT, CALI_ATTR_ASVALUE);

    Blackboard bb;

    EXPECT_TRUE(bb.exchange(attr_imm.id(), Entry(attr_imm, Variant(42)), true).empty());
    EXPECT_EQ(bb.exchange(attr_imm.id(), Entry(attr_imm, Variant(24)), true).value().to_int(), 42);
    EXPECT_EQ(bb.get(attr_imm.id()).value().to_int(), 24);

    EXPECT_EQ(bb.num_skipped_entries(), 0);
}

TEST(BlackboardTest, Overflow) {
    Caliper    c;
    Blackboard bb;

    for (int i = 0; i < 1100; ++i) {
        Attribute attr =
            c.create_attribute(std::string("bb.ov.")+std::to_string(i), CALI_TYPE_INT, CALI_ATTR_ASVALUE);

        bb.set(attr.id(), Entry(attr, Variant(i)), true);
    }

    EXPECT_GT(bb.num_skipped_entries(), 0);

    for (int i = 0; i < 1100; ++i) {
        Attribute attr =
            c.get_attribute(std::string("bb.ov.")+std::to_string(i));

        bb.del(attr.id());
    }

    {
        Attribute attr =
            c.get_attribute("bb.ov.42");

        bb.set(attr.id(), Entry(attr, Variant(1142)), true);
        EXPECT_EQ(bb.get(attr.id()).value().to_int(), 1142);
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

    EXPECT_TRUE(bb.get(attr_imm.id()).empty());
    EXPECT_EQ(bb.get(attr_ref.id()).node(), nullptr);

    bb.set(attr_ref.id(), Entry(node_c), true);
    EXPECT_EQ(bb.get(attr_ref.id()).node()->id(), node_c->id());

    bb.set(attr_imm.id(), Entry(attr_imm, Variant(1122)), true);
    EXPECT_EQ(bb.get(attr_imm.id()).value().to_int(), 1122);
    
    bb.set(attr_hidden.id(), Entry(attr_hidden, Variant(2211)), false);
    EXPECT_EQ(bb.get(attr_hidden.id()).value().to_int(), 2211);

    //
    // --- snapshot
    //
    
    FixedSizeSnapshotRecord<8> rec;
    bb.snapshot(rec.builder());

    auto view = rec.view();

    EXPECT_EQ(view.size(), 2);

    EXPECT_EQ(view.get(attr_ref).value().to_int(), 24);
    EXPECT_EQ(view.get(attr_imm).value().to_int(), 1122);

    EXPECT_EQ(rec.builder().skipped(), 0);
}
