// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include <curious/curious.h>

#include "caliper/Caliper.h"
#include "caliper/CaliperService.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/common/Log.h"

#include <string.h>
#include <cstddef>
#include <unordered_map>

using namespace cali;

namespace
{
  std::unordered_map<Channel *, curious_t> curious_insts;

  int num_io_callbacks;
  int num_failed_io_callbacks;

  Attribute io_region_attr;
  Attribute io_filesystem_attr;
  Attribute io_mount_point_attr;
  Attribute io_bytes_read_attr;
  Attribute io_bytes_written_attr;

  // The least significant bit of he callback type is for whether
  // it comes before or after an I/O call, so we can discard it
  // to index the categories of I/O calls
  #define GET_CATEGORY(callback_type) ((callback_type) >> 1)

  // A lookup table for the Variant corresponding to each callback category
  Variant categories[] = {
     Variant(CALI_TYPE_STRING, "read",     5),
     Variant(CALI_TYPE_STRING, "write",    6),
     Variant(CALI_TYPE_STRING, "metadata", 9),
  };

  // We need to record different info for different I/O regions/records:

  // For read regions...
  void handle_unique_record_data(Caliper &c, Channel *channel, curious_read_record_t *record) {
    if (record->bytes_read > 0) {
      // ...we care about how much data was read
      Variant v_bytes(cali_make_variant_from_uint(record->bytes_read));
      cali_id_t bytes_id(io_bytes_read_attr.id());

      SnapshotRecord rec(1, &bytes_id, &v_bytes);
      c.push_snapshot(channel, &rec);
    }
  }

  // For write regions...
  void handle_unique_record_data(Caliper &c, Channel *channel, curious_write_record_t *record) {
    if (record->bytes_written > 0) {
      // ...we care about how much data was written
      Variant v_bytes(cali_make_variant_from_uint(record->bytes_written));
      cali_id_t bytes_id(io_bytes_written_attr.id());

      SnapshotRecord rec(1, &bytes_id, &v_bytes);
      c.push_snapshot(channel, &rec);
    }
  }

  // We don't need anything extra from metadata regions
  void handle_unique_record_data(Caliper &c, Channel *channel, curious_metadata_record_t *record) {}

  // Just preforms some sanity checking before making a Variant from a C string
  Variant make_variant(char *str) {
    Variant var;

    if (nullptr == str) {
      var = Variant(CALI_TYPE_STRING, "unknown", 8);
    } else {
      var = Variant(CALI_TYPE_STRING, str, strlen(str) + 1);
    }

    return var;
  }

  template <typename Record>
  void handle_io(curious_callback_type_t type, Channel *channel, Record *record) {
    ++num_io_callbacks;

    // get a Caliper instance

    Caliper c = Caliper::sigsafe_instance();

    // If we got a Caliper instance successfully...
    if (c && channel->is_active()) {
      // ...mark the end...
      if (type & CURIOUS_POST_CALLBACK) {
        handle_unique_record_data(c, channel, record);

        c.end(io_region_attr);
        c.end(io_filesystem_attr);
        c.end(io_mount_point_attr);

      // ...or begining of the I/O region, as appropriate
      } else {
        Variant filesystem_var = make_variant(record->filesystem);
        Variant mount_point_var = make_variant(record->mount_point);

        c.begin(io_mount_point_attr, mount_point_var);
        c.begin(io_filesystem_attr, filesystem_var);
        c.begin(io_region_attr, categories[GET_CATEGORY(type)]);
      }

    // ...otherwise note our failure (probably already inside Caliper)
    } else {
      ++num_failed_io_callbacks;
    }
  }


  void init_curious_in_channel(Caliper* c, Channel* channel) {
    channel->events().subscribe_attribute(c, channel, io_region_attr);

    curious_t curious_inst = curious_init(CURIOUS_ALL_APIS);

    curious_register_callback(
      curious_inst,
      CURIOUS_READ_CALLBACK,
      (curious_callback_f) handle_io<curious_read_record_t>,
      (void *) channel
    );
    curious_register_callback(
      curious_inst,
      CURIOUS_READ_CALLBACK | CURIOUS_POST_CALLBACK,
      (curious_callback_f) handle_io<curious_read_record_t>,
      (void *) channel
    );
    curious_register_callback(
      curious_inst,
      CURIOUS_WRITE_CALLBACK,
      (curious_callback_f) handle_io<curious_write_record_t>,
      (void *) channel
    );
    curious_register_callback(
      curious_inst,
      CURIOUS_WRITE_CALLBACK | CURIOUS_POST_CALLBACK,
      (curious_callback_f) handle_io<curious_write_record_t>,
      (void *) channel
    );
    curious_register_callback(
      curious_inst,
      CURIOUS_METADATA_CALLBACK,
      (curious_callback_f) handle_io<curious_metadata_record_t>,
      (void *) channel
    );
    curious_register_callback(
      curious_inst,
      CURIOUS_METADATA_CALLBACK | CURIOUS_POST_CALLBACK,
      (curious_callback_f) handle_io<curious_metadata_record_t>,
      (void *) channel
    );

    curious_insts[channel] = curious_inst;
  }

  void finalize_curious_in_channel(Caliper* c, Channel* channel) {
    curious_finalize(curious_insts[channel]);

    if (Log::verbosity() >= 2)
      Log(2).stream() << channel->name() << ": Processed "
                      << num_io_callbacks << " I/O callbacks, "
                      << num_failed_io_callbacks << " failed." << std::endl;
  }

  void init_curious_service(Caliper* c, Channel* channel) {
    Attribute subscribe_attr = c->get_attribute("subscription_event");
    Variant v_true(true);

    // create I/O attributes
    io_region_attr =
      c->create_attribute("io.region",        CALI_TYPE_STRING,
                          CALI_ATTR_SCOPE_THREAD,
                          1, &subscribe_attr, &v_true);
    io_filesystem_attr =
      c->create_attribute("io.filesystem",    CALI_TYPE_STRING,
                          CALI_ATTR_SCOPE_THREAD | CALI_ATTR_SKIP_EVENTS);
    io_mount_point_attr =
      c->create_attribute("io.mount.point",   CALI_TYPE_STRING,
                          CALI_ATTR_SCOPE_THREAD | CALI_ATTR_SKIP_EVENTS);

    Attribute aggr_attr = c->get_attribute("class.aggregatable");

    io_bytes_read_attr =
      c->create_attribute("io.bytes.read",    CALI_TYPE_UINT,
                          CALI_ATTR_SCOPE_THREAD | CALI_ATTR_ASVALUE,
                          1, &aggr_attr, &v_true);
    io_bytes_written_attr =
      c->create_attribute("io.bytes.written", CALI_TYPE_UINT,
                          CALI_ATTR_SCOPE_THREAD | CALI_ATTR_ASVALUE,
                          1, &aggr_attr, &v_true);

    // register Caliper post_init_evt and finish_evt callbacks
    channel->events().post_init_evt.connect(init_curious_in_channel);

    // finish_evt: unregister channel from curious (??)
    channel->events().finish_evt.connect(finalize_curious_in_channel);

    Log(1).stream() << channel->name() << ": Registered io service" << std::endl;
  }
} // namespace [anonymous]

namespace cali
{

CaliperService io_service { "io", ::init_curious_service };

}
