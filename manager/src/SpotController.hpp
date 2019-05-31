#pragma once

#include <caliper/ChannelController.h>

namespace cali
{

class SpotController : public cali::ChannelController
{
    std::string m_output;
    bool m_use_mpi;
    
public:

    explicit SpotController(bool use_mpi, const char* output);

    ~SpotController();

    void flush();
};

}
