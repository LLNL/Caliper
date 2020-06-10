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
