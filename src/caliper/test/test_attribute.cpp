// Tests for the attribute APIs

#include "caliper/cali.h"
#include "caliper/Caliper.h"

#include "caliper/common/Attribute.h"
#include "caliper/common/Node.h"

#include "caliper/common/util/split.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <iterator>
#include <vector>

using namespace cali;

TEST(AttributeAPITest, ValidAttribute) {
    Caliper c;

    Attribute meta_attr =
        c.create_attribute("test.attribute.api.meta", CALI_TYPE_INT, CALI_ATTR_HIDDEN);

    ASSERT_NE(meta_attr, Attribute::invalid);

    EXPECT_TRUE(meta_attr.is_hidden());

    EXPECT_FALSE(meta_attr.is_nested());
    EXPECT_FALSE(meta_attr.store_as_value());

    cali_id_t      meta_id  = meta_attr.id();
    cali_variant_t meta_val = cali_make_variant_from_int(42);

    cali_id_t attr_id =
        cali_create_attribute_with_metadata("test.attribute.api", CALI_TYPE_STRING,
                                            CALI_ATTR_NESTED | CALI_ATTR_SCOPE_PROCESS | CALI_ATTR_NOMERGE,
                                            1, &meta_id, &meta_val);

    ASSERT_NE(attr_id, CALI_INV_ID);

    EXPECT_STREQ(cali_attribute_name(attr_id), "test.attribute.api");
    EXPECT_EQ(cali_attribute_type(attr_id), CALI_TYPE_STRING);
    EXPECT_EQ(cali_find_attribute("test.attribute.api"), attr_id);
    EXPECT_TRUE(c.attribute_exists("test.attribute.api"));

    Attribute attr = c.get_attribute(attr_id);

    EXPECT_EQ(attr.name(), "test.attribute.api");
    EXPECT_EQ(attr.get(meta_attr).to_int(), 42);
    EXPECT_FALSE(attr.is_autocombineable());
    EXPECT_TRUE(attr.is_nested());

    char buf[120];
    ASSERT_GE(cali_prop2string(cali_attribute_properties(attr_id), buf, 120), 1);

    std::vector<std::string> props;
    util::split(std::string(buf), ':',  std::back_inserter(props));

    std::vector<std::string> props_exp { "nested", "process_scope", "nomerge", "default" };

    for (auto s : props) {
        auto it = std::find(props_exp.begin(), props_exp.end(), s);
        ASSERT_NE(it, props_exp.end()) << s << " is not an expected property" << std::endl;
        props_exp.erase(it);
    }

    EXPECT_TRUE(props_exp.empty());
}

TEST(AttributeAPITest, InvalidAttribute) {
    EXPECT_EQ(cali_attribute_type(CALI_INV_ID), CALI_TYPE_INV);
    EXPECT_EQ(cali_attribute_name(CALI_INV_ID), nullptr);
    EXPECT_EQ(cali_find_attribute("test.attribute.api.nope"), CALI_INV_ID);
    EXPECT_FALSE(Caliper::instance().attribute_exists("test.attribute.api.nope"));

}

TEST(AttributeAPITest, GlobalAttributes) {
    Caliper c;

    Attribute global_attr =
        c.create_attribute("test.attribute.global", CALI_TYPE_INT, CALI_ATTR_GLOBAL);

    ASSERT_NE(global_attr, Attribute::invalid);

    // global attributes should always have process scope
    EXPECT_EQ(global_attr.properties() & CALI_ATTR_SCOPE_MASK, CALI_ATTR_SCOPE_PROCESS);

    // cali.caliper.version should be a global attribute
    EXPECT_NE(c.get_attribute("cali.caliper.version").properties() & CALI_ATTR_GLOBAL, 0);

    c.set(global_attr, Variant(42));

    std::vector<Entry> globals = c.get_globals();

    auto it = std::find_if(globals.begin(), globals.end(),
                           [global_attr](const Entry& e) {
                               return e.count(global_attr) > 0;
                           } );

    ASSERT_NE(it, globals.end());

    EXPECT_EQ(it->value(global_attr).to_int(), 42);
}

TEST(AttributeAPITest, NestedAttribute) {
    Caliper c;

    Attribute nested_a =
        c.create_attribute("test.attr.nested.a", CALI_TYPE_INT, CALI_ATTR_NESTED);
    Attribute nested_b =
        c.create_attribute("test.attr.nested.b", CALI_TYPE_INT, CALI_ATTR_NESTED);
    Attribute nomerge  =
        c.create_attribute("test.attr.nomerge",  CALI_TYPE_INT, CALI_ATTR_NESTED | CALI_ATTR_NOMERGE);

    EXPECT_TRUE(nested_a.is_nested());
    EXPECT_TRUE(nested_a.is_autocombineable());
    EXPECT_TRUE(nomerge.is_nested());
    EXPECT_FALSE(nomerge.is_autocombineable());

    c.begin(nested_a, Variant(16));
    c.begin(nomerge,  Variant(25));
    c.begin(nested_b, Variant(36));

    const Node* node = c.get(nested_b).node();

    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->attribute(), nested_b.id());

    node = node->parent();
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->attribute(), nested_a.id());
    EXPECT_EQ(node->data().to_int(), 16);

    node = c.get(nomerge).node();

    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->attribute(), nomerge.id());
    EXPECT_EQ(node->data().to_int(), 25);

    // nomerge attribute should have hidden root node as parent even though it's nested
    node = node->parent();
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->attribute(), CALI_INV_ID);

    c.end(nested_b);
    c.end(nomerge);
    c.end(nested_a);
}
