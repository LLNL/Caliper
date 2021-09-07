#include "../SnapshotTableFormatter.h"

#include "caliper/reader/CaliperMetadataDB.h"

#include "caliper/common/Node.h"

#include <gtest/gtest.h>

#include <sstream>

using namespace cali;

TEST(SnapshotTableFormatter, Format) {
    CaliperMetadataDB db;

    Attribute a = db.create_attribute("aaaa", CALI_TYPE_INT,    CALI_ATTR_ASVALUE);
    Attribute b = db.create_attribute("bb",   CALI_TYPE_UINT,   CALI_ATTR_ASVALUE);
    Attribute s = db.create_attribute("str",  CALI_TYPE_STRING, CALI_ATTR_DEFAULT);

    IdMap idmap;
    const Node* node = db.merge_node(101, s.id(), CALI_INV_ID, Variant("a string value"), idmap);

    std::vector<Entry> rec;
    rec.push_back(Entry(a,   Variant(42)));
    rec.push_back(Entry(b, Variant(4242)));
    rec.push_back(Entry(node));

    std::ostringstream os;
    format_record_as_table(db, rec, os);

    std::string expect =
        "aaaa :   42 \n"
        "bb   : 4242 \n"
        "str  : a string value\n";

    EXPECT_EQ(os.str(), expect);
}