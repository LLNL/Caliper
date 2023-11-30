// Copyright (c) 2021, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "caliper/CaliperService.h"

#include "../Services.h"

#include "caliper/Caliper.h"
#include "caliper/RegionProfile.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/common/Log.h"
#include "caliper/common/OutputStream.h"
#include "caliper/common/RuntimeConfig.h"

#include "../../common/util/format_util.h"

#include "ldms/ldms.h"
#include "ldms/ldmsd_stream.h"
#include "ovis_util/util.h"
#include "ldms/ldms_xprt.h"
#include "semaphore.h"

#include <chrono>
#include <iomanip>
#include <map>
#include <string>

#include <iostream>
#include <sstream>


#define SLURM_NOTIFY_TIMEOUT 5
ldms_t ldms_g;
pthread_mutex_t ln_lock;
int conn_status, to;
ldms_t ldms_cali;
sem_t conn_sem;
sem_t recv_sem;

using namespace cali;

namespace
{

static void event_cb(ldms_t x, ldms_xprt_event_t e, void *cb_arg)
{
        switch (e->type) {
        case LDMS_XPRT_EVENT_CONNECTED:
                sem_post(&conn_sem);
                conn_status = 0;
                break;
        case LDMS_XPRT_EVENT_REJECTED:
                ldms_xprt_put(x);
                conn_status = ECONNREFUSED;
                break;
        case LDMS_XPRT_EVENT_DISCONNECTED:
                ldms_xprt_put(x);
                conn_status = ENOTCONN;
                break;
        case LDMS_XPRT_EVENT_ERROR:
                conn_status = ECONNREFUSED;
                break;
        case LDMS_XPRT_EVENT_RECV:
                sem_post(&recv_sem);
                break;
        case LDMS_XPRT_EVENT_SEND_COMPLETE:
                break;
        default:
                printf("Received invalid event type %d\n", e->type);
        }
}

ldms_t setup_connection(const char *xprt, const char *host,
                        const char *port, const char *auth)
{
        char hostname[PATH_MAX];
        const char *timeout = "5";
        int rc;
        struct timespec ts;

        if (!host) {
                if (0 == gethostname(hostname, sizeof(hostname)))
                        host = hostname;
        }
        if (!timeout) {
                ts.tv_sec = time(NULL) + 5;
                ts.tv_nsec = 0;
        } else {
                int to = atoi(timeout);
                if (to <= 0)
                        to = 5;
                ts.tv_sec = time(NULL) + to;
                ts.tv_nsec = 0;
        }

        ldms_g = ldms_xprt_new_with_auth(xprt, auth, NULL);
        if (!ldms_g) {
                printf("Error %d creating the '%s' transport\n",
                       errno, xprt);
                return NULL;
        }

        sem_init(&recv_sem, 1, 0);
        sem_init(&conn_sem, 1, 0);

        rc = ldms_xprt_connect_by_name(ldms_g, host, port, event_cb, NULL);
        if (rc) {
                printf("Error %d connecting to %s:%s\n",
                       rc, host, port);
                return NULL;
        }
        sem_timedwait(&conn_sem, &ts);
        if (conn_status)
                return NULL;
        return ldms_g;
}

void caliper_ldms_connector_initialize()
{
    const char* env_ldms_stream           = getenv("CALIPER_LDMS_STREAM");
    const char* env_ldms_xprt             = getenv("CALIPER_LDMS_XPRT");
    const char* env_ldms_host             = getenv("CALIPER_LDMS_HOST");
    const char* env_ldms_port             = getenv("CALIPER_LDMS_PORT");
    const char* env_ldms_auth             = getenv("CALIPER_LDMS_AUTH");

    /* Check/set LDMS transport type */
    if (!env_ldms_xprt || !env_ldms_host || !env_ldms_port || !env_ldms_auth || !env_ldms_stream){
	Log(1).stream() << "Either the transport, host, port or authentication is not given\n";
        return;
    }

    pthread_mutex_lock(&ln_lock);
    ldms_cali = setup_connection(env_ldms_xprt, env_ldms_host, env_ldms_port, env_ldms_auth);
        if (conn_status != 0) {
            Log(0).stream() << "Error setting up connection to LDMS streams daemon:" << conn_status << " -- exiting\n";
            pthread_mutex_unlock(&ln_lock);
            return;
        }
        else if (ldms_cali->disconnected){
            Log(1).stream() << "Disconnected from LDMS streams daemon -- exiting\n";
            pthread_mutex_unlock(&ln_lock);
            return;
        }
    pthread_mutex_unlock(&ln_lock);
    return;
}

std::ostream& write_ldms_record(std::ostream& os, int mpi_rank, RegionProfile& profile)
{

    caliper_ldms_connector_initialize();	

    std::map<std::string, double> region_times;
    double total_time = 0;
    

    int buffer_size = 4096;
    char* buffer = (char*) malloc (sizeof(char) * buffer_size);
    
    const char*   env_ldms_jobid_str            = getenv("SLURM_JOB_ID");
    const char*   env_ldms_procid               = getenv("SLURM_PROCID");
    const char*   env_ldms_slurm_nodelist       = getenv("SLURM_JOB_NODELIST");
    const char*   env_ldms_caliper_verbose_str  = getenv("CALIPER_LDMS_VERBOSE");

    int env_ldms_jobid = env_ldms_jobid_str == NULL ? 0 : atoi( env_ldms_jobid_str );
    int env_ldms_caliper_verbose = env_ldms_caliper_verbose_str == NULL ? 0 : atoi( env_ldms_caliper_verbose_str );    

    std::tie(region_times, std::ignore, total_time) =
        profile.inclusive_path_profile();

    double unix_ts = 1e-6 * std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    for (const auto &p : region_times) {
        // ignore regions with < 5% of the epoch's total time
        if (p.second < 0.05 * total_time)
            continue;

        os << "{ \"timestamp\": " << std::fixed << unix_ts
           << ", \"duration\": "  << std::fixed << p.second;

        util::write_esc_string(os << ", \"path\": \"", p.first) << "\"";

	std::string path_msg = "" + p.first;
        std::string path = path_msg.c_str();

        if (mpi_rank >= 0) {
            os << ", \"rank\": " << mpi_rank;
	    snprintf(buffer, buffer_size, "{ \"timestamp\": %f, \"jobid\" : %d, \"rank\" : %d, \"procid\" : %d, \"nodelist\" : %s, \"caliper-perf-data\", \"duration\": %f, \"path\": \"%s\"} \n", unix_ts, env_ldms_jobid, mpi_rank, env_ldms_procid, env_ldms_slurm_nodelist, p.second, path); 
	} else {
	    snprintf(buffer, buffer_size, "{ \"timestamp\": %f, \"jobid\" : %d, \"rank\": %d, \"procid\" : %d, \"nodelist\" : %s, \"caliper-perf-data\", \"duration\": %f, \"path\": \"%s\"} \n", unix_ts, env_ldms_jobid, 0, env_ldms_procid, env_ldms_slurm_nodelist, p.second, path);	
	}
	os << "} \n";
	
	if (env_ldms_caliper_verbose > 0)
		puts(buffer);

	int rc = ldmsd_stream_publish( ldms_cali, "caliper-perf-data", LDMSD_STREAM_JSON, buffer, strlen(buffer) + 1);
	if (rc)
		 Log(0).stream() << "Error " << rc << " publishing data.\n";

	else if (env_ldms_caliper_verbose > 0)
		 Log(2).stream() << "Caliper Message published successfully\n";
	}

	return os;
}

class LdmsForwarder
{
    RegionProfile profile;
    OutputStream  stream;

    std::string   filename;

    void snapshot(Caliper* c, Channel*) {
        Entry e = c->get(c->get_attribute("mpi.rank"));
        int rank = e.empty() ? -1 : e.value().to_int();

        write_ldms_record(*stream.stream(), rank, profile) << "\n";

        profile.clear(); // reset profile - skip to create a cumulative profile
    }

    void post_init(Caliper* c, Channel* channel) {
        std::vector<Entry> rec;
        stream.set_filename(filename.c_str(), *c, rec);
        profile.start();
    }

    LdmsForwarder(const char* fname)
        : filename { fname }
    { }

public:
    
    static const char* s_spec;

    static void create(Caliper* c, Channel* channel) {
        ConfigSet cfg = 
            services::init_config_from_spec(channel->config(), s_spec);

        LdmsForwarder* instance = new LdmsForwarder(cfg.get("filename").to_string().c_str());

        channel->events().post_init_evt.connect(
            [instance](Caliper* c, Channel* channel){
                instance->post_init(c, channel);
            });
        channel->events().snapshot.connect(
            [instance](Caliper* c, Channel* channel, int scope, SnapshotView, SnapshotBuilder&){
                instance->snapshot(c, channel);
            });
        channel->events().finish_evt.connect(
            [instance](Caliper* c, Channel* chn){
                delete instance;
            });

        Log(1).stream() << channel->name() << "Initialized LDMS forwarder\n";
    }

	
};

const char* LdmsForwarder::s_spec = R"json(
{   
 "name"        : "ldms",
 "description" : "Forward Caliper regions to LDMS (prototype)",
 "config"      :
 [
  {
    "name"        : "filename",
    "description" : "Output file name, or stdout/stderr",
    "type"        : "string",
    "value"       : "stdout"
  }
 ]
}
)json";

} // namespace [anonymous]

namespace cali
{

CaliperService ldms_service { ::LdmsForwarder::s_spec, ::LdmsForwarder::create };

}
