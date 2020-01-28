#include "caliper/reader/RecordSelector.h"

#include "caliper/reader/CaliperMetadataDB.h"
#include "caliper/reader/QuerySpec.h"

#include "caliper/common/Node.h"

#include <gtest/gtest.h>

using namespace cali;

namespace
{

bool
has_attribute(const EntryList& list, const Attribute& attr)
{
    for (const Entry& e : list)
        if (attr.store_as_value()) {
            if (e.attribute() == attr.id())
                return true;
        } else {
            for (const Node* node = e.node(); node; node = node->parent())
                if (node->attribute() == attr.id())
                    return true;
        }
    
    return false;
}

bool
has_entry(const EntryList& list, const Attribute& attr, const Variant& val)
{
    for (const Entry& e : list)
        if (attr.store_as_value()) {
            if (e.attribute() == attr.id() && e.value() == val)
                return true;
        } else {
            for (const Node* node = e.node(); node; node = node->parent())
                if (node->attribute() == attr.id() && node->data() == val)
                    return true;
        }
    
    return false;
}

}

TEST(RecordFilterTest, TestExist) {
    CaliperMetadataDB db;
    IdMap             idmap;

    // create some context attributes
    
    Attribute ctx1 =
        db.create_attribute("ctx.1", CALI_TYPE_STRING, CALI_ATTR_DEFAULT);
    Attribute ctx2 = 
        db.create_attribute("ctx.2", CALI_TYPE_INT,    CALI_ATTR_DEFAULT);
    Attribute ctx3 = 
        db.create_attribute("ctx.3", CALI_TYPE_INT,    CALI_ATTR_DEFAULT);
    Attribute val_attr =
        db.create_attribute("val",   CALI_TYPE_INT,    CALI_ATTR_ASVALUE);

    // make some nodes
    
    const struct NodeInfo {
        cali_id_t node_id;
        cali_id_t attr_id;
        cali_id_t prnt_id;
        Variant   data;
    } test_nodes[] = {
        { 100, ctx1.id(), CALI_INV_ID, Variant(CALI_TYPE_STRING, "outer", 6) },
        { 101, ctx2.id(), 100,         Variant(42)                           },
        { 102, ctx1.id(), 101,         Variant(CALI_TYPE_STRING, "inner", 6) }
    };

    for ( const NodeInfo& nI : test_nodes )
        db.merge_node(nI.node_id, nI.attr_id, nI.prnt_id, nI.data, idmap);

    Variant v_val47(47);

    cali_id_t node_ctx1 = 100;
    cali_id_t node_ctx2 = 102;
    cali_id_t attr_id   = val_attr.id();

    std::vector<EntryList> in;

    in.push_back(db.merge_snapshot(1, &node_ctx1, 1, &attr_id, &v_val47, idmap));
    in.push_back(db.merge_snapshot(1, &node_ctx1, 0, nullptr,  nullptr,  idmap));
    in.push_back(db.merge_snapshot(1, &node_ctx1, 1, &attr_id, &v_val47, idmap));
    in.push_back(db.merge_snapshot(1, &node_ctx2, 1, &attr_id, &v_val47, idmap));
    in.push_back(db.merge_snapshot(1, &node_ctx2, 0, nullptr,  nullptr,  idmap));
    in.push_back(db.merge_snapshot(0, nullptr,    1, &attr_id, &v_val47, idmap));
    in.push_back(db.merge_snapshot(0, nullptr,    0, nullptr,  nullptr,  idmap));

    QuerySpec::Condition ex_1  { QuerySpec::Condition::Op::Exist,    "ctx.1", "" };
    QuerySpec::Condition ex_2  { QuerySpec::Condition::Op::Exist,    "ctx.2", "" };
    QuerySpec::Condition ex_3  { QuerySpec::Condition::Op::Exist,    "ctx.3", "" };
    QuerySpec::Condition ex_v  { QuerySpec::Condition::Op::Exist,    "val",   "" };
    QuerySpec::Condition nex_1 { QuerySpec::Condition::Op::NotExist, "ctx.1", "" };
    QuerySpec::Condition nex_v { QuerySpec::Condition::Op::NotExist, "val",   "" };

    {
        QuerySpec spec;

        spec.filter.selection = QuerySpec::FilterSelection::List;
        spec.filter.list.push_back(ex_1);

        RecordSelector filter(spec);
        std::vector<EntryList> result;

        for ( const EntryList& rec : in )
            if (filter.pass(db, rec))
                result.push_back(rec);

        EXPECT_EQ(static_cast<int>(result.size()), 5);

        for ( const EntryList& rec : result )
            EXPECT_TRUE(::has_attribute(rec, ctx1));
    }

    {
        QuerySpec spec;

        spec.filter.selection = QuerySpec::FilterSelection::List;
        spec.filter.list.push_back(ex_3);

        RecordSelector filter(spec);
        std::vector<EntryList> result;

        for ( const EntryList& rec : in )
            if (filter.pass(db, rec))
                result.push_back(rec);

        EXPECT_EQ(static_cast<int>(result.size()), 0);

        for ( const EntryList& rec : result )
            EXPECT_FALSE(::has_attribute(rec, ctx3));
    }

    {
        QuerySpec spec;

        spec.filter.selection = QuerySpec::FilterSelection::List;
        spec.filter.list.push_back(ex_2);

        RecordSelector filter(spec);
        std::vector<EntryList> result;

        for ( const EntryList& rec : in )
            if (filter.pass(db, rec))
                result.push_back(rec);

        EXPECT_EQ(static_cast<int>(result.size()), 2);

        for ( const EntryList& rec : result )
            EXPECT_TRUE(::has_attribute(rec, ctx2));
    }

    {
        QuerySpec spec;

        spec.filter.selection = QuerySpec::FilterSelection::List;
        spec.filter.list.push_back(ex_1);
        spec.filter.list.push_back(ex_v);

        RecordSelector filter(spec);
        std::vector<EntryList> result;

        for ( const EntryList& rec : in )
            if (filter.pass(db, rec))
                result.push_back(rec);

        EXPECT_EQ(static_cast<int>(result.size()), 3);

        for ( const EntryList& rec : result )
            EXPECT_TRUE(::has_attribute(rec, ctx1) && ::has_attribute(rec, val_attr));
    }
    
    {
        QuerySpec spec;

        spec.filter.selection = QuerySpec::FilterSelection::List;
        spec.filter.list.push_back(nex_1);

        RecordSelector filter(spec);
        std::vector<EntryList> result;

        for ( const EntryList& rec : in )
            if (filter.pass(db, rec))
                result.push_back(rec);

        EXPECT_EQ(static_cast<int>(result.size()), 2);

        for ( const EntryList& rec : result )
            EXPECT_TRUE(!::has_attribute(rec, ctx1));
    }

    {
        QuerySpec spec;

        spec.filter.selection = QuerySpec::FilterSelection::List;
        spec.filter.list.push_back(ex_1);
        spec.filter.list.push_back(nex_v);

        RecordSelector filter(spec);
        std::vector<EntryList> result;

        for ( const EntryList& rec : in )
            if (filter.pass(db, rec))
                result.push_back(rec);

        EXPECT_EQ(static_cast<int>(result.size()), 2);

        for ( const EntryList& rec : result )
            EXPECT_TRUE(::has_attribute(rec, ctx1) && !::has_attribute(rec, val_attr));
    }
}

TEST(RecordFilterTest, TestEqual) {
    CaliperMetadataDB db;
    IdMap             idmap;

    // create some context attributes
    
    Attribute ctx1 =
        db.create_attribute("ctx.1", CALI_TYPE_STRING, CALI_ATTR_DEFAULT);
    Attribute ctx2 = 
        db.create_attribute("ctx.2", CALI_TYPE_INT,    CALI_ATTR_DEFAULT);
    Attribute val_attr =
        db.create_attribute("val",   CALI_TYPE_INT,    CALI_ATTR_ASVALUE);

    // make some nodes
    
    const struct NodeInfo {
        cali_id_t node_id;
        cali_id_t attr_id;
        cali_id_t prnt_id;
        Variant   data;
    } test_nodes[] = {
        { 100, ctx1.id(), CALI_INV_ID, Variant(CALI_TYPE_STRING, "outer", 5) },
        { 101, ctx2.id(), 100,         Variant(42)                           },
        { 102, ctx1.id(), 101,         Variant(CALI_TYPE_STRING, "inner", 5) }
    };

    for ( const NodeInfo& nI : test_nodes )
        db.merge_node(nI.node_id, nI.attr_id, nI.prnt_id, nI.data, idmap);

    Variant v_val47(47);

    cali_id_t node_ctx1 = 100;
    cali_id_t node_ctx2 = 102;
    cali_id_t attr_id   = val_attr.id();

    std::vector<EntryList> in;

    in.push_back(db.merge_snapshot(1, &node_ctx1, 1, &attr_id, &v_val47, idmap));
    in.push_back(db.merge_snapshot(1, &node_ctx1, 0, nullptr,  nullptr,  idmap));
    in.push_back(db.merge_snapshot(1, &node_ctx1, 1, &attr_id, &v_val47, idmap));
    in.push_back(db.merge_snapshot(1, &node_ctx2, 1, &attr_id, &v_val47, idmap));
    in.push_back(db.merge_snapshot(1, &node_ctx2, 0, nullptr,  nullptr,  idmap));
    in.push_back(db.merge_snapshot(0, nullptr,    1, &attr_id, &v_val47, idmap));
    in.push_back(db.merge_snapshot(0, nullptr,    0, nullptr,  nullptr,  idmap));

    QuerySpec::Condition eq_1a  { QuerySpec::Condition::Op::Equal,    "ctx.1", "outer" };
    QuerySpec::Condition eq_1b  { QuerySpec::Condition::Op::Equal,    "ctx.1", "inner" };
    QuerySpec::Condition eq_1c  { QuerySpec::Condition::Op::Equal,    "ctx.1", "inner" };
    QuerySpec::Condition eq_2a  { QuerySpec::Condition::Op::Equal,    "ctx.2", "42"    };
    QuerySpec::Condition eq_2b  { QuerySpec::Condition::Op::Equal,    "ctx.2", "142"   };
    QuerySpec::Condition eq_va  { QuerySpec::Condition::Op::Equal,    "val",   "47"    };
    QuerySpec::Condition eq_vb  { QuerySpec::Condition::Op::Equal,    "val",   "147"   };
    QuerySpec::Condition neq_1a { QuerySpec::Condition::Op::NotEqual, "ctx.1", "outer" };
    QuerySpec::Condition neq_2a { QuerySpec::Condition::Op::NotEqual, "val",   "42"    };
    QuerySpec::Condition neq_va { QuerySpec::Condition::Op::NotEqual, "val",   "47"    };

    {
        QuerySpec spec;

        spec.filter.selection = QuerySpec::FilterSelection::List;
        spec.filter.list.push_back(eq_1a);

        RecordSelector filter(spec);
        std::vector<EntryList> result;

        for ( const EntryList& rec : in )
            if (filter.pass(db, rec))
                result.push_back(rec);

        EXPECT_EQ(static_cast<int>(result.size()), 5);

        for ( const EntryList& rec : result )
            EXPECT_TRUE(::has_entry(rec, ctx1, Variant(CALI_TYPE_STRING, "outer", 5)));
    }

    {
        QuerySpec spec;

        spec.filter.selection = QuerySpec::FilterSelection::List;
        spec.filter.list.push_back(eq_va);

        RecordSelector filter(spec);
        std::vector<EntryList> result;

        for ( const EntryList& rec : in )
            if (filter.pass(db, rec))
                result.push_back(rec);

        EXPECT_EQ(static_cast<int>(result.size()), 4);

        for ( const EntryList& rec : result )
            EXPECT_TRUE(::has_entry(rec, val_attr, v_val47));
    }

    {
        QuerySpec spec;

        spec.filter.selection = QuerySpec::FilterSelection::List;
        spec.filter.list.push_back(eq_vb);

        RecordSelector filter(spec);
        std::vector<EntryList> result;

        for ( const EntryList& rec : in )
            if (filter.pass(db, rec))
                result.push_back(rec);

        EXPECT_EQ(static_cast<int>(result.size()), 0);
    }

    {
        QuerySpec spec;

        spec.filter.selection = QuerySpec::FilterSelection::List;
        spec.filter.list.push_back(neq_1a);

        RecordSelector filter(spec);
        std::vector<EntryList> result;

        for ( const EntryList& rec : in )
            if (filter.pass(db, rec))
                result.push_back(rec);

        EXPECT_EQ(static_cast<int>(result.size()), 2);

        for ( const EntryList& rec : result )
            EXPECT_FALSE(::has_entry(rec, ctx1, Variant(CALI_TYPE_STRING, "outer", 5)));
    }

    {
        QuerySpec spec;

        spec.filter.selection = QuerySpec::FilterSelection::List;
        spec.filter.list.push_back(eq_2a);
        spec.filter.list.push_back(neq_va);

        RecordSelector filter(spec);
        std::vector<EntryList> result;

        for ( const EntryList& rec : in )
            if (filter.pass(db, rec))
                result.push_back(rec);

        EXPECT_EQ(static_cast<int>(result.size()), 1);

        for ( const EntryList& rec : result ) {
            EXPECT_TRUE (::has_entry(rec, ctx2,     Variant(42)));
            EXPECT_FALSE(::has_entry(rec, val_attr, v_val47));
        }
    }
}

TEST(RecordFilterTest, TestLess) {
    CaliperMetadataDB db;
    IdMap             idmap;

    // create some context attributes
    
    Attribute ctx1 =
        db.create_attribute("ctx.1", CALI_TYPE_STRING, CALI_ATTR_DEFAULT);
    Attribute ctx2 = 
        db.create_attribute("ctx.2", CALI_TYPE_INT,    CALI_ATTR_DEFAULT);
    Attribute val_attr =
        db.create_attribute("val",   CALI_TYPE_INT,    CALI_ATTR_ASVALUE);

    // make some nodes
    
    const struct NodeInfo {
        cali_id_t node_id;
        cali_id_t attr_id;
        cali_id_t prnt_id;
        Variant   data;
    } test_nodes[] = {
        { 100, ctx1.id(), CALI_INV_ID, Variant(CALI_TYPE_STRING, "outer", 5) },
        { 101, ctx2.id(), 100,         Variant(42)                           },
        { 102, ctx1.id(), 101,         Variant(CALI_TYPE_STRING, "inner", 5) }
    };

    for ( const NodeInfo& nI : test_nodes )
        db.merge_node(nI.node_id, nI.attr_id, nI.prnt_id, nI.data, idmap);

    Variant v_val47(47);
    Variant v_val42(42);

    cali_id_t node_ctx1 = 100;
    cali_id_t node_ctx2 = 102;
    cali_id_t attr_id   = val_attr.id();

    std::vector<EntryList> in;

    in.push_back(db.merge_snapshot(1, &node_ctx1, 1, &attr_id, &v_val47, idmap));
    in.push_back(db.merge_snapshot(1, &node_ctx1, 0, nullptr,  nullptr,  idmap));
    in.push_back(db.merge_snapshot(1, &node_ctx1, 1, &attr_id, &v_val47, idmap));
    in.push_back(db.merge_snapshot(1, &node_ctx2, 1, &attr_id, &v_val47, idmap));
    in.push_back(db.merge_snapshot(1, &node_ctx2, 0, nullptr,  nullptr,  idmap));
    in.push_back(db.merge_snapshot(0, nullptr,    1, &attr_id, &v_val42, idmap));
    in.push_back(db.merge_snapshot(0, nullptr,    0, nullptr,  nullptr,  idmap));

    QuerySpec::Condition ls_45  { QuerySpec::Condition::Op::LessThan,    "val",   "45"    };
    QuerySpec::Condition ls_50  { QuerySpec::Condition::Op::LessThan,    "val",   "50"    };
    QuerySpec::Condition le_42  { QuerySpec::Condition::Op::LessOrEqual, "val",   "42"    };

    {
        QuerySpec spec;

        spec.filter.selection = QuerySpec::FilterSelection::List;
        spec.filter.list.push_back(ls_45);

        RecordSelector filter(spec);
        std::vector<EntryList> result;

        for ( const EntryList& rec : in )
            if (filter.pass(db, rec))
                result.push_back(rec);

        EXPECT_EQ(static_cast<int>(result.size()), 1);

        for ( const EntryList& rec : result ) {
            EXPECT_TRUE (::has_entry(rec, val_attr, v_val42));
            EXPECT_FALSE(::has_entry(rec, val_attr, v_val47));
        }
    }

    {
        QuerySpec spec;

        spec.filter.selection = QuerySpec::FilterSelection::List;
        spec.filter.list.push_back(ls_50);

        RecordSelector filter(spec);
        std::vector<EntryList> result;

        for ( const EntryList& rec : in )
            if (filter.pass(db, rec))
                result.push_back(rec);

        EXPECT_EQ(static_cast<int>(result.size()), 4);

        for ( const EntryList& rec : result )
            EXPECT_TRUE (::has_entry(rec, val_attr, v_val42) || ::has_entry(rec, val_attr, v_val47));
    }

    {
        QuerySpec spec;

        spec.filter.selection = QuerySpec::FilterSelection::List;
        spec.filter.list.push_back(le_42);

        RecordSelector filter(spec);
        std::vector<EntryList> result;

        for ( const EntryList& rec : in )
            if (filter.pass(db, rec))
                result.push_back(rec);

        EXPECT_EQ(static_cast<int>(result.size()), 1);

        for ( const EntryList& rec : result ) {
            EXPECT_TRUE (::has_entry(rec, val_attr, v_val42));
            EXPECT_FALSE(::has_entry(rec, val_attr, v_val47));
        }
    }
}

TEST(RecordFilterTest, TestGreater) {
    CaliperMetadataDB db;
    IdMap             idmap;

    // create some context attributes
    
    Attribute ctx1 =
        db.create_attribute("ctx.1", CALI_TYPE_STRING, CALI_ATTR_DEFAULT);
    Attribute ctx2 = 
        db.create_attribute("ctx.2", CALI_TYPE_INT,    CALI_ATTR_DEFAULT);
    Attribute val_attr =
        db.create_attribute("val",   CALI_TYPE_INT,    CALI_ATTR_ASVALUE);

    // make some nodes
    
    const struct NodeInfo {
        cali_id_t node_id;
        cali_id_t attr_id;
        cali_id_t prnt_id;
        Variant   data;
    } test_nodes[] = {
        { 100, ctx1.id(), CALI_INV_ID, Variant(CALI_TYPE_STRING, "outer", 5) },
        { 101, ctx2.id(), 100,         Variant(42)                           },
        { 102, ctx1.id(), 101,         Variant(CALI_TYPE_STRING, "inner", 5) }
    };

    for ( const NodeInfo& nI : test_nodes )
        db.merge_node(nI.node_id, nI.attr_id, nI.prnt_id, nI.data, idmap);

    Variant v_val47(47);
    Variant v_val42(42);

    cali_id_t node_ctx1 = 100;
    cali_id_t node_ctx2 = 102;
    cali_id_t attr_id   = val_attr.id();

    std::vector<EntryList> in;

    in.push_back(db.merge_snapshot(1, &node_ctx1, 1, &attr_id, &v_val47, idmap));
    in.push_back(db.merge_snapshot(1, &node_ctx1, 0, nullptr,  nullptr,  idmap));
    in.push_back(db.merge_snapshot(1, &node_ctx1, 1, &attr_id, &v_val47, idmap));
    in.push_back(db.merge_snapshot(1, &node_ctx2, 1, &attr_id, &v_val47, idmap));
    in.push_back(db.merge_snapshot(1, &node_ctx2, 0, nullptr,  nullptr,  idmap));
    in.push_back(db.merge_snapshot(0, nullptr,    1, &attr_id, &v_val42, idmap));
    in.push_back(db.merge_snapshot(0, nullptr,    0, nullptr,  nullptr,  idmap));

    QuerySpec::Condition gt_45  { QuerySpec::Condition::Op::GreaterThan,    "val",   "45"    };
    QuerySpec::Condition gt_40  { QuerySpec::Condition::Op::GreaterThan,    "val",   "40"    };
    QuerySpec::Condition ge_47  { QuerySpec::Condition::Op::GreaterOrEqual, "val",   "47"    };

    {
        QuerySpec spec;

        spec.filter.selection = QuerySpec::FilterSelection::List;
        spec.filter.list.push_back(gt_45);

        RecordSelector filter(spec);
        std::vector<EntryList> result;

        for ( const EntryList& rec : in )
            if (filter.pass(db, rec))
                result.push_back(rec);

        EXPECT_EQ(static_cast<int>(result.size()), 3);

        for ( const EntryList& rec : result ) {
            EXPECT_TRUE (::has_entry(rec, val_attr, v_val47));
            EXPECT_FALSE(::has_entry(rec, val_attr, v_val42));
        }
    }

    {
        QuerySpec spec;

        spec.filter.selection = QuerySpec::FilterSelection::List;
        spec.filter.list.push_back(gt_40);

        RecordSelector filter(spec);
        std::vector<EntryList> result;

        for ( const EntryList& rec : in )
            if (filter.pass(db, rec))
                result.push_back(rec);

        EXPECT_EQ(static_cast<int>(result.size()), 4);

        for ( const EntryList& rec : result )
            EXPECT_TRUE (::has_entry(rec, val_attr, v_val42) || ::has_entry(rec, val_attr, v_val47));
    }

    {
        QuerySpec spec;

        spec.filter.selection = QuerySpec::FilterSelection::List;
        spec.filter.list.push_back(ge_47);

        RecordSelector filter(spec);
        std::vector<EntryList> result;

        for ( const EntryList& rec : in )
            if (filter.pass(db, rec))
                result.push_back(rec);

        EXPECT_EQ(static_cast<int>(result.size()), 3);

        for ( const EntryList& rec : result ) {
            EXPECT_TRUE (::has_entry(rec, val_attr, v_val47));
            EXPECT_FALSE(::has_entry(rec, val_attr, v_val42));
        }
    }
}
