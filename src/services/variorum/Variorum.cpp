// Copyright (c) 2021 Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.
//
// SPDX-License-Identifier: BSD-3-Clause

// This is a variorum caliper service based on
// the caliper measurement service template file.

#include "caliper/CaliperService.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/common/RuntimeConfig.h"
#include "caliper/common/Log.h"

extern "C" {
#include <variorum.h>
#include <jansson.h>
}

#include <mutex>
#include <tuple>
#include <vector>
#include <iostream>

using namespace cali;

namespace cali
{

extern cali::Attribute class_aggregatable_attr;

}

namespace
{

// Power measurement function
std::tuple<bool, uint64_t> measure(const std::string& name)
{
    double power_watts;
    json_t *power_obj = json_object();

    int ret = variorum_get_node_power_json(power_obj);
    if (ret != 0)
    {
        std::cout << "Variorum JSON API failed" << std::endl;
        uint64_t val;
        return std::make_tuple(false, val);
    }

    // TODO: Add error if name is an invalid JSON field
    // TODO: Assume 1 rank/node for aggregation

    power_watts = json_real_value(json_object_get(power_obj, name.c_str()));

    uint64_t val = (uint64_t)power_watts;

    return std::make_tuple(true, val);
}

// The VariorumService class reads a list of domains from the
// CALI_VARIORUM_DOMAINS config variable. For each domain, it appends a
// "measurement.val.<name>" entry (absolute value for a performance
// measurement) to Caliper snapshot records.
class VariorumService
{
    std::vector<Attribute> attrs;

    // Configuration variables for the variorum service
    static const ConfigSet::Entry s_configdata[];

    struct MeasurementInfo
    {
        std::string domain;     // Measurement name / ID
        Attribute   value_attr; // Attribute for the measurement value
        Attribute   delta_attr; // Attribute for the delta value (difference
                                // since last snapshot)
        Attribute   prval_attr; // A hidden attribute to store the previous
                                // measurement value on the Caliper blackboard
    };

    // Data for the configured measurement variables
    std::vector<MeasurementInfo> m_info;

    // Number of measurement errors encountered at runtime
    unsigned m_num_errors;

    void snapshot_cb(Caliper* c,
                     Channel* /*channel*/,
                     int /*scopes*/,
                     const SnapshotRecord* /*trigger_info*/,
                     SnapshotRecord* rec)
    {
        // The snapshot callback triggers performance measurements.
        // Measurement services should make measurements and add them to the
        // provided SnapshotRecord parameter, e.g. using rec->append().

        // This callback can be invoked on any thread, and inside signal
        // handlers. Make sure it is threadsafe. If needed, use
        // c->is_signal() to determine if you are running inside a signal
        // handler.

        // Make measurements for all configured variables
        for (const MeasurementInfo& m : m_info)
        {
            bool success;
            uint64_t val;

            std::tie(success, val) = measure(m.domain);
            // Check for measurement errors. Best practice is to count and
            // report them at the end rather than printing error messages at
            // runtime.
            if (!success)
            {
                ++m_num_errors;
                continue;
            }

            // Append measurement value to the snapshot record
            Variant v_val(cali_make_variant_from_uint(val));

            rec->append(m.value_attr, val);

            // We store the previous measurement value on the Caliper thread
            // blackboard so we can compute the difference since the last
            // snapshot. Here, c->exchange() stores the current and returns
            // the previous value. Compute the difference and append it.
            // TODO: For aggregation, we use average power instead of
            // difference.
            Variant v_prev = c->exchange(m.prval_attr, v_val);
            rec->append(m.delta_attr, cali_make_variant_from_uint((val + v_prev.to_uint())/2));
        }
    }

    void post_init_cb(Caliper* /*c*/, Channel* /*channel*/)
    {
        // This callback is invoked when the channel is fully initialized
        // and ready to make measurements. This is a good place to initialize
        // measurement values, if needed.
    }

    void finish_cb(Caliper* c, Channel* channel)
    {
        // This callback is invoked when the channel is being destroyed.
        // This is a good place to shut down underlying measurement libraries
        // (but keep in mind multiple channels may be active), report errors,
        // and print any debug output. Do NOT use Caliper API calls here, as
        // the services they rely on may already be destroyed.

        if (m_num_errors > 0)
        {
            Log(0).stream() << channel->name() << ": variorum: "
                            << m_num_errors << " measurement errors!"
                            << std::endl;
        }
    }

    MeasurementInfo create_measurement_info(Caliper* c, Channel* channel, const std::string& domain)
    {
        MeasurementInfo m;
        m.domain = domain;

        Variant v_true(true);

        // Create Caliper attributes for measurement variables, one for the
        // absolute value and one for the difference since the last snapshot.
        // Do this during service registration. Attributes are the keys for
        // Caliper's key:value snapshot records.

        // Attributes have a name, datatype, flags, and optional metadata.
        // As a convention, we prefix attributes names for services with
        // "<service name>." Datatype here is unsigned int. Use the ASVALUE
        // flag to store entries directly in snapshot records (as opposed to
        // the Caliper context tree). Use SKIP_EVENTS to avoid triggering
        // events when using set/begin/end on this attribute. This attribute
        // is for absolute measurement values for <name>.
        auto domainList =
            channel->config().init("variorum", s_configdata).get("domains").to_stringlist(",");

        for (auto &domain : domainList)
        {
            m.value_attr =
                c->create_attribute(std::string("variorum.val.") + domain,
                                    CALI_TYPE_UINT,
                                    CALI_ATTR_SCOPE_THREAD |
                                    CALI_ATTR_ASVALUE      |
                                    CALI_ATTR_SKIP_EVENTS,
                                    1, &class_aggregatable_attr, &v_true);

            // The delta attribute stores the difference of the measurement
            // value since the last snapshot. We add the "class.aggregatable"
            // metadata attribute here, which lets Caliper aggregate these values
            // automatically.
            m.delta_attr =
                c->create_attribute(std::string("variorum.") + domain,
                            CALI_TYPE_UINT,
                            CALI_ATTR_SCOPE_THREAD |
                            CALI_ATTR_ASVALUE      |
                            CALI_ATTR_SKIP_EVENTS,
                            1, &class_aggregatable_attr, &v_true);

            // We use a hidden attribute to store the previous measurement
            // for <name> on Caliper's per-thread blackboard. This is a
            // channel-specific attribute, so we encode the channel ID in the
            // name.
            //
            // In case more thread-specific information must be stored, it is
            // better to combine them in a structure and create a CALI_TYPE_PTR
            // attribute for this thread info in the service instance.
            m.prval_attr =
                c->create_attribute(std::string("variorum.pv.") + std::to_string(channel->id()) + domain,
                            CALI_TYPE_UINT,
                            CALI_ATTR_SCOPE_THREAD |
                            CALI_ATTR_ASVALUE      |
                            CALI_ATTR_HIDDEN       |
                            CALI_ATTR_SKIP_EVENTS);
            }

        return m;
    }

    VariorumService(Caliper* c, Channel* channel)
        : m_num_errors(0)
    {
        // Get the service configuration. This reads the configuration
        // variables defined in s_configdata from the environment, config
        // file, or channel setting. We create a "variorum"
        // config set, so the configuration variables for our service are
        // prefixed with "CALI_MEASUREMENT_TEMPLATE_". For example, set
        // "CALI_MEASUREMENT_TEMPLATE_NAMES=a,b" to set "names" to "a,b".
        ConfigSet config = channel->config().init("variorum", s_configdata);

        // Read the "domains" variable and treat it as a string list
        // (comma-separated list). Returns a std::vector<std::string>.
        auto domainList = config.get("domains").to_stringlist(",");

        if (domainList.empty())
        {
            Log(1).stream() << channel->name()
                            << ": variorum: No domains specified, dropping variorum service"
                            << std::endl;
            return;
        }

        // Create a MeasurementInfo entry for each of the "measurement
        // variables" in the configuration.
        for (const std::string& domain : domainList)
        {
            m_info.push_back( create_measurement_info(c, channel, domain) );
        }
    }

public:

    // This is the entry function to initialize the service, specified
    // in the CaliperService structure below. It is invoked when a Caliper
    // channel using this service is created. A Caliper channel maintains
    // a measurement configuration and the associated callbacks and data.
    //
    // Generally, a service can be enabled in multiple channels
    // simultaneously. Each channel can have a different configuration, so
    // each channel should use its own service instance. Implementors must
    // take appropriate actions if an underlying API does not allow multiple
    // clients or configurations, e.g. multiplexing access or erroring out
    // when an instance already exists.
    //
    // This is the place to read in the service configuration, create
    // any necessary objects like Caliper attributes, and register callback
    // functions.

    static void register_variorum(Caliper* c, Channel* channel)
    {
        auto domainList =
            channel->config().init("variorum", s_configdata).get("domains").to_stringlist(",");

        if (domainList.empty())
        {
            Log(1).stream() << channel->name()
                            << ": variorum: No domains specified, dropping variorum service"
                            << std::endl;
            return;
        }

        // Create a new service instance for this channel
        VariorumService* instance = new VariorumService(c, channel);

        // Register callback functions using lambdas
        channel->events().post_init_evt.connect(
            [instance](Caliper* c, Channel* channel){
                instance->post_init_cb(c, channel);
            });
        channel->events().snapshot.connect(
            [instance](Caliper* c, Channel* channel, int scopes, const SnapshotRecord* trigger_info, SnapshotRecord* rec){
                instance->snapshot_cb(c, channel, scopes, trigger_info, rec);
            });
        channel->events().finish_evt.connect(
            [instance](Caliper* c, Channel* channel){
                // This callback is invoked when the channel is destroyed.
                // No other callback will be invoked afterwards.
                // Delete the channel's service instance here!
                instance->finish_cb(c, channel);
                delete instance;
            });

        Log(1).stream() << channel->name() << ": Registered variorum service"
                        << std::endl;
    }
};

const ConfigSet::Entry VariorumService::s_configdata[] = {
    {
      "domains",                           // config variable name
      CALI_TYPE_STRING,                    // datatype
      "",                                  // default value
      "List of domains to record", // short description
      // long description
      "List of domains to record (separated by ',')\n"
      "Example: power_node_watts, power_socket_watts, power_gpu_watts, power_mem_watts"
    },
    ConfigSet::Terminator
};

} // namespace

namespace cali
{

CaliperService variorum_service = {
    "variorum", ::VariorumService::register_variorum
};

} // namespace cali
