#include "topdown.h"

#include <cmath>
#include <string>

using namespace cali;
using namespace std;

TopdownObject::TopdownObject(std::string &arch) {

    // Initialize event list and derivation metric functions for architecture
    if (arch.compare("sandybridge") == 0) {

    } else if (arch.compare("ivybridge") == 0) {

        event_list = {
            "BR_MISP_RETIRED.ALL_BRANCHES",
            "CPU_CLK_UNHALTED.THREAD_P",
            "CYCLE_ACTIVITY.CYCLES_NO_EXECUTE",
            "CYCLE_ACTIVITY.STALLS_L1D_PENDING",
            "CYCLE_ACTIVITY.STALLS_L2_PENDING",
            "CYCLE_ACTIVITY.STALLS_LDM_PENDING",
            "IDQ.MS_UOPS",
            "IDQ_UOPS_NOT_DELIVERED.CORE",
            "INT_MISC.RECOVERY_CYCLES",
            "MACHINE_CLEARS.COUNT",
            "MEM_LOAD_UOPS_RETIRED.L3_HIT",
            "MEM_LOAD_UOPS_RETIRED.L3_MISS",
            "RESOURCE_STALLS.SB",
            "RS_EVENTS.EMPTY_CYCLES",
            "UOPS_EXECUTED.THREAD",
            "UOPS_EXECUTED.CORE_CYCLES_GE_1",
            "UOPS_EXECUTED.CORE_CYCLES_GE_2",
            "UOPS_ISSUED.ANY",
            "UOPS_RETIRED.RETIRE_SLOTS"
        };

        derivationFunction= {
            [=](SnapshotRecord *snapshot) { 
                std::map<std::string, double> ret;

                double clocks =  
                    getTopdownEventValue(snapshot, "CPU_CLK_UNHALTED.THREAD_P");
                double slots = 
                    4.0 * clocks;
                double retire_slots =  
                    getTopdownEventValue(snapshot, "UOPS_RETIRED.RETIRE_SLOTS");
                ret["retiring"] = 
                    retire_slots / slots;
                ret["bad_speculation"] = 
                    ((getTopdownEventValue(snapshot, "UOPS_ISSUED.ANY")
                            - getTopdownEventValue(snapshot, "UOPS_RETIRED.RETIRE_SLOTS")
                            + 4.0 * getTopdownEventValue(snapshot, "INT_MISC.RECOVERY_CYCLES"))
                        / slots);
                ret["frontend_bound"] = 
                    getTopdownEventValue(snapshot, "IDQ_UOPS_NOT_DELIVERED.CORE") / slots;
                ret["backend_bound"] = 
                    (1.0 - (ret["frontend_bound"]
                            + ret["bad_speculation"]
                            + ret["retiring"]));
                ret["branch_mispredict"] = 
                    getTopdownEventValue(snapshot, "BR_MISP_RETIRED.ALL_BRANCHES") 
                    / (getTopdownEventValue(snapshot, "BR_MISP_RETIRED.ALL_BRANCHES")
                            + getTopdownEventValue(snapshot, "MACHINE_CLEARS.COUNT"));
                ret["machine_clear"] = 1.0 - ret["branch_mispredict"];
                ret["frontend_latency"] =
                    std::max(getTopdownEventValue(snapshot, "IDQ_UOPS_NOT_DELIVERED.CORE"), 4.0)
                        / clocks;
                ret["frontend_bandwidth"] = (1.0 - ret["frontend_latency"]);
                ret["memory_bound"] = 
                    getTopdownEventValue(snapshot, "CYCLE_ACTIVITY.STALLS_LDM_PENDING")
                    / clocks;
                double be_bound_at_exe = 
                    ((getTopdownEventValue(snapshot, "CYCLE_ACTIVITY.CYCLES_NO_EXECUTE")
                      + getTopdownEventValue(snapshot, "UOPS_EXECUTED.CORE_CYCLES_GE_1")
                      - getTopdownEventValue(snapshot, "UOPS_EXECUTED.CORE_CYCLES_GE_2"))
                     / clocks);
                ret["core_bound"] = 
                    be_bound_at_exe - ret["memory_bound"];
                double l3_hit_fraction = 
                    getTopdownEventValue(snapshot, "MEM_LOAD_UOPS_RETIRED.L3_HIT") /
                    (getTopdownEventValue(snapshot, "MEM_LOAD_UOPS_RETIRED.L3_HIT")
                     + 7*getTopdownEventValue(snapshot, "MEM_LOAD_UOPS_RETIRED.L3_MISS"));
                double l3_miss_fraction = 
                    (7*getTopdownEventValue(snapshot, "MEM_LOAD_UOPS_RETIRED.L3_MISS")
                     / (getTopdownEventValue(snapshot, "MEM_LOAD_UOPS_RETIRED.L3_HIT")
                         + 7*getTopdownEventValue(snapshot, "MEM_LOAD_UOPS_RETIRED.L3_MISS")));
                ret["mem_bound"] = 
                    getTopdownEventValue(snapshot, "CYCLE_ACTIVITY.STALLS_L2_PENDING")
                        * l3_miss_fraction
                        / clocks;
                ret["l1_bound"] =
                    ((getTopdownEventValue(snapshot, "CYCLE_ACTIVITY.STALLS_LDM_PENDING")
                      - getTopdownEventValue(snapshot, "CYCLE_ACTIVITY.STALLS_L1D_PENDING"))
                     / clocks);
                ret["l2_bound"] =
                    ((getTopdownEventValue(snapshot, "CYCLE_ACTIVITY.STALLS_L1D_PENDING")
                      - getTopdownEventValue(snapshot, "CYCLE_ACTIVITY.STALLS_L2_PENDING"))
                     / clocks);
                ret["l3_bound"] = 
                    getTopdownEventValue(snapshot, "CYCLE_ACTIVITY.STALLS_L2_PENDING")
                        * l3_hit_fraction
                        / clocks;
                ret["uncore_bound"] = 
                    getTopdownEventValue(snapshot, "CYCLE_ACTIVITY.STALLS_L2_PENDING")
                        / clocks;

                return ret;
            }
        };
    } else if (arch.compare("broadwell") == 0) {

    } else if (arch.compare("haswell") == 0) {

    } else {

    }

}

void 
TopdownObject::createTopdownEventAttrMap(std::map<std::string, cali::Attribute> attrMap) {
    for (auto const& attrPair : attrMap) {
        for (auto event : event_list) {
            // If event is a substring of this attribute
            std::string attrName = attrPair.first;
            if (attrName.find(event) != std::string::npos) {
                // If event is not assigned, or if event is already assigned but current attribute is longer (e.g. sum) then reassign
                std::map<std::string, Attribute>::iterator assignedEventName = topdownEventsAttrMap.find(event);
                if (assignedEventName == topdownEventsAttrMap.end() || assignedEventName->first.length() < attrName.length()) { // TODO make this look for sum specifically
                    topdownEventsAttrMap[event] = attrPair.second;
                } 
            } 
        }
    }
}

void 
TopdownObject::createTopdownMetricAttrMap(cali::Caliper *c) {
    topdownMetricsAttrMap["retiring"] = c->create_attribute("libpfm.topdown#retiring", CALI_TYPE_DOUBLE, CALI_ATTR_ASVALUE);
    topdownMetricsAttrMap["bad_speculation"] = c->create_attribute("libpfm.topdown#bad_speculation", CALI_TYPE_DOUBLE, CALI_ATTR_ASVALUE);
    topdownMetricsAttrMap["frontend_bound"] = c->create_attribute("libpfm.topdown#frontend_bound", CALI_TYPE_DOUBLE, CALI_ATTR_ASVALUE);
    topdownMetricsAttrMap["backend_bound"] = c->create_attribute("libpfm.topdown#backend_bound", CALI_TYPE_DOUBLE, CALI_ATTR_ASVALUE);
    topdownMetricsAttrMap["branch_mispredict"] = c->create_attribute("libpfm.topdown#branch_mispredict", CALI_TYPE_DOUBLE, CALI_ATTR_ASVALUE);
    topdownMetricsAttrMap["machine_clear"] = c->create_attribute("libpfm.topdown#machine_clear", CALI_TYPE_DOUBLE, CALI_ATTR_ASVALUE);
    topdownMetricsAttrMap["frontend_latency"] = c->create_attribute("libpfm.topdown#frontend_latency", CALI_TYPE_DOUBLE, CALI_ATTR_ASVALUE);
    topdownMetricsAttrMap["frontend_bandwidth"] = c->create_attribute("libpfm.topdown#frontend_bandwidth", CALI_TYPE_DOUBLE, CALI_ATTR_ASVALUE);
    topdownMetricsAttrMap["memory_bound"] = c->create_attribute("libpfm.topdown#memory_bound", CALI_TYPE_DOUBLE, CALI_ATTR_ASVALUE);
    topdownMetricsAttrMap["core_bound"] = c->create_attribute("libpfm.topdown#core_bound", CALI_TYPE_DOUBLE, CALI_ATTR_ASVALUE);
    topdownMetricsAttrMap["mem_bound"] = c->create_attribute("libpfm.topdown#mem_bound", CALI_TYPE_DOUBLE, CALI_ATTR_ASVALUE);
    topdownMetricsAttrMap["l1_bound"] = c->create_attribute("libpfm.topdown#l1_bound", CALI_TYPE_DOUBLE, CALI_ATTR_ASVALUE);
    topdownMetricsAttrMap["l2_bound"] = c->create_attribute("libpfm.topdown#l2_bound", CALI_TYPE_DOUBLE, CALI_ATTR_ASVALUE);
    topdownMetricsAttrMap["l3_bound"] = c->create_attribute("libpfm.topdown#l3_bound", CALI_TYPE_DOUBLE, CALI_ATTR_ASVALUE);
    topdownMetricsAttrMap["uncore_bound"] = c->create_attribute("libpfm.topdown#uncore_bound", CALI_TYPE_DOUBLE, CALI_ATTR_ASVALUE);
}

double 
TopdownObject::getTopdownEventValue(cali::SnapshotRecord *snapshot, std::string name) {
    return snapshot->get(topdownEventsAttrMap[name]).value().to_double();
}

void
TopdownObject::addDerivedMetricsToSnapshot(cali::Caliper *c, cali::SnapshotRecord *snapshot) {

    std::map<string, double> derivedMetrics = derivationFunction(snapshot);

    std::vector<cali::Attribute> attr;
    std::vector<cali::Variant>   data;

    for (auto it : derivedMetrics) {
        if (topdownMetricsAttrMap.find(it.first) == topdownMetricsAttrMap.end()) {
            std::cerr << "cant find attribute " << it.first << std::endl;
        } else {
            attr.push_back(topdownMetricsAttrMap[it.first]);

            double val = it.second;
            if (std::isnan(it.second))
                val = 0.0;
            else if (std::isinf(it.second))
                val = 1.0;
            data.push_back(val);
        }
    }

    if (attr.size() > 0)
        c->make_entrylist(attr.size(), attr.data(), data.data(), *snapshot);
}

