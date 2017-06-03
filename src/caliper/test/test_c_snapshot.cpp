#include "MockupMetadataDB.h"

#include "../cali.h"
#include "../CompressedSnapshotRecord.h"

#include "Node.h"

#include <gtest/gtest.h>

using namespace cali;

//
// --- tests
//

namespace
{
    struct entry_data_t {
        cali_id_t      attr_id;
        cali_variant_t val;
    };
    
    struct UnpackSnapshotTestData {
        int max_visit_count;
        int visit_count;
        std::vector<entry_data_t> entries;

        UnpackSnapshotTestData()
            : max_visit_count(-1),
              visit_count(0)
        { }
    };

    int test_entry_proc_op(void* user_arg, cali_id_t attr_id, cali_variant_t val) {
        UnpackSnapshotTestData* arg = static_cast<UnpackSnapshotTestData*>(user_arg);

        ++arg->visit_count;
        
        if (arg->max_visit_count >= 0 && arg->visit_count >= arg->max_visit_count)
            return 0; // quit

        arg->entries.push_back(entry_data_t({ attr_id, val }));

        return 1;
    }
}

TEST(C_Snapshot_Test, UnpackEmpty) {
    CompressedSnapshotRecord rec;

    EXPECT_EQ(rec.num_nodes(), 0);
    EXPECT_EQ(rec.num_immediates(), 0);

    UnpackSnapshotTestData t1;
    size_t bytes_read = 0;

    cali_unpack_snapshot(rec.data(), &bytes_read, ::test_entry_proc_op, &t1);

    EXPECT_EQ(t1.visit_count, 0);
    EXPECT_EQ(t1.entries.size(), 0);
}

TEST(C_Snapshot_Test, UnpackImmediates) {
    cali_id_t attr_in[] = { 7, CALI_INV_ID, 42, 1337 };
    Variant   data_in[] = { Variant(CALI_TYPE_TYPE), Variant(), Variant(1.23), Variant(true) };

    CompressedSnapshotRecord rec;

    EXPECT_EQ(rec.append(4, attr_in, data_in), 0);

    ASSERT_EQ(rec.num_nodes(), 0);
    ASSERT_EQ(rec.num_immediates(), 4);

    UnpackSnapshotTestData t1;
    size_t bytes_read = 0;
    
    cali_unpack_snapshot(rec.data(), &bytes_read, ::test_entry_proc_op, &t1);

    EXPECT_EQ(t1.visit_count, 3);
    EXPECT_EQ(t1.entries.size(), 3);
    
    for (int i : { 0, 2, 3 } ) {
        auto it = std::find_if(t1.entries.begin(), t1.entries.end(),
                               [i,attr_in,data_in](const entry_data_t& e) {
                                   return (attr_in[i] == e.attr_id && data_in[i] == e.val);
                               });

        EXPECT_NE(it, t1.entries.end()) << " immediate entry ("
                                        << attr_in[i] << "," << data_in[i]
                                        << ") not found!" << std::endl;
    }
}
