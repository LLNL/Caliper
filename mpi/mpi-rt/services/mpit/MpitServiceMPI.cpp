// LLNL-CODE-678900
// All rights reserved.
//
// For details, see https://github.com/scalability-llnl/Caliper.
// Please also see the LICENSE file for our additional BSD notice.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
//  * Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the disclaimer below.
//  * Redistributions in binary form must reproduce the above copyright notice, this list of
//    conditions and the disclaimer (as noted below) in the documentation and/or other materials
//    provided with the distribution.
//  * Neither the name of the LLNS/LLNL nor the names of its contributors may be used to endorse
//    or promote products derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS
// OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// LAWRENCE LIVERMORE NATIONAL SECURITY, LLC, THE U.S. DEPARTMENT OF ENERGY OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
// ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

// MpitService.cpp
// Caliper MPIT service
// This is the MPI part of the MPI-T service, which will be part of libcaliper-mpiwrap.

#include "MpiEvents.h"

#include "caliper/CaliperService.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/common/Log.h"
#include "caliper/common/RuntimeConfig.h"

#include "caliper/common/util/split.hpp"

#include <mpi.h>

#include <cstring>
#include <iterator>
#include <string>
#include <vector>

using namespace cali;
using namespace std;

#define NAME_LEN 1024
#define SOME_BIG_ENOUGH_VALUE 1024
#define MAX_COUNT 10

namespace cali
{
    bool mpit_enabled;
    // vector<cali_id_t> mpit_pvar_attr;
    // vector<cali_id_t> watermark_changed_attr;
    // vector<cali_id_t> watermark_change_attr;
}

namespace
{
    struct PvarInfo {
        std::vector<MPI_T_pvar_handle> handles;
        std::vector<int>               counts;
        int          index;
        int          continuous;
        int          readonly;
        MPI_Datatype datatype;
        int          pvarclass;
        int          bind;
        int          verbosity;
        std::string  name;
        std::string  desc;
        int          atomic;
        bool         aggregatable;
        Attribute    attr;
    };

    std::vector<std::string> pvar_selection;

    std::vector<PvarInfo>    pvars;
    MPI_T_pvar_session       pvar_session;

    unsigned                 num_pvars_read = 0;
    unsigned                 num_pvars_read_error = 0;

    //Arrays storing last values of PVARs. This is a hack. Only in place because current MPI implementations do not
    //support resetting of PVARs. So we store last value of pvars and subtract it
    // vector<array <unsigned long long int, MAX_COUNT> > last_value_unsigned_long;
    // vector<array <double, MAX_COUNT> > last_value_double;

    ConfigSet        config;

    ConfigSet::Entry configdata[] = {
        { "pvars", CALI_TYPE_STRING, "", 
          "List of comma-separated PVARs to read",
          "List of comma-separated PVARs to read. Default: all" 
        },
    	ConfigSet::Terminator
    };

    int num_pvars = 0;

    void snapshot_cb(Caliper* c, int scope, const SnapshotRecord*, SnapshotRecord* snapshot) {
        for (const PvarInfo& pvi : pvars) {
            if (pvi.attr == Attribute::invalid)
                continue;

            if (pvi.handles.size() > 0 && pvi.counts.size() > 0 && pvi.counts[0] == 1) {
                // read only PVAR's with count one for now
                unsigned char buf[64];
                int ret = MPI_T_pvar_read(pvar_session, pvi.handles[0], buf);

                if (ret == MPI_SUCCESS) {
                    snapshot->append(pvi.attr.id(), Variant(pvi.attr.type(), buf, 8));
                    ++num_pvars_read;
                } else {
                    ++num_pvars_read_error;
                }
            } else {
                ++num_pvars_read_error;
            }
        }
        
        /*
        int size, watermark_changed;
        unsigned long long int temp_unsigned, temp_unsigned_array[MAX_COUNT] = {0};
        double temp_double, temp_double_array[MAX_COUNT] = {0.0};
		

        Log(3).stream() << "Collecting PVARs for the MPI-T interface." << endl;

        for(int index=0; index < num_pvars; index++) {
            for(int subindex=0; subindex < pvar_handle[index].size() - 1; subindex++) {
                MPI_T_pvar_read(pvar_session, pvar_handle[index][subindex], buffer);
			
                if((pvar_type[index] == MPI_COUNT) || (pvar_type[index] == MPI_UNSIGNED) || (pvar_type[index] == MPI_UNSIGNED_LONG) || (pvar_type[index] == MPI_UNSIGNED_LONG_LONG))
                {
                    switch(pvar_class[index]) {
                    case MPI_T_PVAR_CLASS_TIMER:
                    case MPI_T_PVAR_CLASS_COUNTER:
                    case MPI_T_PVAR_CLASS_AGGREGATE: {
                        //These are free-counting (monotonically increasing) counters and timers. Keep track of the last value read, and find the difference to get the change in the 
                        //value of the PVAR
                        for(int j=0; j < pvar_count[index][subindex]; j++) {
                            temp_unsigned = ((unsigned long long int *)buffer)[j];
                            ((unsigned long long int *)buffer)[j] -= last_value_unsigned_long[index][j];
                            last_value_unsigned_long[index][j] = temp_unsigned;
                        }
                        break;
                    }
                    case MPI_T_PVAR_CLASS_HIGHWATERMARK: {
                        watermark_changed = 0;
                        //Calculate the "derived" value for watermarks - whether this function changed the watermark, and if so, by how much"
                        for(int j=0; j < pvar_count[index][subindex]; j++) {
                            temp_unsigned_array[j] = ((unsigned long long int *)buffer)[j];
                            if(temp_unsigned_array[j] > last_value_unsigned_long[index][j]) {
                                watermark_changed = 1;
                                temp_unsigned_array[j] -= last_value_unsigned_long[index][j];
                            } else {
                                temp_unsigned_array[j] = 0;
                            }
                            last_value_unsigned_long[index][j] = ((unsigned long long int *)buffer)[j];
                        }
                        if(watermark_changed) {
                            snapshot->append(watermark_changed_attr[index], Variant(CALI_TYPE_UINT, &watermark_changed, 1));	
                            snapshot->append(watermark_change_attr[index], Variant(CALI_TYPE_UINT, (void *)temp_unsigned_array, pvar_count[index][subindex]));
                        }
                        break;
                    }
                    case MPI_T_PVAR_CLASS_LOWWATERMARK: {
                        watermark_changed = 0;
                        //Calculate the "derived" value for watermarks - whether this function changed the watermark, and if so, by how much"
                        for(int j=0; j < pvar_count[index][subindex]; j++) {
                            temp_unsigned_array[j] = ((unsigned long long int *)buffer)[j];
                            if(temp_unsigned_array[j] < last_value_unsigned_long[index][j]) {
                                watermark_changed = 1;
                                temp_unsigned_array[j] = last_value_unsigned_long[index][j] - temp_unsigned_array[j];
                            } else {
                                temp_unsigned_array[j] = 0;
                            }
                            last_value_unsigned_long[index][j] = ((unsigned long long int *)buffer)[j];
                        }
                        if(watermark_changed) {
                            snapshot->append(watermark_changed_attr[index], Variant(CALI_TYPE_UINT, &watermark_changed, 1));	
                            snapshot->append(watermark_change_attr[index], Variant(CALI_TYPE_UINT, (void *)temp_unsigned_array, pvar_count[index][subindex]));
                        }
                        break;
                    }

                    default: {
                        //do nothing for any other class of PVARs.
                        break;
                    }
                    }

                    snapshot->append(mpit_pvar_attr[index], Variant(CALI_TYPE_UINT, buffer, pvar_count[index][subindex]));
					
                    Log(3).stream() << "Index and Value: " << index << " " << ((unsigned long long int *)buffer)[0] << endl;
                }
                else if((pvar_type[index] == MPI_INT))
                {
                    snapshot->append(mpit_pvar_attr[index], Variant(CALI_TYPE_INT, buffer, pvar_count[index][subindex]));
					
                    Log(3).stream() << "Index and Value: " <<  index << " " << ((int *)buffer)[0] << endl;
                }
                else if((pvar_type[index] == MPI_CHAR))
                {
                    snapshot->append(mpit_pvar_attr[index], Variant(CALI_TYPE_STRING, buffer, pvar_count[index][subindex]));
					
                    Log(3).stream() << "Index and Value: " <<  index << "  " << ((char *)buffer)[0] << endl;
                }
                else if((pvar_type[index] == MPI_DOUBLE))
                {
                    switch(pvar_class[index]) {
                    case MPI_T_PVAR_CLASS_TIMER:
                    case MPI_T_PVAR_CLASS_COUNTER:
                    case MPI_T_PVAR_CLASS_AGGREGATE: {
                        //These are free-counting (monotonically increasing) counters and timers. Keep track of the last value read, and find the difference to get the change in the 
                        //value of the PVAR
                        for(int j=0; j < pvar_count[index][subindex]; j++) {
                            temp_double = ((double *)buffer)[j];
                            ((double *)buffer)[j] -= last_value_double[index][j];
                            last_value_double[index][j] = temp_double;
                        }
                        break;
                    }
                    case MPI_T_PVAR_CLASS_HIGHWATERMARK: {
                        watermark_changed = 0;
                        //Calculate the "derived" value for watermarks - whether this function changed the watermark, and if so, by how much"
                        for(int j=0; j < pvar_count[index][subindex]; j++) {
                            temp_double_array[j] = ((double *)buffer)[j];
                            if(temp_double_array[j] > last_value_double[index][j]) {
                                watermark_changed = 1;
                                temp_double_array[j] -= last_value_double[index][j];
                            } else {
                                temp_double_array[j] = 0;
                            }
                            last_value_double[index][j] = ((double *)buffer)[j];
                        }
                        if(watermark_changed) {
                            snapshot->append(watermark_changed_attr[index], Variant(CALI_TYPE_UINT, &watermark_changed, 1));	
                            snapshot->append(watermark_change_attr[index], Variant(CALI_TYPE_UINT, (void *)temp_double_array, pvar_count[index][subindex]));
                        }
                        break;
                    }
                    case MPI_T_PVAR_CLASS_LOWWATERMARK: {
                        watermark_changed = 0;
                        //Calculate the "derived" value for watermarks - whether this function changed the watermark, and if so, by how much"
                        for(int j=0; j < pvar_count[index][subindex]; j++) {
                            temp_double_array[j] = ((double *)buffer)[j];
                            if(temp_double_array[j] < last_value_double[index][j]) {
                                watermark_changed = 1;
                                temp_double_array[j] = last_value_double[index][j] - temp_double_array[j];
                            } else {
                                temp_double_array[j] = 0;
                            }
                            last_value_double[index][j] = ((double *)buffer)[j];
                        }
                        if(watermark_changed) {
                            snapshot->append(watermark_changed_attr[index], Variant(CALI_TYPE_UINT, &watermark_changed, 1));	
                            snapshot->append(watermark_change_attr[index], Variant(CALI_TYPE_UINT, (void *)temp_double_array, pvar_count[index][subindex]));
                        }
                        break;
                    }
						
                    default: {
                        //do nothing for any other class of PVARs.
                        break;
                    }
                    }

                    snapshot->append(mpit_pvar_attr[index], Variant(CALI_TYPE_DOUBLE, buffer, pvar_count[index][subindex]));
					
                    Log(3).stream() << "Index and Value: " << index << " " << ((double *)buffer)[0] << endl;
                }
            }
        }
        */
    }

    void create_attribute_for_pvar(Caliper *c, PvarInfo& pvi) {
        Attribute aggr_class_attr = c->get_attribute("class.aggregatable");
        Variant   v_true(true);

        cali_attr_type type = CALI_TYPE_INV;

        const struct calimpi_type_info_t {
            MPI_Datatype   mpitype;
            cali_attr_type calitype;
        } calimpi_types[] = {
            { MPI_COUNT,              CALI_TYPE_UINT   },
            { MPI_UNSIGNED,           CALI_TYPE_UINT   },
            { MPI_UNSIGNED_LONG,      CALI_TYPE_UINT   },
            { MPI_UNSIGNED_LONG_LONG, CALI_TYPE_UINT   },
            { MPI_INT,                CALI_TYPE_INT    },
            { MPI_DOUBLE,             CALI_TYPE_DOUBLE },
            { MPI_CHAR,               CALI_TYPE_STRING }
        };

        for (calimpi_type_info_t i : calimpi_types)
            if (pvi.datatype == i.mpitype) {
                type = i.calitype;
                break;
            }
            
        if (type != CALI_TYPE_INV) {
            pvi.attr = 
                c->create_attribute(string("mpit.")+pvi.name, CALI_TYPE_UINT,
                                    CALI_ATTR_ASVALUE       | 
                                    CALI_ATTR_SCOPE_PROCESS | 
                                    CALI_ATTR_SKIP_EVENTS,
                                    (pvi.aggregatable ? 1 : 0), &aggr_class_attr, &v_true);
        } else {
            Log(1).stream() << "mpit: Cannot create attribute for PVAR with index " << pvi.index
                            << " (" << pvi.name << "): unsupported MPI datatype." << std::endl;
        }
    }

    bool is_pvar_class_aggregatable(int index, int pvarclass) {
        // General idea to determine aggretablitity of a PVAR: 
        // Any PVAR that represents an internal MPI state is by default not aggregatable
        // A PVAR is aggregatable if it "makes sense" to apply one or more of these operators to it: 
        //   COUNT, SUM, MIN, MAX, AVG. In the future, more operators may be considered.

        const struct aggregateable_t {
            int pvarclass; bool state;
        } aggregateable_table[] = {
            { MPI_T_PVAR_CLASS_STATE,         false },
            { MPI_T_PVAR_CLASS_LEVEL,         true  },
            { MPI_T_PVAR_CLASS_SIZE,          false },
            { MPI_T_PVAR_CLASS_PERCENTAGE,    true  },
            { MPI_T_PVAR_CLASS_HIGHWATERMARK, false },
            { MPI_T_PVAR_CLASS_LOWWATERMARK,  false },
            { MPI_T_PVAR_CLASS_COUNTER,       true  },
            { MPI_T_PVAR_CLASS_AGGREGATE,     true  },
            { MPI_T_PVAR_CLASS_TIMER,         true  },
            { MPI_T_PVAR_CLASS_GENERIC,       false }
        };

        for (const aggregateable_t &atp : aggregateable_table)
            if (atp.pvarclass == pvarclass)
                return atp.state;

        return false;

        /*
        switch(pvi.pvarclass) {
        Attribute attr;
        Attribute aggr_class_attr = c->get_attribute("class.aggregatable");
        Variant   v_true(true);

        case MPI_T_PVAR_CLASS_HIGHWATERMARK: {
            Log(2).stream() << "PVAR at index: " << index << " with name: " << pvar_name << " has a class: MPI_T_PVAR_CLASS_HIGHWATERMARK" << endl;
            attr = c->create_attribute(string("mpit.")+pvar_name+string(".number_highwatermark_changes"), CALI_TYPE_UINT,
                                       CALI_ATTR_ASVALUE      | 
                                       CALI_ATTR_SCOPE_PROCESS | 
                                       CALI_ATTR_SKIP_EVENTS, 1, &aggr_class_attr, &v_true);
				
            watermark_changed_attr[index] = attr.id();
            attr = c->create_attribute(string("mpit.")+pvar_name+string(".total_highwatermark_change"), CALI_TYPE_UINT,
                                       CALI_ATTR_ASVALUE      | 
                                       CALI_ATTR_SCOPE_PROCESS | 
                                       CALI_ATTR_SKIP_EVENTS, 1, &aggr_class_attr, &v_true);
            watermark_change_attr[index] = attr.id();
				
            return false; // Doesn't make sense to aggregate watermarks in accordance with the definition above. We need to create a "derived" attribute for this in order to perform aggregation.
            break;
        }
        case MPI_T_PVAR_CLASS_LOWWATERMARK: {
            Log(2).stream() << "PVAR at index: " << index << " with name: " << pvar_name << " has a class: MPI_T_PVAR_CLASS_LOWWATERMARK" << endl;
            attr = c->create_attribute(pvar_name+string(".number_lowwatermark_changes"), CALI_TYPE_UINT,
                                       CALI_ATTR_ASVALUE      | 
                                       CALI_ATTR_SCOPE_PROCESS | 
                                       CALI_ATTR_SKIP_EVENTS, 1, &aggr_class_attr, &v_true);
            watermark_changed_attr[index] = attr.id();
				
            attr = c->create_attribute(pvar_name+string(".total_lowwatermark_change"), CALI_TYPE_UINT,
                                       CALI_ATTR_ASVALUE      | 
                                       CALI_ATTR_SCOPE_PROCESS | 
                                       CALI_ATTR_SKIP_EVENTS, 1, &aggr_class_attr, &v_true);

            watermark_change_attr[index] = attr.id();
            return false; // Doesn't make sense to aggregate watermarks in accordance with the definition above. We need to create a "derived" attribute for this in order to perform aggregation.
            break;
        }
        }
        */
    }			


    // Allocate PVAR handles when the PVAR is bound to something other than MPI_T_BIND_NO_OBJECT
    void do_mpit_allocate_bound_pvar_handles(Caliper *c, void *in_handle, int bind) {
        for (PvarInfo& pvi : pvars) {
            if (bind == pvi.bind) {
                MPI_T_pvar_handle handle;
                int count;

                int ret = MPI_T_pvar_handle_alloc(pvar_session, pvi.index, in_handle, &handle, &count);

                if (ret != MPI_SUCCESS) {
                    Log(0).stream() << "mpit: MPI_T_pvar_handle_alloc error for PVAR at index " << pvi.index
                                    << " (" << pvi.name << ")" << std::endl;
                    continue;
                }

                pvi.handles.push_back(handle);
                pvi.counts.push_back(count);

                if (pvi.continuous) {
                    ret = MPI_T_pvar_start(pvar_session, pvi.handles[pvi.handles.size()-1]);

                    if (ret != MPI_SUCCESS) {
                        Log(0).stream() << "mpit: MPI_T_pvar_start error for PVAR at index " << pvi.index
                                        << " (" << pvi.name << ")" << std::endl;
                    }
                }
            }

            if (pvi.attr == Attribute::invalid) {
                create_attribute_for_pvar(c, pvi);
            }
        }
    }

    /*Allocate handles for pvars and create attributes*/
    void do_mpit_allocate_pvar_handles(Caliper *c) {
        int current_num_pvars = 0;

        {
            int ret = MPI_T_pvar_get_num(&current_num_pvars);

            if (ret != MPI_SUCCESS) {
                perror("MPI_T_pvar_get_num ERROR:");
                Log(0).stream() << "MPI_T_pvar_get_num ERROR." << std::endl;
                return;
            }
        }

        // watermark_changed_attr.resize(current_num_pvars);
        // watermark_change_attr.resize(current_num_pvars);
        // last_value_unsigned_long.resize(current_num_pvars, {0});
        // last_value_double.resize(current_num_pvars, {0.0});

        Log(1).stream() << "mpit: Exporting " << current_num_pvars << " PVARs." << std::endl;

        for (int index = num_pvars; index < current_num_pvars; index++) {
            PvarInfo   pvi;

            char       namebuf[NAME_LEN] = { 0 };
            char       descbuf[NAME_LEN] = { 0 };
            int        namelen = NAME_LEN;
            int        desclen = NAME_LEN;

            MPI_T_enum enumtype;

            int iret = MPI_T_pvar_get_info(index, 
                                           namebuf, &namelen, 
                                           &pvi.verbosity, 
                                           &pvi.pvarclass, 
                                           &pvi.datatype, 
                                           &enumtype, 
                                           descbuf, &desclen, 
                                           &pvi.bind, 
                                           &pvi.readonly, 
                                           &pvi.continuous,
                                           &pvi.atomic);

            pvi.index = index;

            if (namelen > 0)
                pvi.name.assign(namebuf, namelen-1);
            if (desclen > 0)
                pvi.desc.assign(descbuf, desclen-1);

            // see if this pvar is in the selection list
            if (!pvar_selection.empty()) {
                auto it = std::find(pvar_selection.begin(), pvar_selection.end(), pvi.name);

                if (it == pvar_selection.end()) {
                    continue;
                }
            }

            pvi.aggregatable = is_pvar_class_aggregatable(index, pvi.pvarclass);

            // allocate a pvar handle that will be used later

            int ret = -1;

            switch (pvi.bind) {
            case MPI_T_BIND_NO_OBJECT:
            {
                MPI_T_pvar_handle handle;
                int count;

                ret = MPI_T_pvar_handle_alloc(pvar_session, index, nullptr, &handle, &count);

                pvi.handles.push_back(handle);
                pvi.counts.push_back(count);
                break;
            }
            case MPI_T_BIND_MPI_COMM:
                // Handle this through a Wrapper call to MPI_Comm_create(). 
                // Support MPI_COMM_WORLD, MPI_COMM_SELF as default.
            {
                MPI_T_pvar_handle handle;
                int count;

                MPI_Comm comm = MPI_COMM_WORLD;
                ret = MPI_T_pvar_handle_alloc(pvar_session, index, &comm, &handle, &count);

                if (ret == MPI_SUCCESS) {
                    pvi.handles.push_back(handle);
                    pvi.counts.push_back(count);
                } else {
                    break;
                }

                comm = MPI_COMM_SELF;
                ret = MPI_T_pvar_handle_alloc(pvar_session, index, &comm, &handle, &count);

                if (ret == MPI_SUCCESS) {
                    pvi.handles.push_back(handle);
                    pvi.counts.push_back(count);
                }

                break;
            }
            default:
                break;
                // Do nothing here
            }

            if (ret != MPI_SUCCESS) {
                Log(0).stream() << "mpit: MPI_T_pvar_handle_alloc ERROR: " << ret 
                                << " for PVAR at index " << index << " with name " << pvi.name << std::endl;
                return;
            }

            if (pvi.counts.size() > 0 && pvi.counts[0] > 1) {
                Log(1).stream() << "mpit: PVAR at index " << pvi.index 
                                << " (" << pvi.name << ") has count > 1 (count = " << pvi.counts[0] 
                                << "), skipping." << std::endl;

                continue;
            }

            // Non-continuous variables need to be started before being read.

            if (pvi.continuous == 0) {
                Log(1).stream() << "mpit: PVAR \'" << pvi.name << "\' at index " << index 
                                << "is non-continuous. Starting this PVAR."
                                << std::endl;

                ret = MPI_T_pvar_start(pvar_session, pvi.handles[0]);

                if (ret != MPI_SUCCESS) {
                    Log(0).stream() << "mpit: MPI_T_pvar_start ERROR:" << ret
                                    << " for PVAR at index " << index << " with name " << pvi.name << endl;
                    return;
                }
            }
			
            create_attribute_for_pvar(c, pvi);

            if (pvi.attr != Attribute::invalid) {
                Log(2).stream() << "mpit: Registered PVAR " << pvi.index 
                                << " (" << pvi.name << ")" << std::endl;

                pvars.push_back(pvi);
            }
        }
    }

    void finish_cb(Caliper*) {
        Log(1).stream() << "mpit: " << num_pvars_read << " PVARs read, " 
                        << num_pvars_read_error << " PVAR read errors." << std::endl;
    }

    // Register the service and initalize the MPI-T interface
    void do_mpit_init(Caliper *c)
    {	
        int thread_provided;

    	config = RuntimeConfig::init("mpit", configdata);

        util::split(config.get("pvars").to_string(), ',', std::back_inserter(pvar_selection));
    
        /* Initialize MPI_T */
        int return_val = MPI_T_init_thread(MPI_THREAD_SINGLE, &thread_provided);

        if (return_val != MPI_SUCCESS) {
            Log(0).stream() << "MPI_T_init_thread ERROR: " << return_val << ". MPIT service disabled." << endl;
            return;
        }

        /* Track a performance pvar session */
        return_val = MPI_T_pvar_session_create(&pvar_session);
        if (return_val != MPI_SUCCESS) {
            Log(0).stream() << "MPI_T_pvar_session_create ERROR: " << return_val << ". MPIT service disabled." << endl;
            return;
        }  
        		
        do_mpit_allocate_pvar_handles(c);

        c->events().snapshot.connect(&snapshot_cb);
        c->events().finish_evt.connect(&finish_cb);

    	Log(1).stream() << "mpit: MPI-T initialized." << endl;
    }

    /*Thin wrapper functions to invoke pvar allocation function from another module*/
    void mpit_allocate_pvar_handles() {
        Caliper c;
        ::do_mpit_allocate_pvar_handles(&c);
    }

    void mpit_allocate_bound_pvar_handles(void *handle, int bind) {
        Caliper c;
        ::do_mpit_allocate_bound_pvar_handles(&c, handle, bind);
    }

    void mpit_init(Caliper* c) {
        MpiEvents::events.mpi_init_evt.connect(::do_mpit_allocate_pvar_handles);

        ::do_mpit_init(c);
    }

} // anonymous namespace 

namespace cali 
{

CaliperService mpit_service = { "mpit", ::mpit_init };

} // namespace cali
