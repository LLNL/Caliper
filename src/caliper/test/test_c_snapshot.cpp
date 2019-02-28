#include "MockupMetadataDB.h"

#include "caliper/cali.h"
#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/common/CompressedSnapshotRecord.h"
#include "caliper/common/Node.h"

#include <gtest/gtest.h>

using namespace cali;

//
// --- tests
//

namespace
{
    struct entry_data_t {
        cali_id_t attr_id;
        Variant   val;
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

        if (arg->max_visit_count >= 0 && arg->visit_count >= arg->max_visit_count)
            return 0; // quit

        arg->entries.push_back(entry_data_t({ attr_id, val }));
        ++arg->visit_count;

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

TEST(C_Snapshot_Test, Unpack) {
    // Mixed node/immediate record unpack test. Modifies a Caliper instance.

    Caliper c;

    Attribute node_str_attr =
        c.create_attribute("unpack.node.str", CALI_TYPE_STRING, CALI_ATTR_DEFAULT);
    Attribute node_int_attr =
        c.create_attribute("unpack.node.int", CALI_TYPE_INT, CALI_ATTR_DEFAULT);
    Attribute val_int_attr =
        c.create_attribute("unpac.val.int", CALI_TYPE_INT, CALI_ATTR_ASVALUE);

    Variant node_str_1(CALI_TYPE_STRING, "My wonderful unpack test string", 32);
    Variant node_str_2(CALI_TYPE_STRING, "My other unpack test string", 27);

    Variant node_int_1(42);
    Variant node_int_2(1337);

    Variant val_int_1(2020);
    Variant val_int_2(1212);

    SnapshotRecord::FixedSnapshotRecord<20> snapshot_data;
    SnapshotRecord snapshot(snapshot_data);

    Attribute attr_in[] = {
        node_str_attr,
        node_int_attr,
        val_int_attr,
        node_str_attr,
        val_int_attr,
        node_int_attr
    };
    Variant data_in[] = {
        node_str_1,
        node_int_1,
        val_int_1,
        node_str_2,
        val_int_2,
        node_int_2
    };

    c.make_record(6, attr_in, data_in, snapshot);

    ASSERT_EQ(snapshot.size().n_nodes, 1);
    ASSERT_EQ(snapshot.size().n_immediate, 2);

    CompressedSnapshotRecord rec;

    ASSERT_EQ(rec.append(&snapshot), 0);

    {
        // do a full unpack

        UnpackSnapshotTestData t1;
        size_t bytes_read = 0;

        cali_unpack_snapshot(rec.data(), &bytes_read, ::test_entry_proc_op, &t1);

        EXPECT_EQ(bytes_read, rec.size());

        EXPECT_EQ(t1.visit_count, 6);
        EXPECT_EQ(t1.entries.size(), 6);

        for (int i = 0; i < 6; ++i) {
            auto it = std::find_if(t1.entries.begin(), t1.entries.end(),
                                   [i,attr_in,data_in](const entry_data_t& e) {
                                       return (attr_in[i].id() == e.attr_id && data_in[i] == e.val);
                                   });

            EXPECT_NE(it, t1.entries.end()) << " entry ("
                                            << attr_in[i] << "," << data_in[i]
                                            << ") not found!" << std::endl;
        }
    }

    {
        // do a partial unpack (quit after 2 entries)

        UnpackSnapshotTestData t2;
        size_t bytes_read = 0;

        t2.max_visit_count = 2;

        cali_unpack_snapshot(rec.data(), &bytes_read, ::test_entry_proc_op, &t2);

        EXPECT_EQ(bytes_read, rec.size());

        EXPECT_EQ(t2.visit_count, 2);
        EXPECT_EQ(t2.entries.size(), 2);

        for (entry_data_t e : t2.entries) {
            // just check values
            int p = 0;

            for ( ; p < 6 && data_in[p] != e.val; ++p)
                ;

            EXPECT_LT(p, 6) << " entry (" << e.attr_id << "," << e.val << ") not found!";
        }
    }
}

TEST(C_Snapshot_Test, PullSnapshot) {
    // Pull a snapshot with the C interface. Modifies the Caliper instance.

    Caliper c;

    Attribute node_str_attr =
        c.create_attribute("pull.node.str", CALI_TYPE_STRING, CALI_ATTR_DEFAULT);
    Attribute node_int_attr =
        c.create_attribute("pull.node.int", CALI_TYPE_INT, CALI_ATTR_DEFAULT);
    Attribute val_int_attr =
        c.create_attribute("pull.val.int", CALI_TYPE_INT, CALI_ATTR_ASVALUE);
    Attribute val_dbl_attr =
        c.create_attribute("pull.val.dbl", CALI_TYPE_DOUBLE, CALI_ATTR_ASVALUE);

    Variant node_str_1(CALI_TYPE_STRING, "My wonderful pull test string", 30);
    Variant node_str_2(CALI_TYPE_STRING, "My other pull test string", 25);

    Variant node_int_1(42);
    Variant node_int_2(1337);

    Variant val_int_1(2020);
    Variant val_dbl_2(0.25);

    SnapshotRecord::FixedSnapshotRecord<20> snapshot_data;
    SnapshotRecord snapshot(snapshot_data);

    Attribute attr_in[] = {
        node_str_attr,
        node_int_attr,
        val_int_attr,
        node_str_attr,
        val_dbl_attr,
        node_int_attr
    };
    Variant data_in[] = {
        node_str_1,
        node_int_1,
        val_int_1,
        node_str_2,
        val_dbl_2,
        node_int_2
    };

    const char* cfg[][2] = {
        { NULL, NULL }
    };

    cali_configset_t cfgset = cali_create_configset(cfg);
    cali_id_t test_channel = cali_create_channel("test.push_snapshot", 0, cfgset);
    cali_delete_configset(cfgset);

    const int count = 6;

    for (int p = 0; p < count; ++p)
        c.begin(attr_in[p], data_in[p]);

    {
        // full snapshot

        const size_t bufsize = 512;
        unsigned char buf[512];

        size_t ret =
            cali_channel_pull_snapshot(test_channel, CALI_SCOPE_THREAD, bufsize, buf);

        ASSERT_NE(ret, 0);
        ASSERT_LE(ret, bufsize);

        UnpackSnapshotTestData t1;
        size_t bytes_read = 0;

        cali_unpack_snapshot(buf, &bytes_read, ::test_entry_proc_op, &t1);

        EXPECT_EQ(ret, bytes_read);

        EXPECT_GE(t1.visit_count, count);
        EXPECT_GE(t1.entries.size(), count);

        for (int i = 0; i < count; ++i) {
            auto it = std::find_if(t1.entries.begin(), t1.entries.end(),
                                   [i,attr_in,data_in](const entry_data_t& e) {
                                       return (attr_in[i].id() == e.attr_id && data_in[i] == e.val);
                                   });

            EXPECT_NE(it, t1.entries.end()) << " entry ("
                                            << attr_in[i] << "," << data_in[i]
                                            << ") not found!" << std::endl;
        }
    }

    {
        // case with too small buffer

        const size_t bufsize = 4;
        unsigned char buf[512];

        size_t ret =
            cali_channel_pull_snapshot(test_channel, CALI_SCOPE_THREAD, bufsize, buf);

        ASSERT_NE(ret, 0);
        EXPECT_GT(ret, bufsize);

        UnpackSnapshotTestData t2;
        size_t bytes_read = 0;

        cali_unpack_snapshot(buf, &bytes_read, ::test_entry_proc_op, &t2);

        EXPECT_GE(t2.visit_count, 0);
        EXPECT_LT(t2.visit_count, count);

        // now the correctly sized buffer again

        ASSERT_LE(ret, 512);

        ret = cali_channel_pull_snapshot(test_channel, CALI_SCOPE_THREAD, ret, buf);

        ASSERT_NE(ret, 0);
        ASSERT_LT(ret, 512);

        UnpackSnapshotTestData t3;

        cali_unpack_snapshot(buf, &bytes_read, ::test_entry_proc_op, &t3);

        EXPECT_GE(t3.visit_count, count);
        EXPECT_GE(t3.entries.size(), count);
    }

    for (int p = count-1; p >= 0; --p)
        c.end(attr_in[p]);

    cali_delete_channel(test_channel);
}

TEST(C_Snapshot_Test, FindFirstInSnapshot) {
     // Mixed node/immediate record unpack test. Modifies a Caliper instance.

    Caliper c;

    Attribute node_str_attr =
        c.create_attribute("findfirst.node.str", CALI_TYPE_STRING, CALI_ATTR_DEFAULT);
    Attribute node_int_attr =
        c.create_attribute("findfirst.node.int", CALI_TYPE_INT, CALI_ATTR_DEFAULT);
    Attribute val_int_attr =
        c.create_attribute("findfirst.val.int", CALI_TYPE_INT, CALI_ATTR_ASVALUE);

    Variant node_str_1(CALI_TYPE_STRING, "My wonderful unpack test string", 32);
    Variant node_str_2(CALI_TYPE_STRING, "My other unpack test string", 27);

    Variant node_int_1(42);
    Variant node_int_2(1337);

    Variant val_int_1(2020);
    Variant val_int_2(1212);

    SnapshotRecord::FixedSnapshotRecord<20> snapshot_data;
    SnapshotRecord snapshot(snapshot_data);

    Attribute attr_in[] = {
        node_str_attr,
        node_int_attr,
        val_int_attr,
        node_str_attr,
        val_int_attr,
        node_int_attr
    };
    Variant data_in[] = {
        node_str_1,
        node_int_1,
        val_int_1,
        node_str_2,
        val_int_2,
        node_int_2
    };

    c.make_record(6, attr_in, data_in, snapshot);

    ASSERT_EQ(snapshot.size().n_nodes, 1);
    ASSERT_EQ(snapshot.size().n_immediate, 2);

    CompressedSnapshotRecord rec;

    ASSERT_EQ(rec.append(&snapshot), 0);

    UnpackSnapshotTestData t1;
    size_t bytes_read = 0;

    cali_variant_t val = cali_find_first_in_snapshot(rec.data(), node_str_attr.id(), &bytes_read);

    EXPECT_EQ(bytes_read, rec.size());
    EXPECT_EQ(Variant(val), node_str_2);

    bytes_read = 0;
    val = cali_find_first_in_snapshot(rec.data(), node_int_attr.id(), &bytes_read);

    EXPECT_EQ(bytes_read, rec.size());
    EXPECT_EQ(Variant(val), node_int_2);

    bytes_read = 0;
    val = cali_find_first_in_snapshot(rec.data(), val_int_attr.id(), &bytes_read);

    EXPECT_EQ(bytes_read, rec.size());
    EXPECT_EQ(Variant(val), val_int_1);

    // a non-existing attribute

    bytes_read = 0;
    val = cali_find_first_in_snapshot(rec.data(), CALI_INV_ID, &bytes_read);

    EXPECT_EQ(bytes_read, rec.size());
    EXPECT_TRUE(Variant(val).empty());
}

TEST(C_Snapshot_Test, FindAllInSnapshot) {
    // Mixed node/immediate record unpack test. Modifies a Caliper instance.

    Caliper c;

    Attribute node_str_attr =
        c.create_attribute("findall.node.str", CALI_TYPE_STRING, CALI_ATTR_DEFAULT);
    Attribute node_int_attr =
        c.create_attribute("findall.node.int", CALI_TYPE_INT, CALI_ATTR_DEFAULT);
    Attribute val_int_attr =
        c.create_attribute("findall.val.int", CALI_TYPE_INT, CALI_ATTR_ASVALUE);

    Variant node_str_1(CALI_TYPE_STRING, "My wonderful findall test string", 33);
    Variant node_str_2(CALI_TYPE_STRING, "My other findall test string", 28);

    Variant node_int_1(42);
    Variant node_int_2(1337);

    Variant val_int_1(2020);
    Variant val_int_2(1212);

    SnapshotRecord::FixedSnapshotRecord<20> snapshot_data;
    SnapshotRecord snapshot(snapshot_data);

    Attribute attr_in[] = {
        node_str_attr,
        node_int_attr,
        val_int_attr,
        node_str_attr,
        val_int_attr,
        node_int_attr
    };
    Variant data_in[] = {
        node_str_1,
        node_int_1,
        val_int_1,
        node_str_2,
        val_int_2,
        node_int_2
    };

    c.make_record(6, attr_in, data_in, snapshot);

    ASSERT_EQ(snapshot.size().n_nodes, 1);
    ASSERT_EQ(snapshot.size().n_immediate, 2);

    CompressedSnapshotRecord rec;

    ASSERT_EQ(rec.append(&snapshot), 0);

    {
        UnpackSnapshotTestData t1;
        size_t bytes_read = 0;

        cali_find_all_in_snapshot(rec.data(), node_str_attr.id(), &bytes_read, ::test_entry_proc_op, &t1);

        EXPECT_EQ(bytes_read, rec.size());

        EXPECT_EQ(t1.visit_count, 2);
        ASSERT_EQ(t1.entries.size(), 2);

        EXPECT_EQ(t1.entries[0].val, node_str_2);
        EXPECT_EQ(t1.entries[1].val, node_str_1);
    }

    {
        UnpackSnapshotTestData t2;
        size_t bytes_read = 0;

        cali_find_all_in_snapshot(rec.data(), node_int_attr.id(), &bytes_read, ::test_entry_proc_op, &t2);

        EXPECT_EQ(bytes_read, rec.size());

        EXPECT_EQ(t2.visit_count, 2);
        ASSERT_EQ(t2.entries.size(), 2);

        EXPECT_EQ(t2.entries[0].val, node_int_2);
        EXPECT_EQ(t2.entries[1].val, node_int_1);
    }

    {
        UnpackSnapshotTestData t3;
        size_t bytes_read = 0;

        cali_find_all_in_snapshot(rec.data(), val_int_attr.id(), &bytes_read, ::test_entry_proc_op, &t3);

        EXPECT_EQ(bytes_read, rec.size());

        EXPECT_EQ(t3.visit_count, 2);
        ASSERT_EQ(t3.entries.size(), 2);

        EXPECT_EQ(t3.entries[0].val, val_int_1);
        EXPECT_EQ(t3.entries[1].val, val_int_2);
    }

    {
        // empty

        UnpackSnapshotTestData t4;
        size_t bytes_read = 0;

        cali_find_all_in_snapshot(rec.data(), CALI_INV_ID, &bytes_read, ::test_entry_proc_op, &t4);

        EXPECT_EQ(bytes_read, rec.size());

        EXPECT_EQ(t4.visit_count, 0);
        EXPECT_EQ(t4.entries.size(), 0);
    }


    {
        // quit after 1st

        UnpackSnapshotTestData t5;
        t5.max_visit_count = 1;
        size_t bytes_read = 0;

        cali_find_all_in_snapshot(rec.data(), node_int_attr.id(), &bytes_read, ::test_entry_proc_op, &t5);

        EXPECT_EQ(bytes_read, rec.size());

        EXPECT_EQ(t5.visit_count, 1);
        ASSERT_EQ(t5.entries.size(), 1);

        EXPECT_EQ(t5.entries[0].val, node_int_2);
    }
}
