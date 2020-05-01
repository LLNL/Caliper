#include "caliper/RegionProfile.h"

namespace cali
{

class BufferedRegionProfile : private RegionProfile
{
    struct BufferedRegionProfileImpl;
    std::shared_ptr<BufferedRegionProfileImpl> mP;

public:

    BufferedRegionProfile();
    ~BufferedRegionProfile();

    void   start();
    void   stop();
    
    void   clear();

    void   fetch_exclusive_region_times(const char* region_type = "");
    void   fetch_inclusive_region_times(const char* region_type = "");

    double total_profiling_time() const;
    double total_region_time() const;
    double region_time(const char* region) const;
};

}
