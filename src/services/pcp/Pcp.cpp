// Copyright (c) 2015-2020, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// Pcp.cpp
// Access Performance co-pilot monitoring

#include "caliper/CaliperService.h"

#include "../../caliper/machine.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/common/RuntimeConfig.h"
#include "caliper/common/Log.h"

#include <pcp/pmapi.h>
#include <sys/time.h>

using namespace cali;

namespace cali
{

extern cali::Attribute class_aggregatable_attr;

}

namespace
{

class PcpService {
    static int s_pcp_context;
    static int s_num_instances;

    static const ConfigSet::Entry s_configdata[];

    struct MetricInfo {
        std::string name;
        Attribute   attr;
        pmDesc      pmdesc;
    };

    std::vector<MetricInfo> m_metric_info;
    std::vector<pmID>       m_metric_list;
    std::vector<double>     m_prev_value;  // last snapshot's value to compute diffs

    double m_prev_timestamp { 0 };

    unsigned m_num_lookups        { 0 };
    unsigned m_num_failed_lookups { 0 };
    unsigned m_num_failed_values  { 0 };

    Attribute m_timestamp_sec_attr;
    Attribute m_timestamp_attr;
    Attribute m_time_duration_attr;

    void snapshot(Caliper*, SnapshotRecord* rec) {
        pmResult* res = nullptr;

        int status = pmFetch(static_cast<int>(m_metric_list.size()), m_metric_list.data(), &res);

        if (status < 0) {
            ++m_num_failed_lookups;
            return;
        }

        for (int i = 0; i < res->numpmid; ++i) {
            if (res->vset[i]->numval < 1) {
                ++m_num_failed_values;
                continue;
            }

            double total = 0;
            int    nvals = 0;

            for (int j = 0; j < res->vset[i]->numval; ++j) {
                pmAtomValue val;
                status = pmExtractValue(res->vset[i]->valfmt,
                                        &res->vset[i]->vlist[j],
                                        m_metric_info[i].pmdesc.type,
                                        &val,
                                        PM_TYPE_DOUBLE);
                if (status < 0)
                    continue;
                total += val.d;
                ++nvals;
            }

            if (rec && nvals > 0) {
                double val = total;

                if (m_metric_info[i].pmdesc.sem == PM_SEM_COUNTER)
                    val = total - m_prev_value[i];

                rec->append(m_metric_info[i].attr.id(), Variant(val));
            }
            
            m_prev_value[i] = total;
        }

        double timestamp = res->timestamp.tv_sec + (res->timestamp.tv_usec * 1e-6);

        if (rec) {
            rec->append(m_timestamp_sec_attr.id(), cali_make_variant_from_uint(res->timestamp.tv_sec));
            rec->append(m_timestamp_attr.id(),     Variant(timestamp));
            rec->append(m_time_duration_attr.id(), Variant(timestamp - m_prev_timestamp));
        }

        m_prev_timestamp = timestamp;

        ++m_num_lookups;

        pmFreeResult(res);
    }

    bool setup_metrics(Caliper* c, const std::vector<std::string>& names) {
        std::vector<pmID> list;
        std::vector<MetricInfo> info;

        for (const std::string& name : names) {
            pmID pmid = PM_ID_NULL;
            char* namep = const_cast<char*>(name.c_str());
            int status = pmLookupName(1, &namep, &pmid);

            if (status != 1) {
                Log(0).stream() << "pcp: pmLookup: "
                                << pmErrStr(status)
                                << std::endl;
                return false;
            }

            pmDesc pmdesc;

            status = pmLookupDesc(pmid, &pmdesc);

            if (status != 0) {
                Log(0).stream() << "pcp: pmLookupDesc failed" << std::endl;
                return false;
            }

            if (Log::verbosity() >= 2) {
                Log(2).stream() << "pcp: Adding "
                                << name
                                << " (pmid=" << pmid
                                << ", type=" << pmdesc.type
                                << ", sem="  << pmdesc.sem
                                << ")" << std::endl;
            }

            // TODO: Do some sanity checking

            Variant v_true(true);
            Attribute attr =
                c->create_attribute(std::string("pcp.")+name, CALI_TYPE_DOUBLE,
                                    CALI_ATTR_SKIP_EVENTS |
                                    CALI_ATTR_ASVALUE,
                                    1, &class_aggregatable_attr, &v_true);

            list.push_back(pmid);
            info.push_back( { name, attr, pmdesc} );
        }

        m_metric_list = std::move(list);
        m_metric_info = std::move(info);
        m_prev_value.assign(m_metric_list.size(), 0.0);

        return true;
    }

    void finish(Caliper*, Channel* channel) {
        Log(1).stream() << channel->name() << ": pcp: "
                        << m_num_lookups << " lookups, "
                        << m_num_failed_lookups << " failed." << std::endl;
    }

    PcpService(Caliper* c, Channel* channel)
    {
        Attribute unit_attr =
            c->create_attribute("time.unit", CALI_TYPE_STRING, CALI_ATTR_SKIP_EVENTS);
        Attribute aggr_class_attr =
            c->get_attribute("class.aggregatable");

        Variant   sec_val   = Variant("sec");
        Variant   true_val  = Variant(true);

        Attribute meta_attr[2] = { aggr_class_attr, unit_attr };
        Variant   meta_vals[2] = { true_val,        sec_val   };

        m_timestamp_sec_attr =
            c->create_attribute("pcp.timestamp.sec", CALI_TYPE_UINT,
                                CALI_ATTR_ASVALUE       |
                                CALI_ATTR_SCOPE_PROCESS |
                                CALI_ATTR_SKIP_EVENTS,
                                1, &unit_attr, &sec_val);
        m_timestamp_attr =
            c->create_attribute("pcp.timestamp", CALI_TYPE_DOUBLE,
                                CALI_ATTR_ASVALUE       |
                                CALI_ATTR_SCOPE_PROCESS |
                                CALI_ATTR_SKIP_EVENTS,
                                1, &unit_attr, &sec_val);
        m_time_duration_attr =
            c->create_attribute("pcp.time.duration", CALI_TYPE_DOUBLE,
                                CALI_ATTR_ASVALUE       |
                                CALI_ATTR_SCOPE_PROCESS |
                                CALI_ATTR_SKIP_EVENTS,
                                2, meta_attr, meta_vals);
    }

    static bool init_pcp_context(const char* hostname) {
        if (s_pcp_context < 0)
            s_pcp_context = pmNewContext(PM_CONTEXT_HOST, hostname);

        Log(2).stream() << "pcp: Using context: " << s_pcp_context;

        return !(s_pcp_context < 0);
    }

    static void finish_pcp_context() {
        if (--s_num_instances == 0)
            pmDestroyContext(s_pcp_context);
    }

public:

    static void register_pcp(Caliper* c, Channel* channel) {
        auto metriclist =
            channel->config().init("pcp", s_configdata).get("metrics").to_stringlist(",");

        if (metriclist.empty()) {
            Log(1).stream() << channel->name()
                            << ": pcp: No metrics specified"
                            << std::endl;
            return;
        }

        int node_rank = machine::get_rank_for(machine::MachineLevel::Node);

        if (node_rank < 0)
            Log(0).stream() << channel->name()
                            << ": pcp: Unable to determine node master"
                            << std::endl;
        if (node_rank != 0)
            return;

        if (!init_pcp_context("local:")) {
            Log(0).stream() << channel->name()
                            << ": pcp: Context not initialized"
                            << std::endl;
            return;
        }

        PcpService* instance = new PcpService(c, channel);
        ++s_num_instances;

        if (!instance->setup_metrics(c, metriclist)) {
            Log(0).stream() << channel->name()
                            << ": pcp: Failed to initialize metrics"
                            << std::endl;

            finish_pcp_context();
            delete instance;
            return;
        }

        channel->events().snapshot.connect(
            [instance](Caliper* c, Channel*, int, const SnapshotRecord*, SnapshotRecord* rec){
                instance->snapshot(c, rec);
            });
        channel->events().post_init_evt.connect(
            [instance](Caliper* c, Channel*){
                // fetch current values to initialize m_prev_value
                instance->snapshot(c, nullptr);
            });
        channel->events().finish_evt.connect(
            [instance](Caliper* c, Channel* channel){
                instance->finish(c, channel);
                finish_pcp_context();
                delete instance;
            });

        Log(1).stream() << channel->name() << ": Registered pcp service" << std::endl;
    }
};

int PcpService::s_num_instances =  0;
int PcpService::s_pcp_context   = -1;

const ConfigSet::Entry PcpService::s_configdata[] = {
    { "metrics", CALI_TYPE_STRING, "",
      "List of performance co-pilot metrics to record",
      "List of performance co-pilot metrics to record, separated by ','"
    },
    ConfigSet::Terminator
};

}

namespace cali
{

CaliperService pcp_service = { "pcp", ::PcpService::register_pcp };

}
