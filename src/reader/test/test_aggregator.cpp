#include "caliper/reader/Aggregator.h"

#include "caliper/reader/CaliperMetadataDB.h"

#include "caliper/common/Node.h"

#include <gtest/gtest.h>

#include <algorithm>

using namespace cali;

namespace
{

QuerySpec::AggregationOp make_op(
    const char* name,
    const char* arg1 = nullptr,
    const char* arg2 = nullptr,
    const char* arg3 = nullptr
)
{
    QuerySpec::AggregationOp            op;
    const QuerySpec::FunctionSignature* p = Aggregator::aggregation_defs();

    for (; p && p->name && (0 != strcmp(p->name, name)); ++p)
        ;

    if (p->name) {
        op.op = *p;

        if (arg1)
            op.args.push_back(arg1);
        if (arg2)
            op.args.push_back(arg2);
        if (arg3)
            op.args.push_back(arg3);
    }

    return op;
}

std::map<cali_id_t, cali::Entry> make_dict_from_entrylist(const EntryList& list)
{
    std::map<cali_id_t, Entry> dict;

    for (const Entry& e : list)
        dict[e.attribute()] = e;

    return dict;
}

} // namespace

TEST(AggregatorTest, DefaultKeyCountOpSpec)
{
    //
    // --- setup
    //

    CaliperMetadataDB db;
    IdMap             idmap;

    // create some context attributes

    Attribute ctx1     = db.create_attribute("ctx.1", CALI_TYPE_STRING, CALI_ATTR_DEFAULT);
    Attribute ctx2     = db.create_attribute("ctx.2", CALI_TYPE_INT, CALI_ATTR_DEFAULT);
    Attribute val_attr = db.create_attribute("val", CALI_TYPE_INT, CALI_ATTR_ASVALUE);

    // make some nodes

    const struct NodeInfo {
        cali_id_t node_id;
        cali_id_t attr_id;
        cali_id_t prnt_id;
        Variant   data;
    } test_nodes[] = { { 100, ctx1.id(), CALI_INV_ID, Variant("outer") },
                       { 101, ctx2.id(), 100, Variant(42) },
                       { 102, ctx1.id(), 101, Variant("inner") } };

    for (const NodeInfo& nI : test_nodes)
        db.merge_node(nI.node_id, nI.attr_id, nI.prnt_id, nI.data, idmap);

    //
    // --- Test with default key and count kernel
    //

    // create spec with default key and aggregator

    QuerySpec spec;

    spec.groupby.selection = QuerySpec::SelectionList<std::string>::Default;

    spec.aggregate.selection = QuerySpec::SelectionList<QuerySpec::AggregationOp>::List;
    spec.aggregate.list.push_back(::make_op("count"));

    Aggregator a(spec);

    // add some entries

    Variant v_val(47);

    cali_id_t node_ctx1 = 102;
    cali_id_t node_ctx2 = 101;
    cali_id_t val_id    = val_attr.id();

    a.add(db, db.merge_snapshot(1, &node_ctx1, 1, &val_id, &v_val, idmap));
    a.add(db, db.merge_snapshot(1, &node_ctx1, 0, nullptr, nullptr, idmap));
    a.add(db, db.merge_snapshot(1, &node_ctx1, 1, &val_id, &v_val, idmap));
    a.add(db, db.merge_snapshot(1, &node_ctx2, 1, &val_id, &v_val, idmap));
    a.add(db, db.merge_snapshot(1, &node_ctx2, 0, nullptr, nullptr, idmap));
    a.add(db, db.merge_snapshot(0, nullptr, 1, &val_id, &v_val, idmap));
    a.add(db, db.merge_snapshot(0, nullptr, 0, nullptr, nullptr, idmap));

    std::vector<EntryList> resdb;

    a.flush(db, [&resdb](CaliperMetadataAccessInterface&, const EntryList& list) { resdb.push_back(list); });

    Attribute count_attr = db.get_attribute("count");

    ASSERT_TRUE(count_attr);

    // check results

    EXPECT_EQ(resdb.size(), 3); // one entry for 101, 102, (empty key)

    int rescount = 0;

    std::for_each(resdb.begin(), resdb.end(), [&](const EntryList& list) {
        auto dict = make_dict_from_entrylist(list);
        int  aggr = dict[count_attr.id()].value().to_int();

        if (dict[ctx1.id()].value() == Variant("inner")) {
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

TEST(AggregatorTest, DefaultKeySumOpSpec)
{
    //
    // --- setup
    //

    CaliperMetadataDB db;
    IdMap             idmap;

    // create some context attributes

    Attribute ctx1     = db.create_attribute("ctx.1", CALI_TYPE_STRING, CALI_ATTR_DEFAULT);
    Attribute ctx2     = db.create_attribute("ctx.2", CALI_TYPE_INT, CALI_ATTR_DEFAULT);
    Attribute val_attr = db.create_attribute("val", CALI_TYPE_INT, CALI_ATTR_ASVALUE);

    // make some nodes

    const struct NodeInfo {
        cali_id_t node_id;
        cali_id_t attr_id;
        cali_id_t prnt_id;
        Variant   data;
    } test_nodes[] = { { 100, ctx1.id(), CALI_INV_ID, Variant("outer") },
                       { 101, ctx2.id(), 100, Variant(42) },
                       { 102, ctx1.id(), 101, Variant("inner") } };

    for (const NodeInfo& nI : test_nodes)
        db.merge_node(nI.node_id, nI.attr_id, nI.prnt_id, nI.data, idmap);

    //
    // --- Test with default key, count and sum kernel
    //

    // create spec with default key and aggregator

    QuerySpec spec;

    spec.groupby.selection = QuerySpec::SelectionList<std::string>::Default;

    spec.aggregate.selection = QuerySpec::SelectionList<QuerySpec::AggregationOp>::List;
    spec.aggregate.list.push_back(::make_op("count"));
    spec.aggregate.list.push_back(::make_op("sum", "val"));

    ASSERT_EQ(static_cast<int>(spec.aggregate.list.size()), 2);

    ASSERT_STREQ(spec.aggregate.list[0].op.name, "count"); // see if kernel lookup went OK
    ASSERT_STREQ(spec.aggregate.list[1].op.name, "sum");

    Aggregator a(spec);

    // add some entries

    Variant v_val(7);

    cali_id_t node_ctx1 = 102;
    cali_id_t node_ctx2 = 101;
    cali_id_t val_id    = val_attr.id();

    a.add(db, db.merge_snapshot(1, &node_ctx1, 1, &val_id, &v_val, idmap));
    a.add(db, db.merge_snapshot(1, &node_ctx1, 0, nullptr, nullptr, idmap));
    a.add(db, db.merge_snapshot(1, &node_ctx1, 1, &val_id, &v_val, idmap));
    a.add(db, db.merge_snapshot(1, &node_ctx2, 1, &val_id, &v_val, idmap));
    a.add(db, db.merge_snapshot(1, &node_ctx2, 0, nullptr, nullptr, idmap));
    a.add(db, db.merge_snapshot(0, nullptr, 1, &val_id, &v_val, idmap));
    a.add(db, db.merge_snapshot(0, nullptr, 0, nullptr, nullptr, idmap));

    std::vector<EntryList> resdb;

    a.flush(db, [&resdb](CaliperMetadataAccessInterface&, const EntryList& list) { resdb.push_back(list); });

    Attribute count_attr = db.get_attribute("count");
    Attribute sum_attr   = db.get_attribute("sum#val");

    ASSERT_TRUE(count_attr);
    ASSERT_TRUE(sum_attr);

    // check results

    EXPECT_EQ(resdb.size(), 3); // one entry for 101, 102, (empty key)

    int rescount = 0;

    std::for_each(resdb.begin(), resdb.end(), [&](const EntryList& list) {
        auto dict = make_dict_from_entrylist(list);

        int aggr = dict[count_attr.id()].value().to_int();
        int val  = dict[sum_attr.id()].value().to_int();

        if (dict[ctx1.id()].value() == Variant("inner")) {
            EXPECT_EQ(aggr, 3);
            EXPECT_EQ(val, 14);
            EXPECT_EQ(static_cast<int>(list.size()), 3);
            ++rescount;
        } else if (dict[ctx2.id()].value() == Variant(42)) {
            EXPECT_EQ(aggr, 2);
            EXPECT_EQ(val, 7);
            EXPECT_EQ(static_cast<int>(list.size()), 3);
            ++rescount;
        } else {
            EXPECT_EQ(aggr, 2);
            EXPECT_EQ(val, 7);
            ++rescount;
        }
    });

    EXPECT_EQ(rescount, 3);
}

TEST(AggregatorTest, SingleKeySumOpSpec)
{
    //
    // --- setup
    //

    CaliperMetadataDB db;
    IdMap             idmap;

    // create some context attributes

    Attribute ctx1     = db.create_attribute("ctx.1", CALI_TYPE_STRING, CALI_ATTR_DEFAULT);
    Attribute ctx2     = db.create_attribute("ctx.2", CALI_TYPE_INT, CALI_ATTR_DEFAULT);
    Attribute val_attr = db.create_attribute("val", CALI_TYPE_INT, CALI_ATTR_ASVALUE);

    // make some nodes

    const struct NodeInfo {
        cali_id_t node_id;
        cali_id_t attr_id;
        cali_id_t prnt_id;
        Variant   data;
    } test_nodes[] = { { 100, ctx1.id(), CALI_INV_ID, Variant("outer") },
                       { 101, ctx2.id(), 100, Variant(42) },
                       { 102, ctx1.id(), 101, Variant("inner") } };

    for (const NodeInfo& nI : test_nodes)
        db.merge_node(nI.node_id, nI.attr_id, nI.prnt_id, nI.data, idmap);

    //
    // --- Test with ctx2 key, count and sum kernel
    //

    QuerySpec spec;

    spec.groupby.selection = QuerySpec::SelectionList<std::string>::List;
    spec.groupby.list.push_back("ctx.2");

    spec.aggregate.selection = QuerySpec::SelectionList<QuerySpec::AggregationOp>::List;
    spec.aggregate.list.push_back(::make_op("count"));
    spec.aggregate.list.push_back(::make_op("sum", "val"));

    ASSERT_EQ(static_cast<int>(spec.aggregate.list.size()), 2);

    ASSERT_STREQ(spec.aggregate.list[0].op.name, "count"); // see if kernel lookup went OK
    ASSERT_STREQ(spec.aggregate.list[1].op.name, "sum");

    Aggregator a(spec);

    // add some entries

    Variant v_val(7);

    cali_id_t node_ctx1 = 102;
    cali_id_t node_ctx2 = 101;
    cali_id_t node_ctx3 = 100;
    cali_id_t val_id    = val_attr.id();

    a.add(db, db.merge_snapshot(1, &node_ctx1, 1, &val_id, &v_val, idmap));
    a.add(db, db.merge_snapshot(1, &node_ctx1, 0, nullptr, nullptr, idmap));
    a.add(db, db.merge_snapshot(1, &node_ctx1, 1, &val_id, &v_val, idmap));
    a.add(db, db.merge_snapshot(1, &node_ctx2, 1, &val_id, &v_val, idmap));
    a.add(db, db.merge_snapshot(1, &node_ctx2, 0, nullptr, nullptr, idmap));
    a.add(db, db.merge_snapshot(0, nullptr, 1, &val_id, &v_val, idmap));
    a.add(db, db.merge_snapshot(0, nullptr, 0, nullptr, nullptr, idmap));
    a.add(db, db.merge_snapshot(1, &node_ctx3, 1, &val_id, &v_val, idmap));
    a.add(db, db.merge_snapshot(1, &node_ctx3, 0, nullptr, nullptr, idmap));

    std::vector<EntryList> resdb;

    a.flush(db, [&resdb](CaliperMetadataAccessInterface&, const EntryList& list) { resdb.push_back(list); });

    Attribute count_attr = db.get_attribute("count");
    Attribute sum_attr   = db.get_attribute("sum#val");

    ASSERT_TRUE(count_attr);

    // check results

    EXPECT_EQ(resdb.size(), 2); // one entry for 102, (empty key)

    int rescount = 0;

    std::for_each(resdb.begin(), resdb.end(), [&](const EntryList& list) {
        auto dict = make_dict_from_entrylist(list);

        int count = dict[count_attr.id()].value().to_int();
        int val   = dict[sum_attr.id()].value().to_int();

        if (dict[ctx2.id()].value() == Variant(42)) {
            EXPECT_EQ(count, 5);
            EXPECT_EQ(val, 21);
            EXPECT_EQ(static_cast<int>(list.size()), 3);
            ++rescount;
        } else {
            EXPECT_EQ(count, 4);
            EXPECT_EQ(val, 14);
            ++rescount;
        }
    });

    EXPECT_EQ(rescount, 2);
}

TEST(AggregatorTest, InclusiveSumOp)
{
    //
    // --- setup
    //

    CaliperMetadataDB db;
    IdMap             idmap;

    // create some context attributes

    Attribute ctx1     = db.create_attribute("ictx.1", CALI_TYPE_STRING, CALI_ATTR_NESTED);
    Attribute ctx2     = db.create_attribute("ictx.2", CALI_TYPE_INT, CALI_ATTR_DEFAULT);
    Attribute val_attr = db.create_attribute("val", CALI_TYPE_INT, CALI_ATTR_ASVALUE);

    // make some nodes

    const struct NodeInfo {
        cali_id_t node_id;
        cali_id_t attr_id;
        cali_id_t prnt_id;
        Variant   data;
    } test_nodes[] = { { 100, ctx1.id(), CALI_INV_ID, Variant("outer") }, { 101, ctx1.id(), 100, Variant("inner") } };

    for (const NodeInfo& nI : test_nodes)
        db.merge_node(nI.node_id, nI.attr_id, nI.prnt_id, nI.data, idmap);

    //
    // --- Test with ctx2 key, count and sum kernel
    //

    QuerySpec spec;

    spec.groupby.selection = QuerySpec::SelectionList<std::string>::Default;

    spec.aggregate.selection = QuerySpec::SelectionList<QuerySpec::AggregationOp>::List;
    spec.aggregate.list.push_back(::make_op("count"));
    spec.aggregate.list.push_back(::make_op("sum", "val"));
    spec.aggregate.list.push_back(::make_op("inclusive_sum", "val"));
    spec.aggregate.list.push_back(::make_op("inclusive_scale", "val", "2.0"));

    ASSERT_EQ(static_cast<int>(spec.aggregate.list.size()), 4);

    ASSERT_STREQ(spec.aggregate.list[0].op.name, "count"); // see if kernel lookup went OK
    ASSERT_STREQ(spec.aggregate.list[1].op.name, "sum");
    ASSERT_STREQ(spec.aggregate.list[2].op.name, "inclusive_sum");
    ASSERT_STREQ(spec.aggregate.list[3].op.name, "inclusive_scale");

    Aggregator a(spec);

    // add some entries

    Variant v_val(7);

    cali_id_t node_ctx1 = 101;
    cali_id_t node_ctx2 = 100;
    cali_id_t val_id    = val_attr.id();

    a.add(db, db.merge_snapshot(1, &node_ctx1, 1, &val_id, &v_val, idmap));
    a.add(db, db.merge_snapshot(1, &node_ctx1, 0, nullptr, nullptr, idmap));
    a.add(db, db.merge_snapshot(1, &node_ctx1, 1, &val_id, &v_val, idmap));
    a.add(db, db.merge_snapshot(0, nullptr, 1, &val_id, &v_val, idmap));
    a.add(db, db.merge_snapshot(0, nullptr, 0, nullptr, nullptr, idmap));
    a.add(db, db.merge_snapshot(1, &node_ctx2, 1, &val_id, &v_val, idmap));
    a.add(db, db.merge_snapshot(1, &node_ctx2, 0, nullptr, nullptr, idmap));

    std::vector<EntryList> resdb;

    a.flush(db, [&resdb](CaliperMetadataAccessInterface&, const EntryList& list) { resdb.push_back(list); });

    Attribute count_attr  = db.get_attribute("count");
    Attribute sum_attr    = db.get_attribute("sum#val");
    Attribute isum_attr   = db.get_attribute("inclusive#val");
    Attribute iscale_attr = db.get_attribute("iscale#val");

    ASSERT_TRUE(count_attr);
    ASSERT_TRUE(sum_attr);
    ASSERT_TRUE(isum_attr);
    ASSERT_TRUE(iscale_attr);

    // check results

    EXPECT_EQ(resdb.size(), 3); // one entry for 100, 101, (empty)

    int rescount = 0;

    std::for_each(resdb.begin(), resdb.end(), [&](const EntryList& list) {
        auto dict = make_dict_from_entrylist(list);

        int count  = dict[count_attr.id()].value().to_int();
        int val    = dict[sum_attr.id()].value().to_int();
        int ival   = dict[isum_attr.id()].value().to_int();
        int iscval = dict[iscale_attr.id()].value().to_int();

        if (dict[ctx1.id()].value() == Variant("inner")) {
            EXPECT_EQ(val, 14);
            EXPECT_EQ(ival, 14);
            EXPECT_EQ(iscval, 28);
            EXPECT_EQ(static_cast<int>(list.size()), 6);
            ++rescount;
        } else if (dict[ctx1.id()].value() == Variant("outer")) {
            EXPECT_EQ(val, 7);
            EXPECT_EQ(ival, 21);
            EXPECT_EQ(iscval, 42);
            ++rescount;
        }
    });

    EXPECT_EQ(rescount, 2);
}

TEST(AggregatorTest, InclusiveRatio)
{
    //
    // --- setup
    //

    CaliperMetadataDB db;
    IdMap             idmap;

    // create some context attributes

    Attribute ctx      = db.create_attribute("ctx", CALI_TYPE_STRING, CALI_ATTR_NESTED);
    Attribute num_attr = db.create_attribute("num", CALI_TYPE_INT, CALI_ATTR_ASVALUE);
    Attribute den_attr = db.create_attribute("den", CALI_TYPE_INT, CALI_ATTR_ASVALUE);

    // make some nodes

    const struct NodeInfo {
        cali_id_t node_id;
        cali_id_t attr_id;
        cali_id_t prnt_id;
        Variant   data;
    } test_nodes[] = { { 100, ctx.id(), CALI_INV_ID, Variant("outer") }, { 101, ctx.id(), 100, Variant("inner") } };

    for (const NodeInfo& nI : test_nodes)
        db.merge_node(nI.node_id, nI.attr_id, nI.prnt_id, nI.data, idmap);

    //
    // --- Test with ctx2 key, count and sum kernel
    //

    QuerySpec spec;

    spec.groupby.selection = QuerySpec::SelectionList<std::string>::Default;

    spec.aggregate.selection = QuerySpec::SelectionList<QuerySpec::AggregationOp>::List;
    spec.aggregate.list.push_back(::make_op("inclusive_ratio", "num", "den"));

    ASSERT_EQ(static_cast<int>(spec.aggregate.list.size()), 1);
    ASSERT_STREQ(spec.aggregate.list[0].op.name, "inclusive_ratio");

    Aggregator a(spec);

    // add some entries

    cali_id_t node_inner    = 101;
    cali_id_t node_outer    = 100;
    cali_id_t attr[2]       = { num_attr.id(), den_attr.id() };
    Variant   data_inner[2] = { Variant(10), Variant(10) };
    Variant   data_outer[2] = { Variant(10), Variant(5) };

    a.add(db, db.merge_snapshot(1, &node_outer, 2, attr, data_outer, idmap));
    a.add(db, db.merge_snapshot(1, &node_outer, 0, nullptr, nullptr, idmap));
    a.add(db, db.merge_snapshot(1, &node_outer, 2, attr, data_outer, idmap));
    a.add(db, db.merge_snapshot(0, nullptr, 2, attr, data_outer, idmap));
    a.add(db, db.merge_snapshot(0, nullptr, 0, nullptr, nullptr, idmap));
    a.add(db, db.merge_snapshot(1, &node_inner, 2, attr, data_inner, idmap));
    a.add(db, db.merge_snapshot(1, &node_inner, 0, nullptr, nullptr, idmap));

    std::vector<EntryList> resdb;

    a.flush(db, [&resdb](CaliperMetadataAccessInterface&, const EntryList& list) { resdb.push_back(list); });

    Attribute iratio_attr = db.get_attribute("iratio#num/den");

    ASSERT_TRUE(iratio_attr);

    // check results

    EXPECT_EQ(resdb.size(), 3); // one entry for 100, 101, (empty)

    int rescount = 0;

    std::for_each(resdb.begin(), resdb.end(), [&](const EntryList& list) {
        auto dict = make_dict_from_entrylist(list);

        double iratio = dict[iratio_attr.id()].value().to_double();

        if (dict[ctx.id()].value() == Variant("inner")) {
            EXPECT_DOUBLE_EQ(iratio, 1.0);
            ++rescount;
        } else if (dict[ctx.id()].value() == Variant("outer")) {
            EXPECT_DOUBLE_EQ(iratio, 1.5);
            ++rescount;
        }
    });

    EXPECT_EQ(rescount, 2);
}

TEST(AggregatorTest, NoneKeySumOpSpec)
{
    //
    // --- setup
    //

    CaliperMetadataDB db;
    IdMap             idmap;

    // create some context attributes

    Attribute ctx1     = db.create_attribute("ctx.1", CALI_TYPE_STRING, CALI_ATTR_DEFAULT);
    Attribute ctx2     = db.create_attribute("ctx.2", CALI_TYPE_INT, CALI_ATTR_DEFAULT);
    Attribute val_attr = db.create_attribute("val", CALI_TYPE_INT, CALI_ATTR_ASVALUE);

    // make some nodes

    const struct NodeInfo {
        cali_id_t node_id;
        cali_id_t attr_id;
        cali_id_t prnt_id;
        Variant   data;
    } test_nodes[] = { { 100, ctx1.id(), CALI_INV_ID, Variant("outer") },
                       { 101, ctx2.id(), 100, Variant(42) },
                       { 102, ctx1.id(), 101, Variant("inner") } };

    for (const NodeInfo& nI : test_nodes)
        db.merge_node(nI.node_id, nI.attr_id, nI.prnt_id, nI.data, idmap);

    //
    // --- Test with ctx2 key, count and sum kernel
    //

    QuerySpec spec;

    spec.groupby.selection = QuerySpec::SelectionList<std::string>::None;

    spec.aggregate.selection = QuerySpec::SelectionList<QuerySpec::AggregationOp>::List;
    spec.aggregate.list.push_back(::make_op("count"));
    spec.aggregate.list.push_back(::make_op("sum", "val"));

    ASSERT_EQ(static_cast<int>(spec.aggregate.list.size()), 2);

    ASSERT_STREQ(spec.aggregate.list[0].op.name, "count"); // see if kernel lookup went OK
    ASSERT_STREQ(spec.aggregate.list[1].op.name, "sum");

    Aggregator a(spec);

    // add some entries

    Variant v_val(7);

    cali_id_t node_ctx1 = 102;
    cali_id_t node_ctx2 = 101;
    cali_id_t node_ctx3 = 100;
    cali_id_t val_id    = val_attr.id();

    a.add(db, db.merge_snapshot(1, &node_ctx1, 1, &val_id, &v_val, idmap));
    a.add(db, db.merge_snapshot(1, &node_ctx1, 0, nullptr, nullptr, idmap));
    a.add(db, db.merge_snapshot(1, &node_ctx1, 1, &val_id, &v_val, idmap));
    a.add(db, db.merge_snapshot(1, &node_ctx2, 1, &val_id, &v_val, idmap));
    a.add(db, db.merge_snapshot(1, &node_ctx2, 0, nullptr, nullptr, idmap));
    a.add(db, db.merge_snapshot(0, nullptr, 1, &val_id, &v_val, idmap));
    a.add(db, db.merge_snapshot(0, nullptr, 0, nullptr, nullptr, idmap));
    a.add(db, db.merge_snapshot(1, &node_ctx3, 1, &val_id, &v_val, idmap));
    a.add(db, db.merge_snapshot(1, &node_ctx3, 0, nullptr, nullptr, idmap));

    std::vector<EntryList> resdb;

    a.flush(db, [&resdb](CaliperMetadataAccessInterface&, const EntryList& list) { resdb.push_back(list); });

    Attribute count_attr = db.get_attribute("count");
    Attribute sum_attr   = db.get_attribute("sum#val");

    ASSERT_TRUE(count_attr);
    ASSERT_TRUE(sum_attr);

    // check results

    EXPECT_EQ(resdb.size(), 1); // one for (empty key)

    auto dict = make_dict_from_entrylist(resdb.front());

    int count = dict[count_attr.id()].value().to_int();
    int val   = dict[sum_attr.id()].value().to_int();

    EXPECT_EQ(count, 9);
    EXPECT_EQ(val, 35);
}

TEST(AggregatorTest, StatisticsKernels)
{
    CaliperMetadataDB db;
    IdMap             idmap;

    // create some context attributes

    Attribute ctx      = db.create_attribute("ctx", CALI_TYPE_STRING, CALI_ATTR_DEFAULT);
    Attribute val_attr = db.create_attribute("val", CALI_TYPE_INT, CALI_ATTR_ASVALUE);

    // make some nodes

    const struct NodeInfo {
        cali_id_t node_id;
        cali_id_t attr_id;
        cali_id_t prnt_id;
        Variant   data;
    } test_nodes[] = { { 100, ctx.id(), CALI_INV_ID, Variant("outer") }, { 101, ctx.id(), 100, Variant("inner") } };

    for (const NodeInfo& nI : test_nodes)
        db.merge_node(nI.node_id, nI.attr_id, nI.prnt_id, nI.data, idmap);

    //
    // --- Make spec with default key and statistics kernel for "val"
    //

    QuerySpec spec;

    spec.groupby.selection = QuerySpec::SelectionList<std::string>::Default;

    spec.aggregate.selection = QuerySpec::SelectionList<QuerySpec::AggregationOp>::List;
    spec.aggregate.list.push_back(::make_op("min", "val"));
    spec.aggregate.list.push_back(::make_op("max", "val"));
    spec.aggregate.list.push_back(::make_op("avg", "val"));
    spec.aggregate.list.push_back(::make_op("variance", "val"));

    // perform recursive aggregation from two aggregators

    Aggregator a(spec), b(spec);

    Variant   v_ints[4] = { Variant(-4), Variant(9), Variant(25), Variant(36) };
    cali_id_t node_id   = 101;
    cali_id_t val_id    = val_attr.id();

    a.add(db, db.merge_snapshot(1, &node_id, 1, &val_id, &v_ints[0], idmap));
    a.add(db, db.merge_snapshot(1, &node_id, 1, &val_id, &v_ints[1], idmap));

    b.add(db, db.merge_snapshot(1, &node_id, 1, &val_id, &v_ints[2], idmap));
    b.add(db, db.merge_snapshot(1, &node_id, 1, &val_id, &v_ints[3], idmap));

    // merge b into a
    b.flush(db, a);

    Attribute attr_min = db.get_attribute("min#val");
    Attribute attr_max = db.get_attribute("max#val");
    Attribute attr_avg = db.get_attribute("avg#val");
    Attribute attr_var = db.get_attribute("variance#val");

    ASSERT_TRUE(attr_min);
    ASSERT_TRUE(attr_max);
    ASSERT_TRUE(attr_avg);
    ASSERT_TRUE(attr_var);

    std::vector<EntryList> resdb;

    a.flush(db, [&resdb](CaliperMetadataAccessInterface&, const EntryList& list) { resdb.push_back(list); });

    // check results

    EXPECT_EQ(resdb.size(), 1); // one entry

    auto dict = make_dict_from_entrylist(resdb.front());

    EXPECT_EQ(dict[attr_min.id()].value().to_int(), -4);
    EXPECT_EQ(dict[attr_max.id()].value().to_int(), 36);
    EXPECT_EQ(dict[attr_avg.id()].value().to_int(), 16);
    EXPECT_DOUBLE_EQ(dict[attr_var.id()].value().to_double(), 2018.0 / 4.0 - (16.5 * 16.5));
}

TEST(AggregatorTest, ScaledRatioKernel)
{
    CaliperMetadataDB db;
    IdMap             idmap;

    Attribute x_attr = db.create_attribute("x", CALI_TYPE_INT, CALI_ATTR_ASVALUE);
    Attribute y_attr = db.create_attribute("y", CALI_TYPE_INT, CALI_ATTR_ASVALUE);

    QuerySpec spec;

    spec.groupby.selection = QuerySpec::SelectionList<std::string>::None;

    spec.aggregate.selection = QuerySpec::SelectionList<QuerySpec::AggregationOp>::List;
    spec.aggregate.list.push_back(::make_op("ratio", "x", "y", "10"));

    Aggregator a(spec);

    Variant   v_ints[4] = { Variant(10), Variant(20), Variant(74), Variant(22) };
    cali_id_t attrs[2]  = { x_attr.id(), y_attr.id() };

    a.add(db, db.merge_snapshot(0, nullptr, 2, attrs, v_ints + 0, idmap));
    a.add(db, db.merge_snapshot(0, nullptr, 2, attrs, v_ints + 2, idmap));

    std::vector<EntryList> resdb;

    a.flush(db, [&resdb](CaliperMetadataAccessInterface&, const EntryList& list) { resdb.push_back(list); });

    ASSERT_EQ(resdb.size(), 1);

    Attribute attr_ratio = db.get_attribute("ratio#x/y");

    ASSERT_TRUE(attr_ratio);

    auto dict = make_dict_from_entrylist(resdb.front());

    EXPECT_DOUBLE_EQ(dict[attr_ratio.id()].value().to_double(), 20.0);
}

TEST(AggregatorTest, ScaledSumKernel)
{
    CaliperMetadataDB db;
    IdMap             idmap;

    Attribute x_attr = db.create_attribute("x", CALI_TYPE_INT, CALI_ATTR_ASVALUE);

    QuerySpec spec;

    spec.groupby.selection = QuerySpec::SelectionList<std::string>::None;

    spec.aggregate.selection = QuerySpec::SelectionList<QuerySpec::AggregationOp>::List;
    spec.aggregate.list.push_back(::make_op("scale", "x", "0.5"));

    Aggregator a(spec);

    Variant   v_ints[2] = { Variant(10), Variant(20) };
    cali_id_t attr_id   = x_attr.id();

    a.add(db, db.merge_snapshot(0, nullptr, 1, &attr_id, v_ints + 0, idmap));
    a.add(db, db.merge_snapshot(0, nullptr, 1, &attr_id, v_ints + 1, idmap));

    std::vector<EntryList> resdb;

    a.flush(db, [&resdb](CaliperMetadataAccessInterface&, const EntryList& list) { resdb.push_back(list); });

    ASSERT_EQ(resdb.size(), 1);

    Attribute attr_scale = db.get_attribute("scale#x");

    ASSERT_TRUE(attr_scale);

    auto dict = make_dict_from_entrylist(resdb.front());

    EXPECT_DOUBLE_EQ(dict[attr_scale.id()].value().to_double(), 15.0);
}

TEST(AggregatorTest, ScaledCountKernel)
{
    CaliperMetadataDB db;
    IdMap             idmap;

    Attribute x_attr = db.create_attribute("x", CALI_TYPE_INT, CALI_ATTR_ASVALUE);

    QuerySpec spec;

    spec.groupby.selection = QuerySpec::SelectionList<std::string>::None;

    spec.aggregate.selection = QuerySpec::SelectionList<QuerySpec::AggregationOp>::List;
    spec.aggregate.list.push_back(::make_op("scale_count", "2.5"));

    Aggregator a(spec);

    Variant   v_ints[2] = { Variant(10), Variant(20) };
    cali_id_t attr_id   = x_attr.id();

    a.add(db, db.merge_snapshot(0, nullptr, 1, &attr_id, v_ints + 0, idmap));
    a.add(db, db.merge_snapshot(0, nullptr, 1, &attr_id, v_ints + 1, idmap));

    std::vector<EntryList> resdb;

    a.flush(db, [&resdb](CaliperMetadataAccessInterface&, const EntryList& list) { resdb.push_back(list); });

    ASSERT_EQ(resdb.size(), 1);

    Attribute attr_scale = db.get_attribute("scount");

    ASSERT_TRUE(attr_scale);

    auto dict = make_dict_from_entrylist(resdb.front());

    EXPECT_DOUBLE_EQ(dict[attr_scale.id()].value().to_double(), 5.0);
}

TEST(AggregatorTest, AnyKernel)
{
    CaliperMetadataDB db;
    IdMap             idmap;

    Attribute x_attr = db.create_attribute("x", CALI_TYPE_INT, CALI_ATTR_ASVALUE);

    QuerySpec spec;

    spec.groupby.selection = QuerySpec::SelectionList<std::string>::None;

    spec.aggregate.selection = QuerySpec::SelectionList<QuerySpec::AggregationOp>::List;
    spec.aggregate.list.push_back(::make_op("any", "x"));

    Aggregator a(spec);

    Variant   v_ints[2] = { Variant(42), Variant(42) };
    cali_id_t attr_id   = x_attr.id();

    a.add(db, db.merge_snapshot(0, nullptr, 1, &attr_id, v_ints + 0, idmap));
    a.add(db, db.merge_snapshot(0, nullptr, 1, &attr_id, v_ints + 1, idmap));

    std::vector<EntryList> resdb;

    a.flush(db, [&resdb](CaliperMetadataAccessInterface&, const EntryList& list) { resdb.push_back(list); });

    ASSERT_EQ(resdb.size(), 1);

    Attribute attr_scale = db.get_attribute("any#x");

    ASSERT_TRUE(attr_scale);

    auto dict = make_dict_from_entrylist(resdb.front());

    EXPECT_DOUBLE_EQ(dict[attr_scale.id()].value().to_double(), 42.0);
}

TEST(AggregatorTest, PercentTotalKernel)
{
    CaliperMetadataDB db;
    IdMap             idmap;

    // create some context attributes

    Attribute ctx      = db.create_attribute("ctx", CALI_TYPE_INT, CALI_ATTR_NESTED);
    Attribute val_attr = db.create_attribute("val", CALI_TYPE_INT, CALI_ATTR_ASVALUE);

    // make some nodes

    const struct NodeInfo {
        cali_id_t node_id;
        cali_id_t attr_id;
        cali_id_t prnt_id;
        Variant   data;
    } test_nodes[] = { { 100, ctx.id(), CALI_INV_ID, Variant(-1) },
                       { 101, ctx.id(), 100, Variant(42) },
                       { 102, ctx.id(), 101, Variant(24) } };

    for (const NodeInfo& nI : test_nodes)
        db.merge_node(nI.node_id, nI.attr_id, nI.prnt_id, nI.data, idmap);

    //
    // --- Make spec with default key and statistics kernel for "val"
    //

    QuerySpec spec;

    spec.groupby.selection = QuerySpec::SelectionList<std::string>::Default;

    spec.aggregate.selection = QuerySpec::SelectionList<QuerySpec::AggregationOp>::List;
    spec.aggregate.list.push_back(::make_op("percent_total", "val"));
    spec.aggregate.list.push_back(::make_op("inclusive_percent_total", "val"));

    // perform recursive aggregation from two aggregators

    Aggregator a(spec), b(spec);

    Variant   v_ints[4] = { Variant(4), Variant(24), Variant(16), Variant(36) };
    cali_id_t nodea_id  = 101;
    cali_id_t nodeb_id  = 102;
    cali_id_t val_id    = val_attr.id();

    a.add(db, db.merge_snapshot(1, &nodea_id, 1, &val_id, &v_ints[0], idmap));
    a.add(db, db.merge_snapshot(1, &nodeb_id, 1, &val_id, &v_ints[1], idmap));

    b.add(db, db.merge_snapshot(1, &nodea_id, 1, &val_id, &v_ints[2], idmap));
    b.add(db, db.merge_snapshot(1, &nodeb_id, 1, &val_id, &v_ints[3], idmap));

    // merge b into a
    b.flush(db, a);

    Attribute attr_pct  = db.get_attribute("percent_total#val");
    Attribute attr_ipct = db.get_attribute("ipercent_total#val");

    ASSERT_TRUE(attr_pct);
    ASSERT_TRUE(attr_ipct);

    std::vector<EntryList> resdb;

    a.flush(db, [&resdb](CaliperMetadataAccessInterface&, const EntryList& list) { resdb.push_back(list); });

    // check results

    EXPECT_EQ(resdb.size(), 3); // three entries

    auto it = std::find_if(resdb.begin(), resdb.end(), [ctx](const EntryList& list) {
        for (const Entry& e : list)
            if (e.value(ctx).to_int() == 42)
                return true;
        return false;
    });

    ASSERT_NE(it, resdb.end());

    auto dict = make_dict_from_entrylist(*it);

    EXPECT_DOUBLE_EQ(dict[attr_pct.id()].value().to_double(), 25.0);
    EXPECT_DOUBLE_EQ(dict[attr_ipct.id()].value().to_double(), 100.0);

    it = std::find_if(resdb.begin(), resdb.end(), [ctx](const EntryList& list) {
        for (const Entry& e : list)
            if (e.value(ctx).to_int() == 24)
                return true;
        return false;
    });

    ASSERT_NE(it, resdb.end());

    dict = make_dict_from_entrylist(*it);

    EXPECT_DOUBLE_EQ(dict[attr_pct.id()].value().to_double(), 75.0);
    EXPECT_DOUBLE_EQ(dict[attr_ipct.id()].value().to_double(), 75.0);

    it = std::find_if(resdb.begin(), resdb.end(), [ctx](const EntryList& list) {
        for (const Entry& e : list)
            if (e.value(ctx).to_int() == -1)
                return true;
        return false;
    });

    ASSERT_NE(it, resdb.end());

    dict = make_dict_from_entrylist(*it);

    EXPECT_DOUBLE_EQ(dict[attr_pct.id()].value().to_double(), 0.0);
    EXPECT_DOUBLE_EQ(dict[attr_ipct.id()].value().to_double(), 100.0);
}
