// --- Caliper continous integration test app for string escaping

#include "caliper/cali.h"

int main()
{
    cali_set_string_byname(" =\\weird \"\"attribute\"=  ", "  \\\\ weird,\" name\",");
}
