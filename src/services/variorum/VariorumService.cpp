// Copyright (c) 2020, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.
//
// SPDX-License-Identifier: BSD-3-Clause

//   This is a variorum caliper service based on 
//   the caliper measurement service template file.
//  

#include "caliper/CaliperService.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/common/RuntimeConfig.h"
#include "caliper/common/Log.h"

#include <chrono>
#include <mutex>
#include <tuple>
#include <vector>
#include <iostream>

//Variourm include area 
extern "C" {
#include <variorum.h>
#include <jansson.h>
}


using namespace cali;

namespace cali
{

extern cali::Attribute class_aggregatable_attr;

}

namespace
{

typedef std::chrono::system_clock Clock;
typedef std::chrono::time_point<Clock> TimePoint;

// Our "measurement function"
std::tuple<bool, uint64_t> measure(const TimePoint& start, const std::string& name) {
    /*
    * lets try extracting variorum and returning it
    auto     now = Clock::now(); 
    uint64_t val =
        name.length() * std::chrono::duration_cast<std::chrono::microseconds>(now - start).count();
    */
    //std::cout << "Measuring with Variorum" << std::endl;
  
    double power_node;
    json_t *power_obj  = json_object(); 
    int ret = variorum_get_node_power_json(power_obj);
    if (ret != 0) {
        std::cout << "Variorum JSON API failed" << std::endl;
        uint64_t val;
        return std::make_tuple(false, val);
    }
    power_node =  json_real_value(json_object_get(power_obj, "power_node"));

    uint64_t val = (uint64_t)power_node;
    return std::make_tuple(true, val);
}

//   The VariorumService class serves as template/demo/documentation
// for writing Caliper measurement services.
//   It reads a list of names from the CALI_MEASUREMENT_TEMPLATE_NAMES config
// variable. For each name, it appends "measurement.val.<name>" and
// "measurement.<name>" entries (absolute value and delta-since-last-snapshot
// for a performance measurement) to Caliper snapshot records.
class VariorumService
{
    static const ConfigSet::Entry s_configdata[]; // Configuration variables for this service

    struct MeasurementInfo {
        std::string name;       // Measurement name / ID
        Attribute   value_attr; // Attribute for the measurement value
        Attribute   delta_attr; // Attribute for the delta value (difference since last snapshot)
        Attribute   prval_attr; // A hidden attribute to store the previous measurement value on the Caliper blackboard
    };

    std::vector<MeasurementInfo>  m_info;       // Data for the configured measurement variables

    unsigned                      m_num_errors; // Number of measurement errors encountered at runtime
    TimePoint                     m_starttime;  // Initial value for our measurement function

    void snapshot_cb(Caliper* c, Channel* /*channel*/, int /*scopes*/, const SnapshotRecord* /*trigger_info*/, SnapshotRecord* rec) {
        //   The snapshot callback triggers performance measurements.
        // Measurement services should make measurements and add them to the
        // provided SnapshotRecord parameter, e.g. using rec->append().

        //   This callback can be invoked on any thread, and inside signal
        // handlers. Make sure it is threadsafe. If needed, use
        // c->is_signal() to determine if you are running inside a signal
        // handler.

        // Make measurements for all configured variables
        for (const MeasurementInfo& m : m_info) {
            bool success;
            uint64_t val;
		
        
            
            std::tie(success, val) = measure(m_starttime, m.name);
            std::string outstring = "Node Power "+ std::to_string(val);
            std::cout << outstring << std::endl;
            //   Check for measurement errors. Best practice is to count and
            // report them at the end rather than printing error messages at
            // runtime.
            if (!success) {
                ++m_num_errors;
                continue;
            }

            // Append measurement value to the snapshot record
            Variant v_val(cali_make_variant_from_uint(val));
            
            rec->append(m.value_attr, val);

            //   We store the previous measurement value on the Caliper thread
            // blackboard so we can compute the difference since the last
            // snapshot. Here, c->exchange() stores the current and returns
            // the previous value. Compute the difference and append it.
            Variant v_prev = c->exchange(m.prval_attr, v_val);
            rec->append(m.delta_attr, cali_make_variant_from_uint(val - v_prev.to_uint()));
            outstring = "Node Power prev "+ std::to_string(v_prev);
            std::cout << outstring << std::endl;
            
        }
    }

    void post_init_cb(Caliper* /*c*/, Channel* /*channel*/) {
        //   This callback is invoked when the channel is fully initialized
        // and ready to make measurements. This is a good place to initialize
        // measurement values, if needed.

        m_starttime = Clock::now();
    }

    void finish_cb(Caliper* c, Channel* channel) {
        //   This callback is invoked when the channel is being destroyed.
        // This is a good place to shut down underlying measurement libraries
        // (but keep in mind multiple channels may be active), report errors,
        // and print any debug output. Do NOT use Caliper API calls here, as
        // the services they rely on may already be destroyed.

        if (m_num_errors > 0)
            Log(0).stream() << channel->name() << ": measurement: "
                            << m_num_errors << " measurement errors!"
                            << std::endl;
    }

    MeasurementInfo create_measurement_info(Caliper* c, Channel* channel, const std::string& name) {

        /********
         * Delete Comment Later
         * This is how the measurment info class that was confusing above, comes into play 
         *
         *******/
        MeasurementInfo m;

        m.name = name;

        //   Create Caliper attributes for measurement variables, one for the
        // absolute value and one for the difference since the last snapshot.
        // Do this during service registration. Attributes are the keys for
        // Caliper's key:value snapshot records.

        //   Attributes have a name, datatype, flags, and optional metadata.
        // As a convention, we prefix attributes names for services with
        // "<service name>." Datatype here is unsigned int. Use the ASVALUE
        // flag to store entries directly in snapshot records (as opposed to
        // the Caliper context tree). Use SKIP_EVENTS to avoid triggering
        // events when using set/begin/end on this attribute. This attribute
        // is for absolute measurement values for <name>.
        m.value_attr =
            c->create_attribute(std::string("variorum.val.") + name,
                                CALI_TYPE_UINT,
                                CALI_ATTR_SCOPE_THREAD |
                                CALI_ATTR_ASVALUE      |
                                CALI_ATTR_SKIP_EVENTS);

        Variant v_true(true);

        //   The delta attribute stores the difference of the measurement
        // value since the last snapshot. We add the "class.aggregatable"
        // metadata attribute here, which lets Caliper aggregate these values
        // automatically.
        m.delta_attr =
            c->create_attribute(std::string("variorum.") + name,
                        CALI_TYPE_UINT,
                        CALI_ATTR_SCOPE_THREAD |
                        CALI_ATTR_ASVALUE      |
                        CALI_ATTR_SKIP_EVENTS,
                        1, &class_aggregatable_attr, &v_true);

        //   We use a hidden attribute to store the previous measurement
        // for <name> on Caliper's per-thread blackboard. This is a
        // channel-specific attribute, so we encode the channel ID in the
        // name.
        //   In case more thread-specific information must be stored, it is
        // better to combine them in a structure and create a CALI_TYPE_PTR
        // attribute for this thread info in the service instance.
        m.prval_attr =
            c->create_attribute(std::string("variorum.pv.") + std::to_string(channel->id()) + name,
                        CALI_TYPE_UINT,
                        CALI_ATTR_SCOPE_THREAD |
                        CALI_ATTR_ASVALUE      |
                        CALI_ATTR_HIDDEN       |
                        CALI_ATTR_SKIP_EVENTS);

        return m;
    }

    VariorumService(Caliper* c, Channel* channel)
        : m_num_errors(0)
    {
        //   Get the service configuration. This reads the configuration
        // variables defined in s_configdata from the environment, config
        // file, or channel setting. We create a "variorum"
        // config set, so the configuration variables for our service are
        // prefixed with "CALI_MEASUREMENT_TEMPLATE_". For example, set
        // "CALI_MEASUREMENT_TEMPLATE_NAMES=a,b" to set "names" to "a,b".
        ConfigSet config = channel->config().init("variorum", s_configdata);

        //   Read the "names" variable and treat it as a string list
        // (comma-separated list). Returns a std::vector<std::string>.
        auto names = config.get("names").to_stringlist();

        //   Create a MeasurementInfo entry for each of the "measurement
        // variables" in the configuration.
        for (const std::string& name : names)
            m_info.push_back( create_measurement_info(c, channel, name) );
    }

public:

    //   This is the entry function to initialize the service, specified
    // in the CaliperService structure below. It is invoked when a Caliper
    // channel using this service is created. A Caliper channel maintains
    // a measurement configuration and the associated callbacks and data.
    //
    //   Generally, a service can be enabled in multiple channels
    // simultaneously. Each channel can have a different configuration, so
    // each channel should use its own service instance. Implementors must
    // take appropriate actions if an underlying API does not allow multiple
    // clients or configurations, e.g. multiplexing access or erroring out
    // when an instance already exists.
    //
    //   This is the place to read in the service configuration, create
    // any necessary objects like Caliper attributes, and register callback
    // functions.

    static void register_variorum_service(Caliper* c, Channel* channel) {
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
                //   This callback is invoked when the channel is destroyed.
                // No other callback will be invoked afterwards.
                // Delete the channel's service instance here!
                instance->finish_cb(c, channel);
                delete instance;
            });

        Log(1).stream() << channel->name() << ": Registered measurement template service"
                        << std::endl;
    }
};

const ConfigSet::Entry VariorumService::s_configdata[] = {
    { "names",          // config variable name
      CALI_TYPE_UINT, // datatype
      "0",            // default value
      // short description
      "Names of measurements to record",
      // long description
      "Node Power, Socket Power, GPU Power, All Power Measurements, Memory Power"
    },
    ConfigSet::Terminator
};

} // namespace [anonymous]

namespace cali
{

CaliperService variorum_service = {
    "variorum_service",
    ::VariorumService::register_variorum_service
};

} // namespace cali
