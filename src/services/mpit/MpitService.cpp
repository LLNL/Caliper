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
#include <string>
#include <cstring>
#include <vector>

using namespace cali;
using namespace std;

#define NAME_LEN 1024
#define SOME_BIG_ENOUGH_VALUE 1024
#define MAX_COUNT 10

namespace cali
{
	bool mpit_enabled { false };
    vector<cali_id_t> mpit_pvar_attr;
	vector<cali_id_t> watermark_changed_attr;
	vector<cali_id_t> watermark_change_attr;
}

namespace
{
	void 	 *buffer;
	
	vector<vector<MPI_T_pvar_handle> > pvar_handle;
	vector<vector<int> >pvar_count;
	vector<int> pvar_continuousness;
	vector<int> pvar_readonlyness;
	vector<bool> pvar_is_aggregatable;
	vector<MPI_Datatype> pvar_type;
	vector<int> pvar_class;
	vector<int> pvar_bind;
	vector<string> pvar_names;
	MPI_T_pvar_session pvar_session;

	//Arrays storing last values of PVARs. This is a hack. Only in place because current MPI implementations do not
	//support resetting of PVARs. So we store last value of pvars and subtract it
	vector<array <unsigned long long int, MAX_COUNT> > last_value_unsigned_long;
	vector<array <double, MAX_COUNT> > last_value_double;

	ConfigSet        config;

	ConfigSet::Entry configdata[] = {
    	ConfigSet::Terminator
	};

	int num_pvars = 0;

	void snapshot_cb(Caliper* c, int scope, const SnapshotRecord*, SnapshotRecord* snapshot) {

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

	}

	void create_attribute_for_pvar(Caliper *c, int index, const string& name, MPI_Datatype datatype) {
	    Attribute attr;
		Attribute aggr_class_attr = c->get_attribute("class.aggregatable");
	    Variant   v_true(true);
	    Variant   v_false(false);

			if((datatype == MPI_COUNT) || (datatype == MPI_UNSIGNED) || (datatype == MPI_UNSIGNED_LONG) || (datatype == MPI_UNSIGNED_LONG_LONG))
			{
				if(pvar_is_aggregatable[index]) {
					attr = c->create_attribute(string("mpit.")+name, CALI_TYPE_UINT,
    	                                CALI_ATTR_ASVALUE      | 
        	                            CALI_ATTR_SCOPE_PROCESS | 
            	                        CALI_ATTR_SKIP_EVENTS, 1, &aggr_class_attr, &v_true);
				} else {
					attr = c->create_attribute(string("mpit.")+name, CALI_TYPE_UINT,
    	                                CALI_ATTR_ASVALUE      | 
        	                            CALI_ATTR_SCOPE_PROCESS | 
            	                        CALI_ATTR_SKIP_EVENTS, 1, &aggr_class_attr, &v_false);
				}
			}
			else if(datatype == MPI_INT)
			{
				if(pvar_is_aggregatable[index]) {
					attr = c->create_attribute(string("mpit.")+name, CALI_TYPE_INT,
    	                                CALI_ATTR_ASVALUE      | 
        	                            CALI_ATTR_SCOPE_PROCESS | 
            	                        CALI_ATTR_SKIP_EVENTS, 1, &aggr_class_attr, &v_true);
				} else {
					attr = c->create_attribute(string("mpit.")+name, CALI_TYPE_INT,
    	                                CALI_ATTR_ASVALUE      | 
        	                            CALI_ATTR_SCOPE_PROCESS | 
            	                        CALI_ATTR_SKIP_EVENTS, 1, &aggr_class_attr, &v_false);
				}
			}
			else if(datatype == MPI_CHAR)
			{
				if(pvar_is_aggregatable[index]) {
					attr = c->create_attribute(string("mpit.")+name, CALI_TYPE_STRING,
    	                                CALI_ATTR_ASVALUE      | 
        	                            CALI_ATTR_SCOPE_PROCESS | 
            	                        CALI_ATTR_SKIP_EVENTS, 1, &aggr_class_attr, &v_true);
				} else {
					attr = c->create_attribute(string("mpit.")+name, CALI_TYPE_STRING,
    	                                CALI_ATTR_ASVALUE      | 
        	                            CALI_ATTR_SCOPE_PROCESS | 
            	                        CALI_ATTR_SKIP_EVENTS, 1, &aggr_class_attr, &v_false);
				}
			}
			else if(datatype == MPI_DOUBLE)
			{
				if(pvar_is_aggregatable[index]) {
					attr = c->create_attribute(string("mpit.")+name, CALI_TYPE_DOUBLE,
    	                                CALI_ATTR_ASVALUE      | 
        	                            CALI_ATTR_SCOPE_PROCESS | 
            	                        CALI_ATTR_SKIP_EVENTS, 1, &aggr_class_attr, &v_true);
				} else {
					attr = c->create_attribute(string("mpit.")+name, CALI_TYPE_DOUBLE,
    	                                CALI_ATTR_ASVALUE      | 
        	                            CALI_ATTR_SCOPE_PROCESS | 
            	                        CALI_ATTR_SKIP_EVENTS, 1, &aggr_class_attr, &v_false);
				}
			}

		mpit_pvar_attr[index] = attr.id();
		Log(3).stream() << "Attribute created with name: " << attr.name() << endl;
	}

	bool is_pvar_class_aggregatable(int index, const char* pvar_name, Caliper *c) {
	    Attribute attr;
		Attribute aggr_class_attr = c->get_attribute("class.aggregatable");
	    Variant   v_true(true);

		/*General idea to determine aggretablitity of a PVAR: Any PVAR that represents an internal MPI state is by default not aggregatable
		 * A PVAR is aggregatable if it "makes sense" to apply one or more of these operators to it: COUNT, SUM, MIN, MAX, AVG. In the future, more operators may be considered.*/

		switch(pvar_class[index]) {
			case MPI_T_PVAR_CLASS_STATE:
				Log(2).stream() << "PVAR at index: " << index << " with name: " << pvar_name << " has a class: MPI_T_PVAR_CLASS_STATE" << endl;
				return false; //This class is better viewed along with "time" as another dimension - a.k.a. a trace
				break;
			case MPI_T_PVAR_CLASS_LEVEL:
				Log(2).stream() << "PVAR at index: " << index << " with name: " << pvar_name << " has a class: MPI_T_PVAR_CLASS_LEVEL" << endl;
				return true; //AVG operator can be applied
				break;
			case MPI_T_PVAR_CLASS_SIZE:
				Log(2).stream() << "PVAR at index: " << index << " with name: " << pvar_name << " has a class: MPI_T_PVAR_CLASS_SIZE" << endl;
				return false;
				break;
			case MPI_T_PVAR_CLASS_PERCENTAGE:
				Log(2).stream() << "PVAR at index: " << index << " with name: " << pvar_name << " has a class: MPI_T_PVAR_CLASS_PERCENTAGE" << endl;
				return true; //AVG operator can be applied
				break;
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
			case MPI_T_PVAR_CLASS_COUNTER:
				Log(2).stream() << "PVAR at index: " << index << " with name: " << pvar_name << " has a class: MPI_T_PVAR_CLASS_COUNTER" << endl;
				return true; // "SUM" or "COUNT" operator to aggregate
				break;
			case MPI_T_PVAR_CLASS_AGGREGATE:
				Log(2).stream() << "PVAR at index: " << index << " with name: " << pvar_name << " has a class: MPI_T_PVAR_CLASS_AGGREGATE" << endl;
				return true; 
				break;
			case MPI_T_PVAR_CLASS_TIMER:
				Log(2).stream() << "PVAR at index: " << index << " with name: " << pvar_name << " has a class: MPI_T_PVAR_CLASS_TIMER" << endl;
				return true; 
				break;
			case MPI_T_PVAR_CLASS_GENERIC:
				Log(2).stream() << "PVAR at index: " << index << " with name: " << pvar_name << " has a class: MPI_T_PVAR_CLASS_GENERIC" << endl;
				return false; // Arbitrary PVAR that does not fall into any of the other well-defined classes. Implementation and handling of these PVARs must be custom-designed.
				break;
		}
	}			


	/*Allocate PVAR handles when the PVAR is bound to something other than MPI_T_BIND_NO_OBJECT*/
	void do_mpit_allocate_bound_pvar_handles(Caliper *c, void *handle, int bind) {

		int return_val;

		for(int index=0; index < num_pvars; index++) {

			/*Iterate through all pvars, and only allocate handles for those pvars whose bindings are the same as the input to this function*/
			if(bind == pvar_bind[index]) {
				return_val = MPI_T_pvar_handle_alloc(pvar_session, index, handle, &(pvar_handle.data())[index][pvar_handle[index].size() - 1], &(pvar_count.data())[index][pvar_handle[index].size() - 1]);
			} else {
				continue;
			}
			
			if (return_val != MPI_SUCCESS)
			{
				Log(0).stream() << "MPI_T_pvar_handle_alloc ERROR:" << return_val << " for PVAR at index " << index << " with name " << pvar_names[index] << endl;
		    	return;
  			}

		   /*Non-continuous variables need to be started before being read*/
		   if(pvar_continuousness[index] == 0) 
		   {
			   Log(1).stream() << "PVAR at index: " << index << " and name: " << pvar_names[index] << " is non-continuous. Starting this PVAR. " << endl;

		       return_val = MPI_T_pvar_start(pvar_session, pvar_handle[index][pvar_handle[index].size() - 1]);

		   		if (return_val != MPI_SUCCESS) 
				{
					Log(0).stream() << "MPI_T_pvar_start ERROR:" << return_val << " for PVAR at index " << index << " with name " << pvar_names[index] << endl;
					return;
    			}

			}
			
			/*Only create the attribute for the PVAR once*/
			if(1 == pvar_handle[index].size()) {
				create_attribute_for_pvar(c, index, pvar_names[index], pvar_type[index]);
			}

			/*Increase the size of the PVAR handle and PVAR count arrays*/
			pvar_handle[index].resize(pvar_handle[index].size() + 1, NULL);
			pvar_count[index].resize(pvar_count[index].size() + 1, -1);

		}
	}

	/*Allocate handles for pvars and create attributes*/
	void do_mpit_allocate_pvar_handles(Caliper *c) {
		int current_num_pvars, return_val, thread_provided;
		char pvar_name[NAME_LEN], pvar_desc[NAME_LEN] = "";
		int verbosity, atomic, name_len, desc_len;
		MPI_T_enum enumtype;
		MPI_Comm comm;
		comm = MPI_COMM_WORLD;

		desc_len = name_len = NAME_LEN;
		/* Get the number of pvars exported by the implementation */
		return_val = MPI_T_pvar_get_num(&current_num_pvars);
		if (return_val != MPI_SUCCESS)
		{
			perror("MPI_T_pvar_get_num ERROR:");
			Log(0).stream() << "MPI_T_pvar_get_num ERROR: " << return_val << endl;
		    return;
		}

		pvar_handle.resize(current_num_pvars, {NULL});

		pvar_continuousness.resize(current_num_pvars, 0);
		pvar_readonlyness.resize(current_num_pvars, 1);
		pvar_is_aggregatable.resize(current_num_pvars, false);
		pvar_class.resize(current_num_pvars, MPI_T_PVAR_CLASS_GENERIC);
		pvar_type.resize(current_num_pvars);
		pvar_count.resize(current_num_pvars);
		pvar_bind.resize(current_num_pvars, MPI_T_BIND_NO_OBJECT);
		pvar_names.resize(current_num_pvars,"");
		mpit_pvar_attr.resize(current_num_pvars);
		watermark_changed_attr.resize(current_num_pvars);
		watermark_change_attr.resize(current_num_pvars);


	    last_value_unsigned_long.resize(current_num_pvars, {0});
	    last_value_double.resize(current_num_pvars, {0.0});

		Log(1).stream() << "Num PVARs exported: " << current_num_pvars << endl;

		for(int index=num_pvars; index < current_num_pvars; index++) {

			pvar_handle[index].resize(1, NULL);
			pvar_count[index].resize(1, -1);

			desc_len = name_len = NAME_LEN;

			MPI_T_pvar_get_info(index, pvar_name, &name_len, &verbosity, &(pvar_class.data())[index], &(pvar_type.data())[index], &enumtype, 
								pvar_desc, &desc_len, &(pvar_bind.data())[index], &(pvar_readonlyness.data())[index], &(pvar_continuousness.data())[index], &atomic);
			
			pvar_names[index] = pvar_name;
			pvar_is_aggregatable[index] = is_pvar_class_aggregatable(index, pvar_names[index].c_str(), c);

			Log(3).stream() << "PVAR at index: " << index << " with name: " << pvar_name << " has readonly flag set as: " << pvar_readonlyness[index] << endl;
			Log(2).stream() << "PVAR at index: " << index << " with name: " << pvar_name << " has description: " << pvar_desc << endl;

			/* allocate a pvar handle that will be used later */

			switch (pvar_bind[index]) {
				case MPI_T_BIND_NO_OBJECT:
				{
					Log(3).stream() << "PVAR at index: " << index << " with name: " << pvar_name << " is not bound to an MPI object" << endl;
					return_val = MPI_T_pvar_handle_alloc(pvar_session, index, NULL, &(pvar_handle.data())[index][0], &(pvar_count.data())[index][0]);

					/*Increment the size of the handle and count arrays*/
					pvar_handle[index].resize(2, NULL);
					pvar_count[index].resize(2, -1);
					break;
				}
				case MPI_T_BIND_MPI_COMM: //Handle this through a Wrapper call to MPI_Comm_create(). Support MPI_COMM_WORLD, MPI_COMM_SELF as default.
				{
					Log(0).stream() << "PVAR at index: " << index << " with name: " << pvar_name << " is bound to an MPI object of type MPI_T_BIND_MPI_COMM" << endl;
					return_val = MPI_T_pvar_handle_alloc(pvar_session, index, &comm, &(pvar_handle.data())[index][0], &(pvar_count.data())[index][0]);
					pvar_handle[index].resize(2, NULL);
					pvar_count[index].resize(2, -1);
					comm = MPI_COMM_SELF;
					return_val = MPI_T_pvar_handle_alloc(pvar_session, index, &comm, &(pvar_handle.data())[index][1], &(pvar_count.data())[index][1]);
					pvar_handle[index].resize(3, NULL);
					pvar_count[index].resize(3, -1);

					break;
				}
				case MPI_T_BIND_MPI_WIN: //Handle this through a Wrapper call to MPI_Win_create.
				{
					Log(0).stream() << "PVAR at index: " << index << " with name: " << pvar_name << " is bound to an MPI object of type MPI_T_BIND_MPI_WIN. Not doing anything here." << endl;
					continue;
				}
			}

			if (return_val != MPI_SUCCESS)
			{
				Log(0).stream() << "MPI_T_pvar_handle_alloc ERROR:" << return_val << " for PVAR at index " << index << " with name " << pvar_name << endl;
		    	return;
  			}

		   /*Non-continuous variables need to be started before being read.*/
		   if(pvar_continuousness[index] == 0) 
		   {
			   Log(1).stream() << "PVAR at index: " << index << " and name: " << pvar_name << " is non-continuous. Starting this PVAR. " << endl;

		       return_val = MPI_T_pvar_start(pvar_session, pvar_handle[index][0]);

		   		if (return_val != MPI_SUCCESS) 
				{
					Log(0).stream() << "MPI_T_pvar_start ERROR:" << return_val << " for PVAR at index " << index << " with name " << pvar_name << endl;
					return;
    			}

			}
			
			create_attribute_for_pvar(c, index, pvar_names[index], pvar_type[index]);
			
		}
		num_pvars = current_num_pvars;
		
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
		
		do_mpit_allocate_pvar_handles(c);
		c->events().snapshot.connect(&snapshot_cb);

	}

} // anonymous namespace 

namespace cali 
{
    CaliperService mpit_service = { "mpit", ::mpit_register };

	/*Thin wrapper functions to invoke pvar allocation function from another module*/
	void mpit_allocate_pvar_handles() {
		Caliper c;
		::do_mpit_allocate_pvar_handles(&c);
	}

	void mpit_allocate_bound_pvar_handles(void *handle, int bind) {
		Caliper c;
		::do_mpit_allocate_bound_pvar_handles(&c, handle, bind);
	}
} // namespace cali

