// test the postprocess_snapshot callback

#include "caliper/Caliper.h"

#include "caliper/common/RuntimeConfig.h"

#include <gtest/gtest.h>

using namespace cali;

namespace
{

void flush_cb(Caliper* c, Channel*, SnapshotView, SnapshotFlushFn flush_fn)
{
    Attribute snapshot_attr =
        c->create_attribute("tps.snapshot.val", CALI_TYPE_INT, CALI_ATTR_ASVALUE);

    std::vector<Entry> rec;
    rec.push_back(Entry(snapshot_attr, Variant(49)));

    flush_fn(*c, rec);
}

void postprocess_snapshot_cb(Caliper* c, Channel*, std::vector<Entry>& rec)
{
    Attribute val_attr  =
        c->create_attribute("tps.postprocess.val",  CALI_TYPE_INT, CALI_ATTR_ASVALUE);
    Attribute node_attr =
        c->create_attribute("tps.postprocess.node", CALI_TYPE_INT, CALI_ATTR_DEFAULT);

    rec.push_back(Entry(val_attr, Variant(42)));
    rec.push_back(Entry(c->make_tree_entry(node_attr, Variant(36))));
}

}

TEST(PostprocessSnapshotTest, PostprocessSnapshot)
{
    RuntimeConfig cfg;
    cfg.allow_read_env(false);

    Caliper c;
    Channel* channel = c.create_channel("test.postprocess_snapshot", cfg);

    channel->events().flush_evt.connect(::flush_cb);
    channel->events().postprocess_snapshot.connect(::postprocess_snapshot_cb);

    std::vector< std::vector<Entry> > output;

    c.flush(channel, SnapshotView(), [&output](CaliperMetadataAccessInterface& db, const std::vector<Entry>& rec){
            output.push_back(rec);
        });

    Attribute snapshot_val_attr = c.get_attribute("tps.snapshot.val");
    Attribute post_val_attr = c.get_attribute("tps.postprocess.val");
    Attribute post_node_attr = c.get_attribute("tps.postprocess.node");

    ASSERT_EQ(output.size(), 1);
    ASSERT_EQ(output.front().size(), 3);

    SnapshotView view(output.front().size(), output.front().data());

    EXPECT_EQ(view.get(snapshot_val_attr).value().to_int(), 49);
    EXPECT_EQ(view.get(post_val_attr).value().to_int(), 42);
    EXPECT_EQ(view.get(post_node_attr).value().to_int(), 36);

    c.delete_channel(channel);
}
