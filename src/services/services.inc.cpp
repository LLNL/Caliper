/// @file services.inc.cpp
/// Static list of all available Caliper service modules.


namespace cali
{

extern void csv_writer_register();
extern unique_ptr<MetadataWriter> csv_writer_create();

const Services::MetadataWriterService metadata_writer_services[] = {
    { "csv", csv_writer_register, csv_writer_create },
    
    // Terminator
    { 0, 0, 0 }
};

}
