/// \file RuntimeProfileController.hpp
/// \brief RuntimeProfileController class

#pragma once

#include <caliper/ChannelController.h>

namespace cali
{

class RuntimeProfileController : public cali::ChannelController
{
public:
    
    explicit RuntimeProfileController(bool use_mpi = false, const char* output = "stderr");

    ~RuntimeProfileController()
    { }
};    

}
