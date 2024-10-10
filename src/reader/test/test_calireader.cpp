#include "caliper/reader/CaliReader.h"
#include "caliper/reader/CaliperMetadataDB.h"
#include "caliper/common/Node.h"

#include <gtest/gtest.h>

#include <sstream>

using namespace cali;

const char* cali_txt = R"cali(
__rec=node,id=12,attr=10,data=64,parent=3
__rec=node,id=13,attr=8,data=attribute.alias,parent=12
__rec=node,id=14,attr=8,data=attribute.unit,parent=12
__rec=node,id=33,attr=14,data=sec,parent=5
__rec=node,id=34,attr=13,data=time,parent=33
__rec=node,id=35,attr=10,data=65,parent=34
__rec=node,id=36,attr=8,data=sum#sum#time.duration,parent=35
__rec=node,id=37,attr=13,data=Node order,parent=2
__rec=node,id=38,attr=10,data=65,parent=37
__rec=node,id=39,attr=8,data=min#aggregate.slot,parent=38
__rec=ctx,attr=36=39,data=0.000538=0
__rec=node,id=40,attr=10,data=276,parent=3
__rec=node,id=41,attr=8,data=region,parent=40
__rec=node,id=42,attr=41,data=main
__rec=ctx,ref=42,attr=36=39,data=0.000135=1
__rec=node,id=47,attr=41,data=init,parent=42
__rec=ctx,ref=47,attr=36=39,data=0.000015=2
__rec=node,id=48,attr=8,data=loop,parent=40
__rec=node,id=49,attr=48,data=mainloop,parent=42
__rec=ctx,ref=49,attr=36=39,data=0.000065=3
__rec=node,id=50,attr=41,data=foo,parent=49
__rec=ctx,ref=50,attr=36=39,data=0.000622=4
__rec=node,id=15,attr=10,data=1612,parent=3
__rec=node,id=16,attr=8,data=caliper.config,parent=15
__rec=node,id=17,attr=10,data=1612,parent=1
__rec=node,id=18,attr=8,data=iterations,parent=17
__rec=node,id=19,attr=8,data=cali.caliper.version,parent=15
__rec=node,id=20,attr=19,data=2.11.0-dev
__rec=node,id=21,attr=18,data=4,parent=20
__rec=node,id=22,attr=16,data=hatchet-region-profile,parent=21
__rec=node,id=51,attr=8,data=hatchet-region-profile:node.order,parent=15
__rec=node,id=52,attr=8,data=cali.channel,parent=15
__rec=node,id=53,attr=52,data=hatchet-region-profile
__rec=node,id=54,attr=51,data=true,parent=53
__rec=globals,ref=22=54
)cali";

TEST(CaliReader, BasicRead)
{
    CaliperMetadataDB db;
    CaliReader        reader;

    std::istringstream is(cali_txt);

    unsigned node_count = 0;
    unsigned rec_count  = 0;

    NodeProcessFn node_proc = [&node_count](CaliperMetadataAccessInterface&, const Node*) {
        ++node_count;
    };
    SnapshotProcessFn snap_proc = [&rec_count](CaliperMetadataAccessInterface&, const EntryList&) {
        ++rec_count;
    };

    reader.read(is, db, node_proc, snap_proc);

    EXPECT_FALSE(reader.error()) << reader.error_msg();

    EXPECT_EQ(node_count, 29);
    EXPECT_EQ(rec_count, 5);

    auto globals = db.get_globals();

    EXPECT_FALSE(globals.empty());
}