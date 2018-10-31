#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include <sys/types.h>
#include <inttypes.h>

#include <string>
#include <vector>
#include <map>

class TopdownObject {

    public:
        std::string arch;
        std::vector<std::string> event_list;

        TopdownObject(std::string &arch);
        void addDerivedMetricsToSnapshot(cali::Caliper *c, cali::SnapshotRecord *snapshot);

        void createTopdownMetricAttrMap(cali::Caliper *c);
        void createTopdownEventAttrMap(std::map<std::string, cali::Attribute> attrMap);

    private:
        std::map<std::string, cali::Attribute> topdownEventsAttrMap;
        std::map<std::string, cali::Attribute> topdownMetricsAttrMap;
        std::function<std::map<std::string, double>(cali::SnapshotRecord *)> derivationFunction;

        // TODO: map of int (or something easier ot index) to attribute, instead of string

        double getTopdownEventValue(cali::SnapshotRecord *snapshot, std::string name);
};

