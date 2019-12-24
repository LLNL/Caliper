// Test the StringConverter class

#include "caliper/common/StringConverter.h"

#include "gtest/gtest.h"

using namespace cali;

TEST(StringConverterTest, ConvertBool) {
    StringConverter strconv_in_1(std::string("tRue"));
    StringConverter strconv_in_2(std::string("faLse"));
    StringConverter strconv_in_3;
    StringConverter strconv_in_4("t");
    StringConverter strconv_in_5(std::string("bla"));
    StringConverter strconv_in_6(std::string("0"));
    StringConverter strconv_in_7(std::string("1"));

    bool ok = false;

    bool res_1 = strconv_in_1.to_bool(&ok);
    EXPECT_TRUE(ok);
    EXPECT_EQ(res_1, true);
    ok = false;

    bool res_2 = strconv_in_2.to_bool(&ok);
    EXPECT_TRUE(ok);
    EXPECT_EQ(res_2, false);
    ok = false;

    bool res_3 = strconv_in_3.to_bool(&ok);
    EXPECT_FALSE(ok);
    ok = false;

    bool res_4 = strconv_in_4.to_bool(&ok);
    EXPECT_TRUE(ok);
    EXPECT_EQ(res_4, true);
    ok = false;

    bool res_5 = strconv_in_5.to_bool(&ok);
    EXPECT_FALSE(ok);
    ok = false;

    bool res_6 = strconv_in_6.to_bool(&ok);
    EXPECT_TRUE(ok);
    EXPECT_EQ(res_6, false);
    ok = false;

    bool res_7 = strconv_in_7.to_bool(&ok);
    EXPECT_TRUE(ok);
    EXPECT_EQ(res_7, true);
}

TEST(StringConverterTest, ConvertInt) {
    bool ok = false;

    StringConverter strconv("42");
    int res = strconv.to_int(&ok);
    EXPECT_TRUE(ok);
    EXPECT_EQ(res, 42);

    ok  = false;
    strconv = StringConverter("0");
    res = strconv.to_int(&ok);
    EXPECT_TRUE(ok);
    EXPECT_EQ(res, 0);

    ok  = false;
    strconv = StringConverter("  -14  ");
    res = strconv.to_int(&ok);
    EXPECT_TRUE(ok);
    EXPECT_EQ(res, -14);

    ok  = false;
    strconv = StringConverter("bla");
    res = strconv.to_int(&ok);
    EXPECT_FALSE(ok);

    ok  = false;
    strconv = StringConverter();
    res = strconv.to_int(&ok);
    EXPECT_FALSE(ok);
}

TEST(StringConverterTest, ConvertUint) {
    bool ok = false;

    StringConverter strconv("42");
    unsigned long res = strconv.to_uint(&ok);
    EXPECT_TRUE(ok);
    EXPECT_EQ(res, 42);

    ok  = false;
    strconv = StringConverter("0");
    res = strconv.to_uint(&ok);
    EXPECT_TRUE(ok);
    EXPECT_EQ(res, 0);

    ok  = false;
    strconv = StringConverter("bla");
    res = strconv.to_uint(&ok);
    EXPECT_FALSE(ok);

    ok  = false;
    strconv = StringConverter();
    res = strconv.to_uint(&ok);
    EXPECT_FALSE(ok);
}

TEST(StringConverterTest, ConvertStringList) {
    bool ok = false;

    StringConverter sc(" aword, b.c:cdef,, d, e\\ f\",gword \"  ,  ");

    std::vector<std::string> res = sc.to_stringlist(",:", &ok);

    EXPECT_TRUE(ok);
    ASSERT_EQ(res.size(), static_cast<decltype(res.size())>(5));

    EXPECT_EQ(res[0], std::string("aword"));
    EXPECT_EQ(res[1], std::string("b.c"));
    EXPECT_EQ(res[2], std::string("cdef"));
    EXPECT_EQ(res[3], std::string("d"));
    EXPECT_EQ(res[4], std::string("e f,gword "));
}

TEST(StringConverterTest, JsonList) {
    bool ok = false;

    StringConverter sc("[ a, [ b, c ] , { \"this\": [ is, \"a , nested\" , list ]}, \"  a string with \\\"]\\\" \"  ] ");

    std::vector<StringConverter> res =
        sc.rec_list(&ok);

    EXPECT_TRUE(ok);
    ASSERT_EQ(res.size(), 4u);

    EXPECT_EQ(res[0].to_string(), std::string("a"));
    EXPECT_EQ(res[1].to_string(), std::string("[ b, c ]"));
    EXPECT_EQ(res[2].to_string(), std::string("{ \"this\": [ is, \"a , nested\" , list ]}"));
    EXPECT_EQ(res[3].to_string(), std::string("  a string with \"]\" "));

    std::map<std::string, StringConverter> kv =
        res[2].rec_dict(&ok);

    EXPECT_TRUE(ok);

    EXPECT_EQ(kv["this"].to_string(), std::string("[ is, \"a , nested\" , list ]"));

    std::vector<StringConverter> nested_res =
        kv["this"].rec_list(&ok);

    EXPECT_TRUE(ok);
    ASSERT_EQ(nested_res.size(), 3u);

    EXPECT_EQ(nested_res[0].to_string(), std::string("is"));
    EXPECT_EQ(nested_res[1].to_string(), std::string("a , nested"));
    EXPECT_EQ(nested_res[2].to_string(), std::string("list"));

    StringConverter sce("[   ]");
    std::vector<StringConverter> e_res =
        sce.rec_list(&ok);

    EXPECT_TRUE(ok);
    EXPECT_EQ(e_res.size(), 0u);
}

TEST(StringConverterTest, JsonDict) {
    bool ok = false;

    StringConverter sc("{ \"aa\": { b : [ c, \"d }\", e], ff: \"gg\"},x:y  } blagarbl ");

    std::map<std::string, StringConverter> dict =
        sc.rec_dict(&ok);

    EXPECT_TRUE(ok);
    EXPECT_EQ(dict.size(), 2u);

    EXPECT_EQ(dict["aa"].to_string(), std::string("{ b : [ c, \"d }\", e], ff: \"gg\"}"));
    EXPECT_EQ(dict["x"].to_string(), "y");

    auto ndict = dict["aa"].rec_dict(&ok);

    EXPECT_TRUE(ok);
    EXPECT_EQ(ndict.size(), 2u);
    EXPECT_EQ(ndict["ff"].to_string(), std::string("gg"));

    auto nlist = ndict["b"].rec_list(&ok);

    EXPECT_TRUE(ok);
    ASSERT_EQ(nlist.size(), 3u);

    EXPECT_EQ(nlist[0].to_string(), std::string("c"));
    EXPECT_EQ(nlist[1].to_string(), std::string("d }"));
    EXPECT_EQ(nlist[2].to_string(), std::string("e"));
}

TEST(StringConverterTest, JsonErrors)
{
    bool ok = true;

    StringConverter sc;

    sc = StringConverter("{ a : b, c: d");
    sc.rec_dict(&ok);
    EXPECT_FALSE(ok);
    ok = true;

    sc = StringConverter("{ \"aa\": bb, c d} ");
    sc.rec_dict(&ok);
    EXPECT_FALSE(ok);
    ok = true;

    sc = StringConverter("[ \"aa\", bb, c d ");
    sc.rec_list(&ok);
    EXPECT_FALSE(ok);
    ok = true;

    sc = StringConverter("[ \"aa\", { bb: c, d: e] ");
    sc.rec_list(&ok);
    EXPECT_FALSE(ok);
    ok = true;

    sc = StringConverter("{ \"a a\": [ b, c:d } ");
    sc.rec_dict(&ok);
    EXPECT_FALSE(ok);
    ok = true;
}