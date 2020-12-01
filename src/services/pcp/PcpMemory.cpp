// Copyright (c) 2015-2020, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// PcpMemoryBandwidth.cpp
// Compute memory bandwidth metrics from CAS counters

#include "caliper/CaliperService.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/common/RuntimeConfig.h"
#include "caliper/common/Log.h"

#include "../Services.h"

#include <algorithm>

using namespace cali;

namespace
{

const char* rd_cas_metrics =
    "perfevent.hwcounters.bdx_unc_imc0__UNC_M_CAS_COUNT_RD.value"
    ",perfevent.hwcounters.bdx_unc_imc1__UNC_M_CAS_COUNT_RD.value"
    ",perfevent.hwcounters.bdx_unc_imc4__UNC_M_CAS_COUNT_RD.value"
    ",perfevent.hwcounters.bdx_unc_imc5__UNC_M_CAS_COUNT_RD.value";

const char* wr_cas_metrics =
    "perfevent.hwcounters.bdx_unc_imc0__UNC_M_CAS_COUNT_WR.value"
    ",perfevent.hwcounters.bdx_unc_imc1__UNC_M_CAS_COUNT_WR.value"
    ",perfevent.hwcounters.bdx_unc_imc4__UNC_M_CAS_COUNT_WR.value"
    ",perfevent.hwcounters.bdx_unc_imc5__UNC_M_CAS_COUNT_WR.value";

std::vector<Attribute>
find_counter_attributes(const CaliperMetadataAccessInterface& db, const char* metrics) {
    std::vector<Attribute> res;

    auto counters = StringConverter(metrics).to_stringlist();

    for (const auto &s : counters) {
        Attribute attr = db.get_attribute(std::string("sum#pcp.")+s);

        if (attr == Attribute::invalid)
            attr = db.get_attribute(std::string("pcp.")+s);
        if (attr == Attribute::invalid)
            continue;

        res.push_back(attr);
    }

    return res;
}

std::pair<double, int>
sum_attributes(const std::vector<Entry>& rec, const std::vector<Attribute>& attributes)
{
    double sum = 0.0;
    int count = 0;

    for (const Attribute& a : attributes) {
        auto it = std::find_if(rec.begin(), rec.end(), [a](const Entry& e) {
                            return e.attribute() == a.id();
                        });

        if (it != rec.end()) {
            ++count;
            sum += it->value().to_double();
        }
    }

    return std::make_pair(sum, count);
}


class PcpMemory
{
    std::vector<Attribute> rd_counter_attrs;
    std::vector<Attribute> wr_counter_attrs;

    Attribute rd_result_attr;
    Attribute wr_result_attr;

    unsigned  num_computed;
    unsigned  num_flushes;

    void postprocess_snapshot_cb(std::vector<Entry>& rec) {
        auto rp = sum_attributes(rec, rd_counter_attrs);
        auto wp = sum_attributes(rec, wr_counter_attrs);

        if (rp.second > 0)
            rec.push_back(Entry(rd_result_attr, Variant(64.0 * rp.first)));
        if (wp.second > 0)
            rec.push_back(Entry(wr_result_attr, Variant(64.0 * wp.first)));

        if (wp.second + rp.second > 0)
            ++num_computed;
    }

    void pre_flush_cb(Caliper* c, Channel* channel) {
        ++num_flushes;

        if (rd_counter_attrs.size() + wr_counter_attrs.size() > 0)
            return;

        rd_counter_attrs = ::find_counter_attributes(*c, rd_cas_metrics);
        wr_counter_attrs = ::find_counter_attributes(*c, wr_cas_metrics);

        if (rd_counter_attrs.size() + wr_counter_attrs.size() > 0)
            channel->events().postprocess_snapshot.connect(
                [this](Caliper*, Channel*, std::vector<Entry>& rec){
                    this->postprocess_snapshot_cb(rec);
                });
    }

    void finish_cb(Caliper* c, Channel* channel) {
        if (num_flushes > 0) {
            if (rd_counter_attrs.empty())
                Log(1).stream() << channel->name()
                                << ": pcp.memory: read metrics not found"
                                << std::endl;
            if (wr_counter_attrs.empty())
                Log(1).stream() << channel->name()
                                << ": pcp.memory: write metrics not found"
                                << std::endl;
        }

        Log(1).stream() << channel->name()
                        << ": pcp.memory: Computed memory metrics for "
                        << num_computed << " records"
                        << std::endl;
    }

    PcpMemory(Caliper* c, Channel*)
        : num_computed(0), num_flushes(0)
        {
            Attribute aggr_attr = c->get_attribute("class.aggregatable");
            Variant v_true(true);

            rd_result_attr =
                c->create_attribute("mem.bytes.read", CALI_TYPE_DOUBLE,
                                    CALI_ATTR_ASVALUE |
                                    CALI_ATTR_SKIP_EVENTS,
                                    1, &aggr_attr, &v_true);
            wr_result_attr =
                c->create_attribute("mem.bytes.written", CALI_TYPE_DOUBLE,
                                    CALI_ATTR_ASVALUE |
                                    CALI_ATTR_SKIP_EVENTS,
                                    1, &aggr_attr, &v_true);
        }

public:

    static void pcp_memory_register(Caliper* c, Channel* channel) {
        std::string metrics = rd_cas_metrics;
        metrics.append(",");
        metrics.append(wr_cas_metrics);

        channel->config().set("CALI_PCP_METRICS", metrics);

        if (!cali::services::register_service(c, channel, "pcp")) {
            Log(0).stream() << channel->name()
                            << ": pcp.memory: Unable to register pcp service, skipping pcp.memory"
                            << std::endl;
            return;
        }

        PcpMemory* instance = new PcpMemory(c, channel);

        channel->events().pre_flush_evt.connect(
            [instance](Caliper* c, Channel* channel, const SnapshotRecord*){
                instance->pre_flush_cb(c, channel);
            });
        channel->events().finish_evt.connect(
            [instance](Caliper* c, Channel* channel){
                instance->finish_cb(c, channel);
                delete instance;
            });

        Log(1).stream() << channel->name() << ": Registered pcp.memory service" << std::endl;
    }

};

}

namespace cali
{

CaliperService pcp_memory_service { "pcp.memory", ::PcpMemory::pcp_memory_register };

}
