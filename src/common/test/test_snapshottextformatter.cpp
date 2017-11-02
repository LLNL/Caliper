#include "caliper/common/SnapshotTextFormatter.h"

#include "MockupMetadataDB.h"

#include "gtest/gtest.h"

#include <sstream>

using namespace cali;

TEST(SnapshotTextFormatterTest, FormatTest) {
    Node* strtype_attr_n = new Node(3, 9, Variant(CALI_TYPE_STRING));
    Node* inttype_attr_n = new Node(1, 9, Variant(CALI_TYPE_INT));
    
    Node* str_attr = new Node(100, 8, Variant(CALI_TYPE_STRING, "str.attr", 9));
    Node* int_attr = new Node(101, 8, Variant(CALI_TYPE_STRING, "int.attr", 9));

    strtype_attr_n->append(str_attr);
    inttype_attr_n->append(int_attr);
    
    MockupMetadataDB db;

    db.add_attribute(Attribute::make_attribute(str_attr));
    db.add_attribute(Attribute::make_attribute(int_attr));

    Node* str_node = new Node(200, 100, Variant(CALI_TYPE_STRING, "whee", 5));
    Node* int_node = new Node(101, 101, Variant(42));

    str_node->append(int_node);

    db.add_node(str_node);
    db.add_node(int_node);

    SnapshotTextFormatter format("whoo %str.attr%-%[2]str.attr%%int.attr%-%[6]str.attr%-%[4]int.attr%-end");

    {
        std::ostringstream os;
        format.print(os, db, { Entry(int_node) });

        EXPECT_EQ(os.str(), std::string("whoo whee-whee42-whee  -  42-end"));
    }
    
    format.reset("%[1]int.attr%%str.attr%");

    {
        std::ostringstream os;
        format.print(os, db, { Entry(int_node) });

        EXPECT_EQ(os.str(), std::string("42whee"));
    }    
}
