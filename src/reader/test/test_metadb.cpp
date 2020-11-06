#include "caliper/reader/CaliperMetadataDB.h"

#include "caliper/common/Node.h"

#include <gtest/gtest.h>

using namespace cali;

TEST(MetaDBTest, MergeSnapshotFromDB) {
    CaliperMetadataDB db1;

    Attribute str_attr =
        db1.create_attribute("str.attr", CALI_TYPE_STRING, CALI_ATTR_DEFAULT);
    Attribute int_attr =
        db1.create_attribute("int.attr", CALI_TYPE_INT,    CALI_ATTR_ASVALUE);

    IdMap idmap;

    const Node* a_in =
        db1.merge_node(200, str_attr.id(), CALI_INV_ID, Variant("a"), idmap);
    const Node* b_in =
        db1.merge_node(201, str_attr.id(), 200,         Variant("b"), idmap);

    EntryList list_in { Entry(b_in), Entry(int_attr, Variant(42)) };

    CaliperMetadataDB db2;

    EntryList list_out =
        db2.merge_snapshot(db1, list_in);

    Attribute str_attr_out = db2.get_attribute("str.attr");
    Attribute int_attr_out = db2.get_attribute("int.attr");

    EXPECT_EQ(str_attr_out.type(), CALI_TYPE_STRING);
    EXPECT_EQ(int_attr_out.type(), CALI_TYPE_INT);

    EXPECT_NE(str_attr.node(), str_attr_out.node());
    EXPECT_NE(int_attr.node(), int_attr_out.node());

    ASSERT_EQ(list_out.size(), 2);

    // assume order is nodes, immediate

    const Node* b_out = list_out[0].node();

    ASSERT_NE(b_out, nullptr);
    ASSERT_NE(b_out->parent(), nullptr);
    EXPECT_EQ(b_out->attribute(), str_attr_out.id());
    EXPECT_EQ(b_out->data().to_string(), "b");
    EXPECT_EQ(b_out->parent()->attribute(), str_attr_out.id());
    EXPECT_EQ(b_out->parent()->data().to_string(), "a");

    EXPECT_EQ(list_out[1].attribute(), int_attr_out.id());
    EXPECT_EQ(list_out[1].value().to_int(), 42);

    EXPECT_NE(b_out, b_in);
}

namespace
{

int count_in_record(const std::vector<Entry>& rec, const Attribute& a, const Variant& v)
{
    int count = 0;

    for (const Entry& e : rec)
        if (e.is_reference()) {
            for (const Node* node = e.node(); node; node = node->parent())
                if (node->attribute() == a.id() && node->data() == v)
                    ++count;
        } else if (e.is_immediate() && e.attribute() == a.id() && e.value() == v)
            ++count;

    return count;
}

}

TEST(MetaDBTest, SetGlobal) {
    CaliperMetadataDB db;

    Attribute g_str_attr =
        db.create_attribute("global.str", CALI_TYPE_STRING, CALI_ATTR_GLOBAL);
    Attribute g_int_attr =
        db.create_attribute("global.int", CALI_TYPE_INT,    CALI_ATTR_GLOBAL);
    Attribute g_val_attr =
        db.create_attribute("global.val", CALI_TYPE_INT,    CALI_ATTR_GLOBAL | CALI_ATTR_ASVALUE);
    Attribute no_g_attr  =
        db.create_attribute("noglobal",   CALI_TYPE_INT,    CALI_ATTR_DEFAULT);

    Variant v_str_a(CALI_TYPE_STRING, "a", 1);
    Variant v_str_b(CALI_TYPE_STRING, "b", 1);
    Variant v_int(42);
    Variant v_val(cali_make_variant_from_int64(-9876543210));
    Variant v_no(-42);

    db.set_global(g_str_attr, v_str_a);
    db.set_global(g_int_attr, v_int  );
    db.set_global(g_val_attr, v_val  );
    db.set_global(g_str_attr, v_str_a); // should be set only once
    db.set_global(g_str_attr, v_str_b);
    db.set_global(no_g_attr,  v_no   );

    std::vector<Entry> globals = db.get_globals();

    EXPECT_GE(globals.size(), 2);

    EXPECT_EQ(count_in_record(globals, g_str_attr, v_str_a), 1);
    EXPECT_EQ(count_in_record(globals, g_str_attr, v_str_b), 1);
    EXPECT_EQ(count_in_record(globals, g_int_attr, v_int  ), 1);
    EXPECT_EQ(count_in_record(globals, g_val_attr, v_val  ), 1);
    EXPECT_EQ(count_in_record(globals, no_g_attr,  v_no   ), 0);

    CaliperMetadataDB db_imp;

    db_imp.import_globals(db);

    std::vector<Entry> imp_globals = db_imp.get_globals();

    Attribute g_imp_str_attr = db_imp.get_attribute("global.str");
    Attribute g_imp_int_attr = db_imp.get_attribute("global.int");
    Attribute g_imp_val_attr = db_imp.get_attribute("global.val");

    EXPECT_GE(imp_globals.size(), 2);

    EXPECT_EQ(count_in_record(imp_globals, g_imp_str_attr, v_str_a), 1);
    EXPECT_EQ(count_in_record(imp_globals, g_imp_str_attr, v_str_b), 1);
    EXPECT_EQ(count_in_record(imp_globals, g_imp_int_attr, v_int  ), 1);
    EXPECT_EQ(count_in_record(imp_globals, g_imp_val_attr, v_val  ), 1);
}

TEST(MetadataDBTest, StringDB) {
    CaliperMetadataDB db;

    Attribute attr =
        db.create_attribute("string.attr", CALI_TYPE_STRING, CALI_ATTR_DEFAULT);

    IdMap idmap;

    const Node* n0 =
        db.merge_node(100, attr.id(), CALI_INV_ID, Variant("a.b"  ), idmap);
    const Node* n1 =
        db.merge_node(101, attr.id(), 100,         Variant("a"    ), idmap);
    const Node* n2 =
        db.merge_node(102, attr.id(), 101,         Variant("a.b.c"), idmap);
    const Node* n3 =
        db.merge_node(103, attr.id(), 102,         Variant("a.b"  ), idmap);

    EXPECT_EQ(n0->data().to_string(), std::string("a.b"));
    EXPECT_EQ(n1->data().to_string(), std::string("a"));
    EXPECT_EQ(n2->data().to_string(), std::string("a.b.c"));
    EXPECT_EQ(n3->data().to_string(), std::string("a.b"));

    EXPECT_EQ(n3->data().data(), n0->data().data());

    std::ostringstream os;
    db.print_statistics(os);

    // 21 nodes: 15 default nodes + 2 attribute nodes + 4 test nodes
    // 6 strings: 3 attribute name + 3 different test node strings
    EXPECT_EQ(os.str(), std::string("CaliperMetadataDB: stored 21 nodes, 6 strings.\n"));
}

TEST(MetadataDBTest, AliasesAndUnits) {
    CaliperMetadataDB db;

    std::map<std::string, std::string> aliases = { std::make_pair("x.attr", "x alias") };
    std::map<std::string, std::string> units   = { std::make_pair("x.attr", "x unit")  };

    db.add_attribute_aliases(aliases);
    db.add_attribute_units(units);

    Attribute attr =
        db.create_attribute("x.attr", CALI_TYPE_INT, CALI_ATTR_DEFAULT);

    Attribute alias_attr = db.get_attribute("attribute.alias");
    Attribute unit_attr  = db.get_attribute("attribute.unit");

    EXPECT_EQ(attr.get(alias_attr).to_string(), "x alias");
    EXPECT_EQ(attr.get(unit_attr).to_string(),  "x unit");
}
