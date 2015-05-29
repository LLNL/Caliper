/// @file services.inc.cpp
/// Static list of all available Caliper service modules.


namespace cali
{

#ifdef CALIPER_HAVE_LIBUNWIND
extern const CaliperService CallpathService;
#endif
#ifdef CALIPER_HAVE_PAPI
extern const CaliperService PapiService;
#endif
extern const CaliperService DebugService;
extern const CaliperService PthreadService;
extern const CaliperService RecorderService;
extern const CaliperService TimestampService;
#ifdef CALIPER_HAVE_MITOS
extern const CaliperService MitosService;
#endif
#ifdef CALIPER_HAVE_MPI
extern const CaliperService MpiService;
#endif
#ifdef CALIPER_HAVE_OMPT
extern const CaliperService OmptService;
#endif

const CaliperService caliper_services[] = {
#ifdef CALIPER_HAVE_LIBUNWIND
    CallpathService,
#endif
#ifdef CALIPER_HAVE_PAPI
    PapiService,
#endif
    DebugService,
    PthreadService,
    RecorderService,
    TimestampService,
#ifdef CALIPER_HAVE_MITOS
    MitosService,
#endif
#ifdef CALIPER_HAVE_MPI
    MpiService,
#endif
#ifdef CALIPER_HAVE_OMPT
    OmptService,
#endif
    { nullptr, { nullptr } }
};

}
