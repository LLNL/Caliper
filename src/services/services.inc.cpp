/// @file services.inc.cpp
/// Static list of all available Caliper service modules.


namespace cali
{

#ifdef CALIPER_HAVE_LIBUNWIND
extern CaliperService CallpathService;
#endif
#ifdef CALIPER_HAVE_PAPI
extern CaliperService PapiService;
#endif
extern CaliperService DebugService;
extern CaliperService EventTriggerService;
extern CaliperService PthreadService;
extern CaliperService RecorderService;
extern CaliperService TimestampService;
extern CaliperService StatisticsService;
#ifdef CALIPER_HAVE_MITOS
extern CaliperService MitosService;
#endif
#ifdef CALIPER_HAVE_MPI
extern CaliperService MpiService;
#endif
#ifdef CALIPER_HAVE_OMPT
extern CaliperService OmptService;
#endif

const CaliperService caliper_services[] = {
#ifdef CALIPER_HAVE_LIBUNWIND
    CallpathService,
#endif
#ifdef CALIPER_HAVE_PAPI
    PapiService,
#endif
    DebugService,
    EventTriggerService,
    PthreadService,
    RecorderService,
    TimestampService,
    StatisticsService,
#ifdef CALIPER_HAVE_MITOS
    MitosService,
#endif
#ifdef CALIPER_HAVE_MPI
    MpiService,
#endif
#ifdef CALIPER_HAVE_OMPT
    OmptService,
#endif
    { nullptr, nullptr }
};

}
