/// @file services.inc.cpp
/// Static list of all available Caliper service modules.


namespace cali
{

extern const CaliperService DebugService;
extern const CaliperService RecorderService;
extern const CaliperService TimestampService;
#ifdef CALIPER_HAVE_OMPT
extern const CaliperService OmptService;
#endif

const CaliperService caliper_services[] = {
    DebugService,
    RecorderService,
    TimestampService,
#ifdef CALIPER_HAVE_OMPT
    OmptService,
#endif
    { nullptr, { nullptr } }
};


// --- Metadata Writer Services (will be refactored)

struct MetadataWriterService {
    const char*   name;
    void                           (*register_fn)();
    std::unique_ptr<MetadataWriter>(*create_fn)();
};

extern void csv_writer_register();
extern unique_ptr<MetadataWriter> csv_writer_create();

const MetadataWriterService metadata_writer_services[] = {
    { "csv", csv_writer_register, csv_writer_create },
    
    // Terminator
    { 0, 0, 0 }
};

}
