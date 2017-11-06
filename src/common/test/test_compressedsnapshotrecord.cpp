#include "caliper/common/CompressedSnapshotRecord.h"

#include "MockupMetadataDB.h"

#include "caliper/common/Node.h"

#include <gtest/gtest.h>

using namespace cali;

//
// --- tests
//

TEST(CompressedSnapshotRecordTest, Append) {
    cali_id_t attr_in[3] = { 7, CALI_INV_ID, 42 };
    Variant   data_in[3] = { Variant(CALI_TYPE_INT), Variant(), Variant(1.23) };

    Node* n1 = new Node(1, 1, Variant(CALI_TYPE_STRING, "whee", 4));
    Node* n2 = new Node(2, 2, Variant(-1.0));
    Node* n3 = new Node(3, 2, Variant(42.0));

    n1->append(n2);
    n1->append(n3);

    const Node* node_in[2] = { n2, n3 };

    CompressedSnapshotRecord rec;

    EXPECT_EQ(rec.append(1, attr_in, data_in), static_cast<size_t>(0));
    EXPECT_EQ(rec.append(2, node_in), static_cast<size_t>(0));
    EXPECT_EQ(rec.append(2, attr_in+1, data_in+1), static_cast<size_t>(0));

    ASSERT_EQ(rec.num_nodes(), static_cast<size_t>(2));
    ASSERT_EQ(rec.num_immediates(), static_cast<size_t>(3));

    cali_id_t node_out[2];
    cali_id_t attr_out[3];
    Variant   data_out[3];

    CompressedSnapshotRecordView view(rec.view());

    view.unpack_nodes(2, node_out);
    view.unpack_immediate(3, attr_out, data_out);

    EXPECT_EQ(node_out[0], n2->id());
    EXPECT_EQ(node_out[1], n3->id());

    EXPECT_EQ(attr_out[0], attr_in[0]);
    EXPECT_EQ(attr_out[1], attr_in[1]);
    EXPECT_EQ(attr_out[2], attr_in[2]);

    EXPECT_EQ(data_out[0], data_in[0]);
    EXPECT_EQ(data_out[1], data_in[1]);
    EXPECT_EQ(data_out[2], data_in[2]);

    delete n3;
    delete n2;
    delete n1;
}

TEST(CompressedSnapshotRecordTest, AppendEntrylist) {
    cali_id_t attr_in[3] = { 7, CALI_INV_ID, 42 };
    Variant   data_in[3] = { Variant(CALI_TYPE_INT), Variant(), Variant(1.23) };

    Node* n1 = new Node(1, 1, Variant(CALI_TYPE_STRING, "whee", 4));
    Node* n2 = new Node(2, 2, Variant(-1.0));
    Node* n3 = new Node(3, 2, Variant(42.0));

    n1->append(n2);
    n1->append(n3);

    std::vector<Entry> entrylist { Entry(n2),
            Entry(attr_in[0], data_in[0]),
            Entry(n3),
            Entry(attr_in[1], data_in[1]),
            Entry(attr_in[2], data_in[2]) };

    CompressedSnapshotRecord rec;

    ASSERT_EQ(rec.append(entrylist.size(), entrylist.data()), static_cast<size_t>(0));

    ASSERT_EQ(rec.num_nodes(), static_cast<size_t>(2));
    ASSERT_EQ(rec.num_immediates(), static_cast<size_t>(2)); // adding through entrylist skips invalid entry (attr_in[1])

    cali_id_t node_out[2];
    cali_id_t attr_out[2];
    Variant   data_out[2];

    CompressedSnapshotRecordView view(rec.view());

    view.unpack_nodes(2, node_out);
    view.unpack_immediate(2, attr_out, data_out);

    EXPECT_EQ(node_out[0], n2->id());
    EXPECT_EQ(node_out[1], n3->id());

    EXPECT_EQ(attr_out[0], attr_in[0]);
    EXPECT_EQ(attr_out[1], attr_in[2]); // attr_in[1] was skipped

    EXPECT_EQ(data_out[0], data_in[0]);
    EXPECT_EQ(data_out[1], data_in[2]); // data_in[1] was skipped

    delete n3;
    delete n2;
    delete n1;
}

TEST(CompressedSnapshotRecordTest, Decode) {
    cali_id_t attr_in[3] = { 7, CALI_INV_ID, 42 };
    Variant   data_in[3] = { Variant(CALI_TYPE_INT), Variant(), Variant(1.23) };

    Node* n1 = new Node(1, 1, Variant(CALI_TYPE_STRING, "whee", 4));
    Node* n2 = new Node(2, 2, Variant(-1.0));
    Node* n3 = new Node(3, 2, Variant(42.0));

    n1->append(n2);
    n1->append(n3);

    const Node* node_in[2] = { n2, n3 };

    CompressedSnapshotRecord rec;

    EXPECT_EQ(rec.append(1, attr_in, data_in), static_cast<size_t>(0));
    EXPECT_EQ(rec.append(2, node_in), static_cast<size_t>(0));
    EXPECT_EQ(rec.append(2, attr_in+1, data_in+1), static_cast<size_t>(0));

    ASSERT_EQ(rec.num_nodes(), static_cast<size_t>(2));
    ASSERT_EQ(rec.num_immediates(), static_cast<size_t>(3));

    cali_id_t node_out[2];
    cali_id_t attr_out[3];
    Variant   data_out[3];

    size_t pos;
    CompressedSnapshotRecordView view(rec.data(), &pos);

    ASSERT_EQ(view.num_nodes(), rec.num_nodes());
    ASSERT_EQ(view.num_immediates(), rec.num_immediates());

    view.unpack_nodes(2, node_out);
    view.unpack_immediate(3, attr_out, data_out);

    EXPECT_EQ(node_out[0], n2->id());
    EXPECT_EQ(node_out[1], n3->id());

    EXPECT_EQ(attr_out[0], attr_in[0]);
    EXPECT_EQ(attr_out[1], attr_in[1]);
    EXPECT_EQ(attr_out[2], attr_in[2]);

    EXPECT_EQ(data_out[0], data_in[0]);
    EXPECT_EQ(data_out[1], data_in[1]);
    EXPECT_EQ(data_out[2], data_in[2]);

    delete n3;
    delete n2;
    delete n1;
}

TEST(CompressedSnapshotRecordTest, MakeEntrylist) {
    cali_id_t attr_in[3] = { 7, CALI_INV_ID, 42 };
    Variant   data_in[3] = { Variant(CALI_TYPE_INT), Variant(), Variant(1.23) };

    Node* n1 = new Node(1, 1, Variant(CALI_TYPE_STRING, "whee", 4));
    Node* n2 = new Node(2, 2, Variant(-1.0));
    Node* n3 = new Node(3, 2, Variant(42.0));

    n1->append(n2);
    n1->append(n3);

    Node* node_in[2] = { n2, n3 };

    CompressedSnapshotRecord rec;

    EXPECT_EQ(rec.append(1, attr_in, data_in), static_cast<size_t>(0));
    EXPECT_EQ(rec.append(2, node_in), static_cast<size_t>(0));
    EXPECT_EQ(rec.append(2, attr_in+1, data_in+1), static_cast<size_t>(0));

    ASSERT_EQ(rec.num_nodes(), static_cast<size_t>(2));
    ASSERT_EQ(rec.num_immediates(), static_cast<size_t>(3));

    MockupMetadataDB db;
    db.add_nodes(2, node_in);

    std::vector<Entry> list_out = rec.view().to_entrylist(&db);

    ASSERT_EQ(list_out.size(), 5);

    // Assume <nodes> <immediates> order in list for the test for now: exploits
    // implementation detail that's not guaranteed by the interface

    Entry list_in[] = {
        Entry(n2),
        Entry(n3),
        Entry(attr_in[0], data_in[0]),
        Entry(attr_in[1], data_in[1]),
        Entry(attr_in[2], data_in[2])
    };

    for (size_t i = 0; i < 5; ++i) {
        EXPECT_TRUE(list_in[i] == list_out[i]) << " differs for entry " << i;
    }
}

namespace
{
    class UnpackTester {
        int m_count;
        int m_max_count;

        std::vector<Entry> m_in_list;

        void check_entry(const Entry& e) {
            auto it = std::find(m_in_list.begin(), m_in_list.end(), e);

            EXPECT_FALSE(it == m_in_list.end()) << " Entry no " << m_count << " ("
                                                << e.attribute() << ","
                                                << e.value()
                                                << ") not found!";

            if (it != m_in_list.end())
                m_in_list.erase(it);
        }

    public:

        UnpackTester(size_t n, const Entry in_list[], int max_count)
            : m_count(0), m_max_count(max_count)
        {
            for (size_t i = 0; i < n; ++i)
                m_in_list.push_back(in_list[i]);
        }

        int count() const {
            return m_count;
        }

        bool handle_entry(const Entry& e) {
            ++m_count;

            if (m_max_count > 0 && m_count >= m_max_count)
                return false;

            check_entry(e);

            return true;
        }

    };
}

TEST(CompressedSnapshotRecordTest, Unpack) {
    cali_id_t attr_in[3] = { 7, CALI_INV_ID, 42 };
    Variant   data_in[3] = { Variant(CALI_TYPE_INT), Variant(), Variant(1.23) };

    Node* n1 = new Node(1, 1, Variant(CALI_TYPE_STRING, "whee", 4));
    Node* n2 = new Node(2, 2, Variant(-1.0));
    Node* n3 = new Node(3, 2, Variant(42.0));

    n1->append(n2);
    n1->append(n3);

    Node* node_in[2] = { n2, n3 };

    CompressedSnapshotRecord rec;

    EXPECT_EQ(rec.append(1, attr_in, data_in), static_cast<size_t>(0));
    EXPECT_EQ(rec.append(2, node_in), static_cast<size_t>(0));
    EXPECT_EQ(rec.append(2, attr_in+1, data_in+1), static_cast<size_t>(0));

    ASSERT_EQ(rec.num_nodes(), static_cast<size_t>(2));
    ASSERT_EQ(rec.num_immediates(), static_cast<size_t>(3));

    MockupMetadataDB db;
    db.add_nodes(2, node_in);

    Entry list_in[5] = {
        Entry(n2),
        Entry(n3),
        Entry(attr_in[0], data_in[0]),
        Entry(attr_in[1], data_in[1]),
        Entry(attr_in[2], data_in[2])
    };

    CompressedSnapshotRecordView view(rec.view());

    ::UnpackTester t1(5, list_in, -1);

    view.unpack(&db, [&t1](const Entry& e){ return t1.handle_entry(e); });

    EXPECT_EQ(t1.count(), 5);

    ::UnpackTester t2(5, list_in, 2);

    view.unpack(&db, [&t2](const Entry& e){ return t2.handle_entry(e); });

    EXPECT_EQ(t2.count(), 2);
}
