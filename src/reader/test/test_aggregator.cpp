#include "caliper/reader/Aggregator.h"

#include "caliper/reader/CaliperMetadataDB.h"

#include "caliper/common/Node.h"

#include <gtest/gtest.h>

using namespace cali;

namespace
{

QuerySpec::AggregationOp
make_op(const char* name, const char* arg = nullptr)
{
    QuerySpec::AggregationOp op;    
    const QuerySpec::FunctionSignature* p = Aggregator::aggregation_defs();

    for ( ; p && p->name && (0 != strcmp(p->name, name)); ++p )
        ;

    if (p->name) {
        op.op = *p;

        if (arg)
            op.args.push_back(arg);
    }

    return op;
}

std::map<cali_id_t, cali::Entry>
make_dict_from_entrylist(const EntryList& list)
{
    std::map<cali_id_t, Entry> dict;
    
    for ( const Entry& e : list )
        dict[e.attribute()] = e;

    return dict;
}

} // namespace


TEST(AggregatorTest, DefaultKeyCountOpSpec) {
    //
    // --- setup
    //
    
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
        { 100, ctx1.id(), CALI_INV_ID, Variant(CALI_TYPE_STRING, "outer", 6) },
        { 101, ctx2.id(), 100,         Variant(42)                           },
        { 102, ctx1.id(), 101,         Variant(CALI_TYPE_STRING, "inner", 6) }
    };

    const Node* node = nullptr;

    for ( const NodeInfo& nI : test_nodes )
        node = db.merge_node(nI.node_id, nI.attr_id, nI.prnt_id, nI.data, idmap);

    //
    // --- Test with default key and count kernel
    //

    // create spec with default key and aggregator
        
    QuerySpec spec;

    spec.aggregation_key.selection = QuerySpec::SelectionList<std::string>::Default;
    
    spec.aggregation_ops.selection = QuerySpec::SelectionList<QuerySpec::AggregationOp>::List;
    spec.aggregation_ops.list.push_back(::make_op("count"));

    Aggregator a(spec);
    
    // add some entries

    Variant   v_val(47);
    
    cali_id_t node_ctx1 = 102;
    cali_id_t node_ctx2 = 101;
    cali_id_t val_id    = val_attr.id();

    a.add(db, db.merge_snapshot(1, &node_ctx1, 1, &val_id, &v_val,  idmap));
    a.add(db, db.merge_snapshot(1, &node_ctx1, 0, nullptr, nullptr, idmap));
    a.add(db, db.merge_snapshot(1, &node_ctx1, 1, &val_id, &v_val,  idmap));
    a.add(db, db.merge_snapshot(1, &node_ctx2, 1, &val_id, &v_val,  idmap));
    a.add(db, db.merge_snapshot(1, &node_ctx2, 0, nullptr, nullptr, idmap));
    a.add(db, db.merge_snapshot(0, nullptr,    1, &val_id, &v_val,  idmap));
    a.add(db, db.merge_snapshot(0, nullptr,    0, nullptr, nullptr, idmap));

    std::vector<EntryList> resdb;

    a.flush(db, [&resdb](CaliperMetadataAccessInterface&, const EntryList& list) {
            resdb.push_back(list);
        });

    Attribute count_attr = db.get_attribute("aggregate.count");

    ASSERT_NE(count_attr, Attribute::invalid);

    // check results
    
    EXPECT_EQ(resdb.size(), 3); // one entry for 101, 102, (empty key)

    int rescount = 0;

    std::for_each(resdb.begin(), resdb.end(), [&](const EntryList& list){
            auto dict = make_dict_from_entrylist(list);
            int  aggr = dict[count_attr.id()].value().to_int();

            if (dict[ctx1.id()].value() == Variant(CALI_TYPE_STRING, "inner", 6)) {
                EXPECT_EQ(aggr, 3);
                EXPECT_EQ(static_cast<int>(list.size()), 2);
                ++rescount;
            } else if (dict[ctx2.id()].value() == Variant(42)) {
                EXPECT_EQ(aggr, 2);
                EXPECT_EQ(static_cast<int>(list.size()), 2);
                ++rescount;
            } else {
                EXPECT_EQ(aggr, 2);
                ++rescount;
            }
        });

    EXPECT_EQ(rescount, 3);
}

TEST(AggregatorTest, DefaultKeySumOpSpec) {
    //
    // --- setup
    //
    
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
        { 100, ctx1.id(), CALI_INV_ID, Variant(CALI_TYPE_STRING, "outer", 6) },
        { 101, ctx2.id(), 100,         Variant(42)                           },
        { 102, ctx1.id(), 101,         Variant(CALI_TYPE_STRING, "inner", 6) }
    };

    const Node* node = nullptr;

    for ( const NodeInfo& nI : test_nodes )
        node = db.merge_node(nI.node_id, nI.attr_id, nI.prnt_id, nI.data, idmap);

    //
    // --- Test with default key, count and sum kernel
    //
    
    // create spec with default key and aggregator
        
    QuerySpec spec;

    spec.aggregation_key.selection = QuerySpec::SelectionList<std::string>::Default;
    
    spec.aggregation_ops.selection = QuerySpec::SelectionList<QuerySpec::AggregationOp>::List;
    spec.aggregation_ops.list.push_back(::make_op("count"));
    spec.aggregation_ops.list.push_back(::make_op("sum", "val"));

    ASSERT_EQ(static_cast<int>(spec.aggregation_ops.list.size()), 2);
    
    ASSERT_STREQ(spec.aggregation_ops.list[0].op.name, "count"); // see if kernel lookup went OK
    ASSERT_STREQ(spec.aggregation_ops.list[1].op.name, "sum");

    Aggregator a(spec);
    
    // add some entries

    Variant   v_val(7);
    
    cali_id_t node_ctx1 = 102;
    cali_id_t node_ctx2 = 101;
    cali_id_t val_id    = val_attr.id();

    a.add(db, db.merge_snapshot(1, &node_ctx1, 1, &val_id, &v_val,  idmap));
    a.add(db, db.merge_snapshot(1, &node_ctx1, 0, nullptr, nullptr, idmap));
    a.add(db, db.merge_snapshot(1, &node_ctx1, 1, &val_id, &v_val,  idmap));
    a.add(db, db.merge_snapshot(1, &node_ctx2, 1, &val_id, &v_val,  idmap));
    a.add(db, db.merge_snapshot(1, &node_ctx2, 0, nullptr, nullptr, idmap));
    a.add(db, db.merge_snapshot(0, nullptr,    1, &val_id, &v_val,  idmap));
    a.add(db, db.merge_snapshot(0, nullptr,    0, nullptr, nullptr, idmap));

    std::vector<EntryList> resdb;

    a.flush(db, [&resdb](CaliperMetadataAccessInterface&, const EntryList& list) {
            resdb.push_back(list);
        });

    Attribute count_attr = db.get_attribute("aggregate.count");

    ASSERT_NE(count_attr, Attribute::invalid);

    // check results
    
    EXPECT_EQ(resdb.size(), 3); // one entry for 101, 102, (empty key)

    int rescount = 0;

    std::for_each(resdb.begin(), resdb.end(), [&](const EntryList& list){
            auto dict = make_dict_from_entrylist(list);
                
            int  aggr = dict[count_attr.id()].value().to_int();
            int  val  = dict[val_attr.id()  ].value().to_int();

            if (dict[ctx1.id()].value() == Variant(CALI_TYPE_STRING, "inner", 6)) {
                EXPECT_EQ(aggr,  3);
                EXPECT_EQ(val,  14);
                EXPECT_EQ(static_cast<int>(list.size()), 3);
                ++rescount;
            } else if (dict[ctx2.id()].value() == Variant(42)) {
                EXPECT_EQ(aggr,  2);
                EXPECT_EQ(val,   7);
                EXPECT_EQ(static_cast<int>(list.size()), 3);
                ++rescount;
            } else {
                EXPECT_EQ(aggr,  2);
                EXPECT_EQ(val,   7);
                ++rescount;
            }
        });

    EXPECT_EQ(rescount, 3);
}

TEST(AggregatorTest, SingleKeySumOpSpec) {
    //
    // --- setup
    //
    
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
        { 100, ctx1.id(), CALI_INV_ID, Variant(CALI_TYPE_STRING, "outer", 6) },
        { 101, ctx2.id(), 100,         Variant(42)                           },
        { 102, ctx1.id(), 101,         Variant(CALI_TYPE_STRING, "inner", 6) }
    };

    const Node* node = nullptr;

    for ( const NodeInfo& nI : test_nodes )
        node = db.merge_node(nI.node_id, nI.attr_id, nI.prnt_id, nI.data, idmap);

    //
    // --- Test with ctx2 key, count and sum kernel
    //

    QuerySpec spec;

    spec.aggregation_key.selection = QuerySpec::SelectionList<std::string>::List;
    spec.aggregation_key.list.push_back("ctx.2");
    
    spec.aggregation_ops.selection = QuerySpec::SelectionList<QuerySpec::AggregationOp>::List;
    spec.aggregation_ops.list.push_back(::make_op("count"));
    spec.aggregation_ops.list.push_back(::make_op("sum", "val"));

    Aggregator a(spec);
    
    // add some entries

    Variant   v_val(7);
    
    cali_id_t node_ctx1 = 102;
    cali_id_t node_ctx2 = 101;
    cali_id_t node_ctx3 = 100;
    cali_id_t val_id    = val_attr.id();

    a.add(db, db.merge_snapshot(1, &node_ctx1, 1, &val_id, &v_val,  idmap));
    a.add(db, db.merge_snapshot(1, &node_ctx1, 0, nullptr, nullptr, idmap));
    a.add(db, db.merge_snapshot(1, &node_ctx1, 1, &val_id, &v_val,  idmap));
    a.add(db, db.merge_snapshot(1, &node_ctx2, 1, &val_id, &v_val,  idmap));
    a.add(db, db.merge_snapshot(1, &node_ctx2, 0, nullptr, nullptr, idmap));
    a.add(db, db.merge_snapshot(0, nullptr,    1, &val_id, &v_val,  idmap));
    a.add(db, db.merge_snapshot(0, nullptr,    0, nullptr, nullptr, idmap));
    a.add(db, db.merge_snapshot(1, &node_ctx3, 1, &val_id, &v_val,  idmap));
    a.add(db, db.merge_snapshot(1, &node_ctx3, 0, nullptr, nullptr, idmap));
        
    std::vector<EntryList> resdb;

    a.flush(db, [&resdb](CaliperMetadataAccessInterface&, const EntryList& list) {
            resdb.push_back(list);
        });

    Attribute count_attr = db.get_attribute("aggregate.count");

    ASSERT_NE(count_attr, Attribute::invalid);

    // check results
    
    EXPECT_EQ(resdb.size(), 2); // one entry for 102, (empty key)

    int rescount = 0;

    std::for_each(resdb.begin(), resdb.end(), [&](const EntryList& list){
            auto dict = make_dict_from_entrylist(list);
                
            int  count = dict[count_attr.id()].value().to_int();
            int  val   = dict[val_attr.id()  ].value().to_int();

            if (dict[ctx2.id()].value() == Variant(42)) {
                EXPECT_EQ(count,  5);
                EXPECT_EQ(val,   21);
                EXPECT_EQ(static_cast<int>(list.size()), 3);
                ++rescount;
            } else {
                EXPECT_EQ(count,  4);
                EXPECT_EQ(val,   14);
                ++rescount;
            }
        });

    EXPECT_EQ(rescount, 2);
}
