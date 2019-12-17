// --- Caliper continuous integration test app: postprocess_snapshot

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include <pthread.h>

using namespace cali;


namespace 
{

void postprocess_snapshot_cb(Caliper* c, Channel*, std::vector<Entry>& rec)
{    
    Attribute val_attr  = 
        c->create_attribute("postprocess.val",  CALI_TYPE_INT, CALI_ATTR_ASVALUE);
    Attribute node_attr =
        c->create_attribute("postprocess.node", CALI_TYPE_INT, CALI_ATTR_DEFAULT);

    rec.push_back(Entry(val_attr, Variant(42)));
    rec.push_back(Entry(c->make_tree_entry(node_attr, Variant(36))));
}

}

int main() 
{
    Caliper  c;
    Channel* chn = c.get_channel(0);

    chn->events().postprocess_snapshot.connect(::postprocess_snapshot_cb);
        
    cali_id_t snapshot_attr_id = 
        c.create_attribute("snapshot.val", CALI_TYPE_INT, CALI_ATTR_ASVALUE).id();
    Variant   snapshot_val(49);

    SnapshotRecord trigger_info(1, &snapshot_attr_id, &snapshot_val);
    c.push_snapshot(chn, &trigger_info);
}
