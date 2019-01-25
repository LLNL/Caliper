// Test case for the report service and the C runtime config API

#include "caliper/cali.h"

int main()
{
    cali_config_allow_read_env(0);
    cali_config_preset("CALI_LOG_VERBOSITY", "0");

    cali::create_channel("report profile", 0, {
            { "CALI_SERVICES_ENABLE", "event, trace, report" },
            { "CALI_REPORT_CONFIG",
                    "SELECT function,annotation,count() WHERE annotation,function GROUP BY annotation,function FORMAT table" }
        });

    CALI_CXX_MARK_FUNCTION;

    CALI_MARK_BEGIN("my phase");

    CALI_MARK_END("my phase");    

    CALI_MARK_BEGIN("my phase");

    CALI_MARK_END("my phase");    
}
