/// @file services.inc.cpp
/// Static list of all available Caliper service modules.


namespace cali
{

extern const CaliperService DebugService;
extern const CaliperService PthreadService;
extern const CaliperService RecorderService;
extern const CaliperService TimestampService;
#ifdef CALIPER_HAVE_OMPT
extern const CaliperService OmptService;
#endif

const CaliperService caliper_services[] = {
    DebugService,
    PthreadService,
    RecorderService,
    TimestampService,
#ifdef CALIPER_HAVE_OMPT
    OmptService,
#endif
    { nullptr, { nullptr } }
};

}
