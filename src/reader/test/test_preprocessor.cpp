#include "caliper/reader/Preprocessor.h"

#include "caliper/reader/CaliperMetadataDB.h"

#include "caliper/common/Node.h"

#include <gtest/gtest.h>

using namespace cali;

namespace
{

QuerySpec::AggregationOp
make_op(const char* name, const char* arg1 = nullptr, const char* arg2 = nullptr, const char* arg3 = nullptr)
{
    QuerySpec::AggregationOp op;
    const QuerySpec::FunctionSignature* p = Preprocessor::preprocess_defs();

    for ( ; p && p->name && (0 != strcmp(p->name, name)); ++p )
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

QuerySpec::PreprocessSpec
make_spec(const std::string& target, const QuerySpec::AggregationOp& op)
{
    QuerySpec::PreprocessSpec spec;

    spec.target = target;
    spec.op = op;
    spec.cond.op = QuerySpec::Condition::None;

    return spec;
}

QuerySpec::PreprocessSpec
make_spec_with_cond(const std::string& target, const QuerySpec::AggregationOp& op, const QuerySpec::Condition::Op cond, const char* cond_attr, const char* cond_val = nullptr)
{
    QuerySpec::PreprocessSpec spec;

    spec.target = target;
    spec.op = op;
    spec.cond.op = cond;
    if (cond_attr)
        spec.cond.attr_name = cond_attr;
    if (cond_val)
        spec.cond.value = cond_val;

    return spec;
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


TEST(PreprocessorTest, Ratio) {
    //
    // --- setup
    //

    CaliperMetadataDB db;
    IdMap             idmap;

    Attribute ctx =
        db.create_attribute("ctx.1", CALI_TYPE_STRING, CALI_ATTR_DEFAULT);
    Attribute nom =
        db.create_attribute("nom", CALI_TYPE_INT, CALI_ATTR_ASVALUE);
    Attribute dnm =
        db.create_attribute("dnm", CALI_TYPE_INT, CALI_ATTR_ASVALUE);

    EntryList rec;

    rec.push_back(Entry(db.merge_node(100, ctx.id(), CALI_INV_ID, Variant("test.preprocessor.ratio"), idmap)));
    rec.push_back(Entry(nom, Variant(18)));
    rec.push_back(Entry(dnm, Variant(3)));

    QuerySpec spec;

    spec.preprocess_ops.push_back(::make_spec("d.ratio", ::make_op("ratio", "nom", "dnm")));
    spec.preprocess_ops.push_back(::make_spec("s.ratio", ::make_op("ratio", "nom", "dnm", "2.0")));

    //
    // --- run
    //

    Preprocessor pp(spec);
    EntryList out = pp.process(db, rec);

    Attribute d_attr = db.get_attribute("d.ratio");
    Attribute s_attr = db.get_attribute("s.ratio");

    EXPECT_NE(d_attr, Attribute::invalid);
    EXPECT_NE(d_attr, Attribute::invalid);
    EXPECT_EQ(d_attr.type(), CALI_TYPE_DOUBLE);
    EXPECT_TRUE(d_attr.store_as_value());

    auto res  = ::make_dict_from_entrylist(out);
    auto d_it = res.find(d_attr.id());
    auto s_it = res.find(s_attr.id());

    ASSERT_NE(d_it, res.end()) << "d.ratio attribute not found\n";
    ASSERT_NE(s_it, res.end()) << "s.ratio attribute not found\n";

    EXPECT_DOUBLE_EQ(d_it->second.value().to_double(),  6.0);
    EXPECT_DOUBLE_EQ(s_it->second.value().to_double(), 12.0);
}


TEST(PreprocessorTest, Scale) {
    //
    // --- setup
    //

    CaliperMetadataDB db;
    IdMap             idmap;

    Attribute ctx_a =
        db.create_attribute("ctx.1", CALI_TYPE_STRING, CALI_ATTR_DEFAULT);
    Attribute val_a =
        db.create_attribute("val", CALI_TYPE_INT, CALI_ATTR_ASVALUE);

    EntryList rec;

    rec.push_back(Entry(db.merge_node(100, ctx_a.id(), CALI_INV_ID, Variant("test.preprocessor.scale"), idmap)));
    rec.push_back(Entry(val_a, Variant(42)));

    QuerySpec spec;

    spec.preprocess_ops.push_back(::make_spec("valx2.0", ::make_op("scale", "val", "2.0")));
    spec.preprocess_ops.push_back(::make_spec("valx0.5", ::make_op("scale", "val", "0.5")));

    //
    // --- run
    //

    Preprocessor pp(spec);
    EntryList out = pp.process(db, rec);

    Attribute v_attr = db.get_attribute("val");
    Attribute d_attr = db.get_attribute("valx2.0");
    Attribute h_attr = db.get_attribute("valx0.5");

    EXPECT_NE(v_attr, Attribute::invalid);
    EXPECT_NE(d_attr, Attribute::invalid);
    EXPECT_NE(h_attr, Attribute::invalid);
    EXPECT_EQ(d_attr.type(), CALI_TYPE_DOUBLE);
    EXPECT_TRUE(d_attr.store_as_value());

    auto res  = ::make_dict_from_entrylist(out);
    auto d_it = res.find(d_attr.id());
    auto h_it = res.find(h_attr.id());

    ASSERT_NE(d_it, res.end()) << "valx2.0 attribute not found\n";
    ASSERT_NE(h_it, res.end()) << "valx0.5 attribute not found\n";

    EXPECT_DOUBLE_EQ(d_it->second.value().to_double(), 84.0);
    EXPECT_DOUBLE_EQ(h_it->second.value().to_double(), 21.0);
}


TEST(PreprocessorTest, First) {
    //
    // --- setup
    //

    CaliperMetadataDB db;
    IdMap             idmap;

    Attribute ctx_a =
        db.create_attribute("ctx.1", CALI_TYPE_STRING, CALI_ATTR_DEFAULT);
    Attribute val_a =
        db.create_attribute("val.a", CALI_TYPE_INT, CALI_ATTR_ASVALUE);
    Attribute val_b =
        db.create_attribute("val.b", CALI_TYPE_INT, CALI_ATTR_ASVALUE);

    EntryList rec;

    rec.push_back(Entry(db.merge_node(100, ctx_a.id(), CALI_INV_ID, Variant("test.preprocessor.first"), idmap)));
    rec.push_back(Entry(val_a, Variant(42)));
    rec.push_back(Entry(val_b, Variant(24)));

    QuerySpec spec;

    spec.preprocess_ops.push_back(::make_spec("val.a.out",  ::make_op("first", "dummy.0", "val.a", "dummy.1")));
    spec.preprocess_ops.push_back(::make_spec("val.b.out",  ::make_op("first", "val.b",   "val.a", "dummy.0")));

    //
    // --- run
    //

    Preprocessor pp(spec);
    EntryList out = pp.process(db, rec);

    Attribute vao_attr = db.get_attribute("val.a.out");
    Attribute vbo_attr = db.get_attribute("val.b.out");

    EXPECT_NE(vao_attr, Attribute::invalid);
    EXPECT_NE(vbo_attr, Attribute::invalid);
    EXPECT_EQ(vao_attr.type(), CALI_TYPE_INT);
    EXPECT_EQ(vbo_attr.type(), CALI_TYPE_INT);

    auto res  = ::make_dict_from_entrylist(out);
    auto a_it = res.find(vao_attr.id());
    auto b_it = res.find(vbo_attr.id());

    ASSERT_NE(a_it, res.end()) << "val.a.out attribute not found\n";
    ASSERT_NE(b_it, res.end()) << "val.b.out attribute not found\n";

    EXPECT_EQ(a_it->second.value().to_int(), 42);
    EXPECT_EQ(b_it->second.value().to_int(), 24);
}


TEST(PreprocessorTest, Truncate) {
    //
    // --- setup
    //

    CaliperMetadataDB db;
    IdMap             idmap;

    Attribute ctx_a =
        db.create_attribute("ctx.1", CALI_TYPE_STRING, CALI_ATTR_DEFAULT);
    Attribute val_a =
        db.create_attribute("val", CALI_TYPE_DOUBLE, CALI_ATTR_ASVALUE);

    EntryList rec;

    rec.push_back(Entry(db.merge_node(100, ctx_a.id(), CALI_INV_ID, Variant("test.preprocessor.scale"), idmap)));
    rec.push_back(Entry(val_a, Variant(15.5)));

    QuerySpec spec;

    spec.preprocess_ops.push_back(::make_spec("valt6", ::make_op("truncate", "val", "6.0")));
    spec.preprocess_ops.push_back(::make_spec("valtd", ::make_op("truncate", "val")));

    //
    // --- run
    //

    Preprocessor pp(spec);
    EntryList out = pp.process(db, rec);

    Attribute v_attr = db.get_attribute("val");
    Attribute t6_attr = db.get_attribute("valt6");
    Attribute td_attr = db.get_attribute("valtd");

    EXPECT_NE(v_attr, Attribute::invalid);
    EXPECT_NE(t6_attr, Attribute::invalid);
    EXPECT_NE(td_attr, Attribute::invalid);
    EXPECT_EQ(td_attr.type(), CALI_TYPE_DOUBLE);
    EXPECT_TRUE(td_attr.store_as_value());

    auto res  = ::make_dict_from_entrylist(out);
    auto t6_it = res.find(t6_attr.id());
    auto td_it = res.find(td_attr.id());

    ASSERT_NE(t6_it, res.end()) << "valt6 attribute not found\n";
    ASSERT_NE(td_it, res.end()) << "valtd attribute not found\n";

    EXPECT_DOUBLE_EQ(t6_it->second.value().to_double(), 12.0);
    EXPECT_DOUBLE_EQ(td_it->second.value().to_double(), 15.0);
}


TEST(PreprocessorTest, Chain) {
    //
    // --- setup
    //

    CaliperMetadataDB db;
    IdMap             idmap;

    Attribute ctx_a =
        db.create_attribute("ctx.1", CALI_TYPE_STRING, CALI_ATTR_DEFAULT);
    Attribute val_a =
        db.create_attribute("val", CALI_TYPE_DOUBLE, CALI_ATTR_ASVALUE);

    EntryList rec;

    rec.push_back(Entry(db.merge_node(100, ctx_a.id(), CALI_INV_ID, Variant("test.preprocessor.scale"), idmap)));
    rec.push_back(Entry(val_a, Variant(5.5)));

    QuerySpec spec;

    spec.preprocess_ops.push_back(::make_spec("valx2",   ::make_op("scale", "val", "2.0")));
    spec.preprocess_ops.push_back(::make_spec("valx2t5", ::make_op("truncate", "valx2", "10")));

    //
    // --- run
    //

    Preprocessor pp(spec);
    EntryList out = pp.process(db, rec);

    Attribute v_attr = db.get_attribute("val");
    Attribute d_attr = db.get_attribute("valx2");
    Attribute t_attr = db.get_attribute("valx2t5");

    EXPECT_NE(v_attr, Attribute::invalid);
    EXPECT_NE(d_attr, Attribute::invalid);
    EXPECT_NE(t_attr, Attribute::invalid);
    EXPECT_EQ(d_attr.type(), CALI_TYPE_DOUBLE);
    EXPECT_TRUE(d_attr.store_as_value());

    auto res  = ::make_dict_from_entrylist(out);
    auto d_it = res.find(d_attr.id());
    auto t_it = res.find(t_attr.id());

    ASSERT_NE(d_it, res.end()) << "valtx2 attribute not found\n";
    ASSERT_NE(t_it, res.end()) << "valx2t5 attribute not found\n";

    EXPECT_DOUBLE_EQ(d_it->second.value().to_double(), 11.0);
    EXPECT_DOUBLE_EQ(t_it->second.value().to_double(), 10.0);
}

TEST(PreprocessorTest, Conditions) {
    //
    // --- setup
    //

    CaliperMetadataDB db;
    IdMap             idmap;

    Attribute ctx_a =
        db.create_attribute("ctx.1", CALI_TYPE_STRING, CALI_ATTR_DEFAULT);
    Attribute val_a =
        db.create_attribute("val.a", CALI_TYPE_INT, CALI_ATTR_ASVALUE);
    Attribute val_b =
        db.create_attribute("val.b", CALI_TYPE_INT, CALI_ATTR_ASVALUE);

    EntryList rec;

    rec.push_back(Entry(db.merge_node(100, ctx_a.id(), CALI_INV_ID, Variant("test.preprocessor.first"), idmap)));
    rec.push_back(Entry(val_a, Variant(42)));
    rec.push_back(Entry(val_b, Variant(24)));

    QuerySpec spec;

    spec.preprocess_ops.push_back(::make_spec_with_cond("val.a.out", ::make_op("first", "val.a"), QuerySpec::Condition::Exist, "ctx.1"));
    spec.preprocess_ops.push_back(::make_spec_with_cond("val.b.out", ::make_op("first", "val.b"), QuerySpec::Condition::NotExist, "ctx.1"));

    //
    // --- run
    //

    Preprocessor pp(spec);
    EntryList out = pp.process(db, rec);

    Attribute vao_attr = db.get_attribute("val.a.out");
    Attribute vbo_attr = db.get_attribute("val.b.out");

    EXPECT_NE(vao_attr, Attribute::invalid);
    EXPECT_EQ(vbo_attr, Attribute::invalid);
    EXPECT_EQ(vao_attr.type(), CALI_TYPE_INT);

    auto res  = ::make_dict_from_entrylist(out);
    auto a_it = res.find(vao_attr.id());

    ASSERT_NE(a_it, res.end()) << "val.a.out attribute not found\n";

    EXPECT_EQ(a_it->second.value().to_int(), 42);
}
