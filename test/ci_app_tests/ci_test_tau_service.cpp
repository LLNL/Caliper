// Test case for the TAU service and the C runtime config API

#include "caliper/cali.h"

const char* report_profile[][2] = {
    { "CALI_SERVICES_ENABLE", "event, trace, report" },

    { "CALI_REPORT_CONFIG",
      "SELECT function,annotation,count() WHERE annotation,function GROUP BY annotation,function FORMAT table" },

    { NULL, NULL }
};

int main()
{
    cali_config_allow_read_env(0);
    cali_config_preset("CALI_LOG_VERBOSITY", "0");
    cali_config_define_profile("report profile", report_profile);
    cali_config_set("CALI_CONFIG_PROFILE", "\"report profile\"");

    CALI_CXX_MARK_FUNCTION;

    CALI_MARK_BEGIN("my phase");

    CALI_MARK_END("my phase");    

    CALI_MARK_BEGIN("my phase");

    CALI_MARK_END("my phase");    
}
