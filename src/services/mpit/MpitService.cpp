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
/// @file MpitService.cpp
/// @brief Caliper MPIT service

#include "../CaliperService.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/common/Log.h"
#include "caliper/common/RuntimeConfig.h"

#include <mpi.h>

#include <vector>

using namespace cali;
using namespace std;

#define NAME_LEN 1024
#define SOME_BIG_ENOUGH_VALUE 1024
#define MAX_COUNT 10

namespace
{
    vector<cali_id_t> mpit_pvar_attr;
	vector<MPI_T_pvar_handle> pvar_handle;
	vector<int> pvar_count;
	vector<MPI_Datatype> pvar_type;

    bool      mpit_enabled  { false };
	void 	 *buffer;

	ConfigSet        config;

	ConfigSet::Entry configdata[] = {
    	ConfigSet::Terminator
	};

	static MPI_T_pvar_session pvar_session;
	int num_pvars = 0;

	void snapshot_cb(Caliper* c, int scope, const SnapshotRecord*, SnapshotRecord* snapshot) {

		int size;

		Log(3).stream() << "Collecting PVARs for the MPI-T interface." << endl;

		for(int index=0; index < num_pvars; index++) {
			MPI_T_pvar_read(pvar_session, pvar_handle[index], buffer);
			switch(pvar_type[index])
			{
				
				case MPI_COUNT:
				case MPI_UNSIGNED:
				case MPI_UNSIGNED_LONG:
				case MPI_UNSIGNED_LONG_LONG:
				{
			    	snapshot->append(mpit_pvar_attr[index], Variant(CALI_TYPE_UINT, buffer, pvar_count[index]));
					
					Log(3).stream() << "Index and Value: " << index << " " << ((unsigned long long int *)buffer)[0] << endl;
					break;
				}
				case MPI_INT:
				{
			    	snapshot->append(mpit_pvar_attr[index], Variant(CALI_TYPE_INT, buffer, pvar_count[index]));
					
					Log(3).stream() << "Index and Value: " <<  index << " " << ((int *)buffer)[0] << endl;
					break;
				}
				case MPI_CHAR:
				{
			    	snapshot->append(mpit_pvar_attr[index], Variant(CALI_TYPE_STRING, buffer, pvar_count[index]));
					
					Log(3).stream() << "Index and Value: " <<  index << "  " << ((char *)buffer)[0] << endl;
					break;
				}
				case MPI_DOUBLE:
				{
			    	snapshot->append(mpit_pvar_attr[index], Variant(CALI_TYPE_DOUBLE, buffer, pvar_count[index]));
					
					Log(3).stream() << "Index and Value: " << index << " " << ((double *)buffer)[0] << endl;
					break;
				}
			}
		}
	}

	void create_attribute_for_pvar(Caliper *c, int index, const string& name, MPI_Datatype datatype) {
	    Attribute attr;
		switch(datatype)
		{
			case MPI_COUNT:
			case MPI_UNSIGNED:
			case MPI_UNSIGNED_LONG:
			case MPI_UNSIGNED_LONG_LONG:
			{
				attr = c->create_attribute(string("mpit.")+name, CALI_TYPE_UINT,
                                    CALI_ATTR_ASVALUE      | 
                                    CALI_ATTR_SCOPE_PROCESS | 
                                    CALI_ATTR_SKIP_EVENTS);
				break;
			}
			case MPI_INT:
			{
				attr = c->create_attribute(string("mpit.")+name, CALI_TYPE_INT,
                                    CALI_ATTR_ASVALUE      | 
                                    CALI_ATTR_SCOPE_PROCESS | 
                                    CALI_ATTR_SKIP_EVENTS);
				break;
			}
			case MPI_CHAR:
			{
				attr = c->create_attribute(string("mpit.")+name, CALI_TYPE_STRING,
                                    CALI_ATTR_ASVALUE      | 
                                    CALI_ATTR_SCOPE_PROCESS | 
                                    CALI_ATTR_SKIP_EVENTS);
				break;
			}
			case MPI_DOUBLE:
			{
				attr = c->create_attribute(string("mpit.")+name, CALI_TYPE_DOUBLE,
                                    CALI_ATTR_ASVALUE      | 
                                    CALI_ATTR_SCOPE_PROCESS | 
                                    CALI_ATTR_SKIP_EVENTS);
				break;
			}
		}

		mpit_pvar_attr.push_back(attr.id());
		Log(1).stream() << "Attribute created with name: " << attr.name() << endl;
	}


	/*Allocate handles for pvars and create attributes*/
	void mpit_allocate_pvar_handles(Caliper *c) {
		int current_num_pvars, return_val;
		char pvar_name[NAME_LEN], pvar_desc[NAME_LEN] = "";
		int var_class, verbosity, bind, readonly, continuous, atomic, name_len, desc_len;
		MPI_Datatype datatype;
		MPI_T_enum enumtype;

		desc_len = name_len = NAME_LEN;
		/* Get the number of pvars exported by the implementation */
		return_val = MPI_T_pvar_get_num(&current_num_pvars);
		if (return_val != MPI_SUCCESS)
		{
			perror("MPI_T_pvar_get_num ERROR:");
			Log(0).stream() << "MPI_T_pvar_get_num ERROR: " << return_val << endl;
		    return;
		}

		pvar_handle.reserve(current_num_pvars);
		pvar_type.reserve(current_num_pvars);
		pvar_count.reserve(current_num_pvars);
		mpit_pvar_attr.reserve(current_num_pvars);

		Log(0).stream() << "Num PVARs exported: " << current_num_pvars << endl;

		for(int index=num_pvars; index < current_num_pvars; index++) {
			desc_len = name_len = NAME_LEN;

			MPI_T_pvar_get_info(index, pvar_name, &name_len, &verbosity, &var_class, &datatype, &enumtype, 
								pvar_desc, &desc_len, &bind, &readonly, &continuous, &atomic);
			
			/* allocate a pvar handle that will be used later */
			pvar_count[index] = 0;
			return_val = MPI_T_pvar_handle_alloc(pvar_session, index, NULL, &(pvar_handle.data())[index], &(pvar_count.data())[index]);
			if (return_val != MPI_SUCCESS)
			{
				Log(0).stream() << "MPI_T_pvar_handle_alloc ERROR:" << return_val << " for PVAR at index " << index << " with name " << pvar_name << endl;
		    	return;
  			}

		    pvar_type[index] = datatype; 

		   /*Non-continuous variables need to be started before being read. If this is not done
		   *TODO:Currently, the MVAPICH and MPICH implementations error out if non-continuous PVARs are not started before being read.
		   *Check if this is expected behaviour from an MPI implementation. No mention of the need to do this from a clients perspective in the 3.1 standard.*/
		   if(continuous == 0) 
		   {
		       return_val = MPI_T_pvar_start(pvar_session, pvar_handle[index]);

		   		if (return_val != MPI_SUCCESS) 
				{
					Log(0).stream() << "MPI_T_pvar_start ERROR:" << return_val << " for PVAR at index " << index << " with name " << pvar_name << endl;
					return;
    			}

			}
			
			string s(pvar_name);
			create_attribute_for_pvar(c, index, s, datatype);
			
		}
		::num_pvars = current_num_pvars;
		
	}


	/*Register the service and initalize the MPI-T interface*/
	void mpit_register(Caliper *c)
	{	
	    int thread_provided, return_val;

    	config = RuntimeConfig::init("mpit", configdata);
		buffer = (void*)malloc(sizeof(unsigned long long int)*SOME_BIG_ENOUGH_VALUE); 
    
		/* Initialize MPI_T */
		return_val = MPI_T_init_thread(MPI_THREAD_SINGLE, &thread_provided);

		if (return_val != MPI_SUCCESS) 
		{
			Log(0).stream() << "MPI_T_init_thread ERROR: " << return_val << ". MPIT service disabled." << endl;
			return;
		} 
		else 
		{
		/* Track a performance pvar session */
			return_val = MPI_T_pvar_session_create(&pvar_session);
			if (return_val != MPI_SUCCESS) {
			    Log(0).stream() << "MPI_T_pvar_session_create ERROR: " << return_val << ". MPIT service disabled." << endl;
			    return;
		    }  
 		}
    	
		mpit_enabled = true; 
    
    	Log(1).stream() << "Registered MPIT service" << endl;
		
		mpit_allocate_pvar_handles(c);
		c->events().snapshot.connect(&snapshot_cb);
		
	}

} // anonymous namespace 

namespace cali 
{
    CaliperService mpit_service = { "mpit", ::mpit_register };
} // namespace cali
