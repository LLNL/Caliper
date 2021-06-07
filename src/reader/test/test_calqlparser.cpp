#include "caliper/reader/CalQLParser.h"

#include <gtest/gtest.h>

#include <sstream>

using namespace cali;

TEST(CalQLParserTest, SelectClause) {
    std::istringstream is("select a,a.a, b , c ");
    CalQLParser p1(is);

    EXPECT_FALSE(p1.error()) << "Unexpected parse error: " << p1.error_msg();

    QuerySpec q1 = p1.spec();

    EXPECT_EQ(q1.attribute_selection.selection, QuerySpec::AttributeSelection::List);
    EXPECT_EQ(q1.aggregation_ops.selection,     QuerySpec::AggregationSelection::None);

    ASSERT_EQ(q1.attribute_selection.list.size(), 4);

    EXPECT_EQ(q1.attribute_selection.list[0], "a");
    EXPECT_EQ(q1.attribute_selection.list[1], "a.a");
    EXPECT_EQ(q1.attribute_selection.list[2], "b");
    EXPECT_EQ(q1.attribute_selection.list[3], "c");

    CalQLParser p2("  SELECT aa ");

    EXPECT_FALSE(p2.error()) << "Unexpected parse error: " << p1.error_msg();

    QuerySpec q2 = p2.spec();

    EXPECT_EQ(q2.attribute_selection.selection, QuerySpec::AttributeSelection::List);
    EXPECT_EQ(q2.aggregation_ops.selection,     QuerySpec::AggregationSelection::None);

    ASSERT_EQ(q2.attribute_selection.list.size(), 1);
    EXPECT_EQ(q2.attribute_selection.list[0], "aa");

    CalQLParser p3("select bla,");
    EXPECT_TRUE(p3.error());

    CalQLParser p4("select ");
    EXPECT_TRUE(p4.error());

    CalQLParser p5("select *");

    EXPECT_FALSE(p5.error()) << "Unexpected parse error: " << p5.error_msg();

    QuerySpec q5 = p5.spec();

    EXPECT_EQ(q5.attribute_selection.selection, QuerySpec::AttributeSelection::All);
    EXPECT_EQ(q5.aggregation_ops.selection, QuerySpec::AggregationSelection::None);
}

TEST(CalQLParserTest, SelectClauseWithAggregation) {
    CalQLParser p1("select aa,count(),sum(bb)");

    EXPECT_FALSE(p1.error()) << "Unexpected parse error: " << p1.error_msg();

    QuerySpec q1 = p1.spec();

    EXPECT_EQ(q1.attribute_selection.selection, QuerySpec::AttributeSelection::List);
    ASSERT_EQ(q1.attribute_selection.list.size(), 3);
    EXPECT_EQ(q1.attribute_selection.list[0], "aa");
    EXPECT_EQ(q1.attribute_selection.list[1], "count");
    EXPECT_EQ(q1.attribute_selection.list[2], "sum#bb");

    EXPECT_EQ(q1.aggregation_ops.selection,     QuerySpec::AggregationSelection::List);
    ASSERT_EQ(q1.aggregation_ops.list.size(), 2);

    EXPECT_STREQ(q1.aggregation_ops.list[0].op.name, "count");
    EXPECT_STREQ(q1.aggregation_ops.list[1].op.name, "sum");
    ASSERT_EQ(q1.aggregation_ops.list[1].args.size(), 1);
    EXPECT_EQ(q1.aggregation_ops.list[1].args[0], "bb");

    CalQLParser p2("SELECT COUNT(),b,ccc ");

    EXPECT_FALSE(p2.error()) << "Unexpected parse error: " << p2.error_msg();

    QuerySpec q2 = p2.spec();

    EXPECT_EQ(q2.attribute_selection.selection, QuerySpec::AttributeSelection::List);
    ASSERT_EQ(q2.attribute_selection.list.size(), 3);
    EXPECT_EQ(q2.attribute_selection.list[0], "count");
    EXPECT_EQ(q2.attribute_selection.list[1], "b");
    EXPECT_EQ(q2.attribute_selection.list[2], "ccc");

    EXPECT_EQ(q2.aggregation_ops.selection,     QuerySpec::AggregationSelection::List);
    ASSERT_EQ(q2.aggregation_ops.list.size(), 1);

    EXPECT_STREQ(q2.aggregation_ops.list[0].op.name, "count");

    CalQLParser p3("select sum()");
    EXPECT_TRUE(p3.error());

    CalQLParser p4("select count(a)");
    EXPECT_TRUE(p4.error());

    CalQLParser p5("select sum(a,b,c)");
    EXPECT_TRUE(p5.error());

    CalQLParser p6("SELECT count(),* ");

    EXPECT_FALSE(p6.error()) << "Unexpected parse error: " << p6.error_msg();

    QuerySpec q6 = p6.spec();

    EXPECT_EQ(q6.attribute_selection.selection, QuerySpec::AttributeSelection::All);
    EXPECT_EQ(q6.aggregation_ops.selection,     QuerySpec::AggregationSelection::List);
    ASSERT_EQ(q6.aggregation_ops.list.size(), 1);
    EXPECT_STREQ(q6.aggregation_ops.list[0].op.name, "count");
}

TEST(CalQLParserTest, WhereClause) {
    CalQLParser p1("where a,bbb<17, NOT cc , dd = 5, not eee = foo, ff>42");

    EXPECT_FALSE(p1.error()) << "Unexpected parse error: " << p1.error_msg();

    QuerySpec q1 = p1.spec();

    EXPECT_EQ(q1.filter.selection, QuerySpec::FilterSelection::List);
    ASSERT_EQ(q1.filter.list.size(), 6);

    EXPECT_EQ(q1.filter.list[0].op, QuerySpec::Condition::Exist);
    EXPECT_EQ(q1.filter.list[0].attr_name, "a");
    EXPECT_EQ(q1.filter.list[1].op, QuerySpec::Condition::LessThan);
    EXPECT_EQ(q1.filter.list[1].attr_name, "bbb");
    EXPECT_EQ(q1.filter.list[1].value, "17");
    EXPECT_EQ(q1.filter.list[2].op, QuerySpec::Condition::NotExist);
    EXPECT_EQ(q1.filter.list[2].attr_name, "cc");
    EXPECT_EQ(q1.filter.list[3].op, QuerySpec::Condition::Equal);
    EXPECT_EQ(q1.filter.list[3].attr_name, "dd");
    EXPECT_EQ(q1.filter.list[3].value, "5");
    EXPECT_EQ(q1.filter.list[4].op, QuerySpec::Condition::NotEqual);
    EXPECT_EQ(q1.filter.list[4].attr_name, "eee");
    EXPECT_EQ(q1.filter.list[4].value, "foo");
    EXPECT_EQ(q1.filter.list[5].op, QuerySpec::Condition::GreaterThan);
    EXPECT_EQ(q1.filter.list[5].attr_name, "ff");
    EXPECT_EQ(q1.filter.list[5].value, "42");

    CalQLParser p2("where a=");
    EXPECT_TRUE(p2.error());

    CalQLParser p3("where aa=bb,NOT");
    EXPECT_TRUE(p3.error());
}

TEST(CalQLParserTest, GroupByClause) {
    CalQLParser p1("GROUP   By   aa,b, ccc ");

    EXPECT_FALSE(p1.error()) << "Unexpected parse error: " << p1.error_msg();

    QuerySpec q1 = p1.spec();

    EXPECT_EQ(q1.aggregation_key.selection, QuerySpec::AttributeSelection::List);
    ASSERT_EQ(q1.aggregation_key.list.size(), 3);

    EXPECT_EQ(q1.aggregation_key.list[0], "aa");
    EXPECT_EQ(q1.aggregation_key.list[1], "b");
    EXPECT_EQ(q1.aggregation_key.list[2], "ccc");
}

TEST(CalQLParserTest, OrderByClause1) {
    CalQLParser p1("Order By aa, b desc , c   asc, ddd ");

    EXPECT_FALSE(p1.error()) << "Unexpected parse error: " << p1.error_msg();

    QuerySpec q1 = p1.spec();

    EXPECT_EQ(q1.sort.selection, QuerySpec::SortSelection::List);
    ASSERT_EQ(q1.sort.list.size(), 4);

    EXPECT_EQ(q1.sort.list[0].attribute, "aa");
    EXPECT_EQ(q1.sort.list[0].order,     QuerySpec::SortSpec::Ascending);
    EXPECT_EQ(q1.sort.list[1].attribute, "b");
    EXPECT_EQ(q1.sort.list[1].order,     QuerySpec::SortSpec::Descending);
    EXPECT_EQ(q1.sort.list[2].attribute, "c");
    EXPECT_EQ(q1.sort.list[2].order,     QuerySpec::SortSpec::Ascending);
    EXPECT_EQ(q1.sort.list[3].attribute, "ddd");
    EXPECT_EQ(q1.sort.list[3].order,     QuerySpec::SortSpec::Ascending);
}

TEST(CalQLParserTest, OrderByClause2) {
    CalQLParser p("Order By aa,\"b with space\" format table ");

    EXPECT_FALSE(p.error()) << "Unexpected parse error: " << p.error_msg();

    QuerySpec q = p.spec();

    EXPECT_EQ(q.sort.selection, QuerySpec::SortSelection::List);
    ASSERT_EQ(q.sort.list.size(), 2);

    EXPECT_EQ(q.sort.list[0].attribute, "aa");
    EXPECT_EQ(q.sort.list[0].order,     QuerySpec::SortSpec::Ascending);
    EXPECT_EQ(q.sort.list[1].attribute, "b with space");
    EXPECT_EQ(q.sort.list[1].order,     QuerySpec::SortSpec::Ascending);

    EXPECT_EQ(q.format.opt, QuerySpec::FormatSpec::User);
    EXPECT_STREQ(q.format.formatter.name, "table");
}

TEST(CalQLParserTest, OrderByClause3) {
    CalQLParser p("Order By aa DESC format table ");

    EXPECT_FALSE(p.error()) << "Unexpected parse error: " << p.error_msg();

    QuerySpec q = p.spec();

    EXPECT_EQ(q.sort.selection, QuerySpec::SortSelection::List);
    ASSERT_EQ(q.sort.list.size(), 1);

    EXPECT_EQ(q.sort.list[0].attribute, "aa");
    EXPECT_EQ(q.sort.list[0].order,     QuerySpec::SortSpec::Descending);

    EXPECT_EQ(q.format.opt, QuerySpec::FormatSpec::User);
    EXPECT_STREQ(q.format.formatter.name, "table");
}

TEST(CalQLParserTest, FormatSpec) {
    CalQLParser p1("FORMAT tree(\"a,bb,ccc\")");

    EXPECT_FALSE(p1.error()) << "Unexpected parse error: " << p1.error_msg();

    QuerySpec q1 = p1.spec();

    EXPECT_EQ(q1.format.opt, QuerySpec::FormatSpec::User);
    EXPECT_STREQ(q1.format.formatter.name, "tree");
    ASSERT_EQ(q1.format.args.size(), 1);
    EXPECT_EQ(q1.format.args[0], "a,bb,ccc");

    CalQLParser p2("FORMAT table");

    EXPECT_FALSE(p2.error()) << "Unexpected parse error: " << p2.error_msg();

    QuerySpec q2 = p2.spec();

    EXPECT_EQ(q2.format.opt, QuerySpec::FormatSpec::User);
    EXPECT_STREQ(q2.format.formatter.name, "table");
    EXPECT_EQ(q2.format.args.size(), 0);

    CalQLParser p3("FORMAT tree(\"a,bb,ccc\", ddd, e)");

    EXPECT_TRUE(p3.error());
}

TEST(CalQLParserTest, AggregateClause) {
    CalQLParser p1(" Aggregate SUM ( aaaa ), Count(  ) ");

    EXPECT_FALSE(p1.error()) << "Unexpected parse error: " << p1.error_msg();

    QuerySpec q1 = p1.spec();

    EXPECT_EQ(q1.aggregation_ops.selection, QuerySpec::AggregationSelection::List);
    ASSERT_EQ(q1.aggregation_ops.list.size(), 2);

    EXPECT_STREQ(q1.aggregation_ops.list[0].op.name, "sum");
    ASSERT_EQ(q1.aggregation_ops.list[0].args.size(), 1);
    EXPECT_EQ(q1.aggregation_ops.list[0].args[0], "aaaa");
    EXPECT_STREQ(q1.aggregation_ops.list[1].op.name, "count");

    // currently listing the same function twice isn't an error
    CalQLParser p2(" Aggregate Percent_Total ( a.b:c ), Count(  ), count() ");

    EXPECT_FALSE(p2.error()) << "Unexpected parse error: " << p2.error_msg();

    QuerySpec q2 = p2.spec();

    EXPECT_EQ(q2.aggregation_ops.selection, QuerySpec::AggregationSelection::List);
    ASSERT_EQ(q2.aggregation_ops.list.size(), 3);

    EXPECT_STREQ(q2.aggregation_ops.list[0].op.name, "percent_total");
    ASSERT_EQ(q2.aggregation_ops.list[0].args.size(), 1);
    EXPECT_EQ(q2.aggregation_ops.list[0].args[0], "a.b:c");
    EXPECT_STREQ(q2.aggregation_ops.list[1].op.name, "count");
    EXPECT_STREQ(q2.aggregation_ops.list[2].op.name, "count");

    CalQLParser p3("aggregate sum(aa), count(");
    EXPECT_TRUE(p3.error());

    CalQLParser p4("aggregate sum(aa),");
    EXPECT_TRUE(p4.error());
}

TEST(CalQLParserTest, AliasAttribute) {
    CalQLParser p1("select a  as \"my alias (for a)\", b");

    EXPECT_FALSE(p1.error()) << "Unexpected parse error: " << p1.error_msg();

    QuerySpec q1 = p1.spec();

    EXPECT_EQ(q1.attribute_selection.selection, QuerySpec::AttributeSelection::List);
    ASSERT_EQ(q1.attribute_selection.list.size(), 2);

    EXPECT_STREQ(q1.attribute_selection.list[0].c_str(), "a");
    EXPECT_STREQ(q1.attribute_selection.list[1].c_str(), "b");

    ASSERT_EQ(q1.aliases.size(), 1);
    EXPECT_STREQ(q1.aliases["a"].c_str(), "my alias (for a)");
}

TEST(CalQLParserTest, AliasAggregate) {
    CalQLParser p1("select x,percent_total(a) as \"my alias (for percent_total#a)\" format table");

    EXPECT_FALSE(p1.error()) << "Unexpected parse error: " << p1.error_msg();

    QuerySpec q1 = p1.spec();

    EXPECT_EQ(q1.attribute_selection.selection, QuerySpec::AttributeSelection::List);
    ASSERT_EQ(q1.attribute_selection.list.size(), 2);

    EXPECT_EQ(q1.attribute_selection.list[0], "x");
    EXPECT_EQ(q1.attribute_selection.list[1], "percent_total#a");

    EXPECT_EQ(q1.aggregation_ops.selection, QuerySpec::AggregationSelection::List);
    ASSERT_EQ(q1.aggregation_ops.list.size(), 1);
    EXPECT_STREQ(q1.aggregation_ops.list[0].op.name, "percent_total");
    EXPECT_EQ(q1.aggregation_ops.list[0].args[0], "a");

    EXPECT_STREQ(q1.format.formatter.name, "table");

    ASSERT_EQ(q1.aliases.size(), 1);
    EXPECT_STREQ(q1.aliases["percent_total#a"].c_str(), "my alias (for percent_total#a)");
}

TEST(CalQLParserTest, AttributeUnit) {
    CalQLParser p1("select x,scale(a,1e-6) unit \"sec\" format table");

    EXPECT_FALSE(p1.error()) << "Unexpected parse error: " << p1.error_msg();

    QuerySpec q1 = p1.spec();

    EXPECT_EQ(q1.attribute_selection.selection, QuerySpec::AttributeSelection::List);
    ASSERT_EQ(q1.attribute_selection.list.size(), 2);

    EXPECT_EQ(q1.attribute_selection.list[0], "x");
    EXPECT_EQ(q1.attribute_selection.list[1], "scale#a");

    EXPECT_EQ(q1.aggregation_ops.selection, QuerySpec::AggregationSelection::List);
    ASSERT_EQ(q1.aggregation_ops.list.size(), 1);
    EXPECT_STREQ(q1.aggregation_ops.list[0].op.name, "scale");
    EXPECT_EQ(q1.aggregation_ops.list[0].args[0], "a");
    EXPECT_EQ(q1.aggregation_ops.list[0].args[1], "1e-6");

    EXPECT_STREQ(q1.format.formatter.name, "table");

    ASSERT_EQ(q1.units.size(), 1);
    EXPECT_STREQ(q1.units["scale#a"].c_str(), "sec");
}

TEST(CalQLParserTest, AttributeAliasAndUnit) {
    CalQLParser p1("select x,ratio(a,b,1e-6) As \"Read BW\" uNiT \"MB/s\" format table");

    EXPECT_FALSE(p1.error()) << "Unexpected parse error: " << p1.error_msg();

    QuerySpec q1 = p1.spec();

    EXPECT_EQ(q1.attribute_selection.selection, QuerySpec::AttributeSelection::List);
    ASSERT_EQ(q1.attribute_selection.list.size(), 2);

    EXPECT_EQ(q1.attribute_selection.list[0], "x");
    EXPECT_EQ(q1.attribute_selection.list[1], "ratio#a/b");

    EXPECT_EQ(q1.aggregation_ops.selection, QuerySpec::AggregationSelection::List);
    ASSERT_EQ(q1.aggregation_ops.list.size(), 1);
    EXPECT_STREQ(q1.aggregation_ops.list[0].op.name, "ratio");
    EXPECT_EQ(q1.aggregation_ops.list[0].args[0], "a");
    EXPECT_EQ(q1.aggregation_ops.list[0].args[1], "b");
    EXPECT_EQ(q1.aggregation_ops.list[0].args[2], "1e-6");

    EXPECT_STREQ(q1.format.formatter.name, "table");

    ASSERT_EQ(q1.units.size(), 1);
    EXPECT_STREQ(q1.units["ratio#a/b"].c_str(), "MB/s");
    ASSERT_EQ(q1.aliases.size(), 1);
    EXPECT_STREQ(q1.aliases["ratio#a/b"].c_str(), "Read BW");
}

TEST(CalQLParserTest, AttributeDoubleAliasParseError) {
    CalQLParser p1("select x,ratio(a,b,1e-6) As \"Read BW\" AS again format table");

    EXPECT_TRUE(p1.error());
    EXPECT_STREQ(p1.error_msg().c_str(), "Expected clause keyword, got as");
}

TEST(CalQLParserTest, FullStatement) {
    const char* s1 =
        "SELECT a,bb, cc, count() where bb< 42, NOT d=\"foo,\"\\ bar, c GROUP BY a, bb,d\n"
        "FORMAT json  ";

    CalQLParser p1(s1);

    EXPECT_FALSE(p1.error()) << "Unexpected parse error: " << p1.error_msg();

    QuerySpec q1 = p1.spec();

    EXPECT_EQ(q1.aggregation_ops.selection, QuerySpec::AggregationSelection::List);
    ASSERT_EQ(q1.aggregation_ops.list.size(), 1);
    EXPECT_STREQ(q1.aggregation_ops.list[0].op.name, "count");

    EXPECT_EQ(q1.attribute_selection.selection, QuerySpec::AttributeSelection::List);
    ASSERT_EQ(q1.attribute_selection.list.size(), 4);
    EXPECT_EQ(q1.attribute_selection.list[0], "a");
    EXPECT_EQ(q1.attribute_selection.list[1], "bb");
    EXPECT_EQ(q1.attribute_selection.list[2], "cc");
    EXPECT_EQ(q1.attribute_selection.list[3], "count");

    EXPECT_EQ(q1.filter.selection, QuerySpec::FilterSelection::List);
    ASSERT_EQ(q1.filter.list.size(), 3);
    EXPECT_EQ(q1.filter.list[0].op, QuerySpec::Condition::LessThan);
    EXPECT_EQ(q1.filter.list[0].attr_name, "bb");
    EXPECT_EQ(q1.filter.list[0].value, "42");
    EXPECT_EQ(q1.filter.list[1].op, QuerySpec::Condition::NotEqual);
    EXPECT_EQ(q1.filter.list[1].attr_name, "d");
    EXPECT_EQ(q1.filter.list[1].value, "foo, bar");
    EXPECT_EQ(q1.filter.list[2].op, QuerySpec::Condition::Exist);
    EXPECT_EQ(q1.filter.list[2].attr_name, "c");

    EXPECT_EQ(q1.aggregation_key.selection, QuerySpec::AttributeSelection::List);
    ASSERT_EQ(q1.aggregation_key.list.size(), 3);
    EXPECT_EQ(q1.aggregation_key.list[0], "a");
    EXPECT_EQ(q1.aggregation_key.list[1], "bb");
    EXPECT_EQ(q1.aggregation_key.list[2], "d");

    EXPECT_EQ(q1.format.opt, QuerySpec::FormatSpec::User);
    EXPECT_STREQ(q1.format.formatter.name, "json");

    const char* s2 =
        " SELECT count(), *, SUM(x\\\\y)  GROUP BY a.b.c where group ";

    CalQLParser p2(s2);

    EXPECT_FALSE(p2.error()) << "Unexpected parse error: " << p2.error_msg();

    QuerySpec q2 = p2.spec();

    EXPECT_EQ(q2.aggregation_ops.selection, QuerySpec::AggregationSelection::List);
    ASSERT_EQ(q2.aggregation_ops.list.size(), 2);
    EXPECT_STREQ(q2.aggregation_ops.list[0].op.name, "count");
    EXPECT_STREQ(q2.aggregation_ops.list[1].op.name, "sum");
    ASSERT_EQ(q2.aggregation_ops.list[1].args.size(), 1);
    EXPECT_EQ(q2.aggregation_ops.list[1].args[0], "x\\y");

    EXPECT_EQ(q2.attribute_selection.selection, QuerySpec::AttributeSelection::All);

    EXPECT_EQ(q2.aggregation_key.selection, QuerySpec::AttributeSelection::List);
    EXPECT_EQ(q2.aggregation_key.list[0], "a.b.c");

    EXPECT_EQ(q2.filter.selection, QuerySpec::FilterSelection::List);
    ASSERT_EQ(q2.filter.list[0].op, QuerySpec::Condition::Exist);
    EXPECT_EQ(q2.filter.list[0].attr_name, "group");

    EXPECT_EQ(q2.format.opt, QuerySpec::FormatSpec::Default);

    CalQLParser p3("SELECT a GROUP b.c.d WHERE e>100");
    EXPECT_TRUE(p3.error());

    CalQLParser p4("SELECT *,count() FORMAT table");

    EXPECT_FALSE(p4.error()) << "Unexpected parse error: " << p4.error_msg();

    QuerySpec q4 = p4.spec();

    EXPECT_EQ(q4.aggregation_ops.selection, QuerySpec::AggregationSelection::List);
    EXPECT_EQ(q4.aggregation_key.selection, QuerySpec::AttributeSelection::Default);
    EXPECT_EQ(q4.filter.selection, QuerySpec::FilterSelection::None);
    EXPECT_EQ(q4.format.opt, QuerySpec::FormatSpec::User);
}

TEST(CalQLParserTest, GarbageAtEnd) {
    CalQLParser p1(" select a,b,c format tree = where b");
    EXPECT_TRUE(p1.error());

    CalQLParser p2("where bla()");
    EXPECT_TRUE(p2.error());
}


TEST(CalQLParserTest, LetClause) {
    CalQLParser p1("let x=  ratio( a,   \"b\" ) , y=scale(c,42) let z=truncate (  yy )");

    EXPECT_FALSE(p1.error()) << "Unexpected parse error: " << p1.error_msg();

    QuerySpec q1 = p1.spec();

    ASSERT_EQ(q1.preprocess_ops.size(), static_cast<size_t>(3));

    EXPECT_STREQ(q1.preprocess_ops[0].target.c_str(), "x");
    EXPECT_STREQ(q1.preprocess_ops[0].op.op.name, "ratio");
    ASSERT_EQ(q1.preprocess_ops[0].op.args.size(), 2);
    EXPECT_STREQ(q1.preprocess_ops[0].op.args[0].c_str(), "a");
    EXPECT_STREQ(q1.preprocess_ops[0].op.args[1].c_str(), "b");
    EXPECT_EQ(q1.preprocess_ops[0].cond.op, QuerySpec::Condition::None);

    EXPECT_STREQ(q1.preprocess_ops[1].target.c_str(), "y");
    EXPECT_STREQ(q1.preprocess_ops[1].op.op.name, "scale");
    ASSERT_EQ(q1.preprocess_ops[1].op.args.size(), 2);
    EXPECT_STREQ(q1.preprocess_ops[1].op.args[0].c_str(), "c");
    EXPECT_STREQ(q1.preprocess_ops[1].op.args[1].c_str(), "42");
    EXPECT_EQ(q1.preprocess_ops[1].cond.op, QuerySpec::Condition::None);

    EXPECT_STREQ(q1.preprocess_ops[2].target.c_str(), "z");
    EXPECT_STREQ(q1.preprocess_ops[2].op.op.name, "truncate");
    ASSERT_EQ(q1.preprocess_ops[2].op.args.size(), 1);
    EXPECT_STREQ(q1.preprocess_ops[2].op.args[0].c_str(), "yy");
    EXPECT_EQ(q1.preprocess_ops[2].cond.op, QuerySpec::Condition::None);
}

TEST(CalQLParserTest, LetIfClause) {
    CalQLParser p1("let x=  ratio( a,   \"b\" ) if not X, y=scale(c,42) if Y =  foo let z=truncate (  yy ) if not Z>1");

    EXPECT_FALSE(p1.error()) << "Unexpected parse error: " << p1.error_msg();

    QuerySpec q1 = p1.spec();

    EXPECT_STREQ(q1.preprocess_ops[0].target.c_str(), "x");
    EXPECT_STREQ(q1.preprocess_ops[0].op.op.name, "ratio");
    EXPECT_EQ(q1.preprocess_ops[0].cond.op, QuerySpec::Condition::NotExist);
    EXPECT_STREQ(q1.preprocess_ops[0].cond.attr_name.c_str(), "X");

    EXPECT_STREQ(q1.preprocess_ops[1].target.c_str(), "y");
    EXPECT_STREQ(q1.preprocess_ops[1].op.op.name, "scale");
    EXPECT_EQ(q1.preprocess_ops[1].cond.op, QuerySpec::Condition::Equal);
    EXPECT_STREQ(q1.preprocess_ops[1].cond.attr_name.c_str(), "Y");
    EXPECT_STREQ(q1.preprocess_ops[1].cond.value.c_str(), "foo");

    EXPECT_STREQ(q1.preprocess_ops[2].target.c_str(), "z");
    EXPECT_STREQ(q1.preprocess_ops[2].op.op.name, "truncate");
    ASSERT_EQ(q1.preprocess_ops[2].op.args.size(), 1);
    EXPECT_STREQ(q1.preprocess_ops[2].op.args[0].c_str(), "yy");
    EXPECT_EQ(q1.preprocess_ops[2].cond.op, QuerySpec::Condition::LessOrEqual);
    EXPECT_STREQ(q1.preprocess_ops[2].cond.attr_name.c_str(), "Z");
    EXPECT_STREQ(q1.preprocess_ops[2].cond.value.c_str(), "1");
}

TEST(CalQLParserTest, LetClauseErrors) {
    CalQLParser p1("let blagarbl");

    EXPECT_TRUE(p1.error());
    EXPECT_STREQ(p1.error_msg().c_str(), "Expected \"=\" after blagarbl");

    CalQLParser p2("let a=notanoperator(x,y)");

    EXPECT_TRUE(p2.error());
    EXPECT_STREQ(p2.error_msg().c_str(), "Unknown operator notanoperator");

    CalQLParser p3("let a=scale(x,10.0), a =scale(y,5)");

    EXPECT_TRUE(p3.error());
    EXPECT_STREQ(p3.error_msg().c_str(), "a defined twice");
}
