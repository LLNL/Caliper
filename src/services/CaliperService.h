/// @file CaliperService.h
/// @definition of CaliperService struct

#ifndef CALI_CALIPERSERVICE_H
#define CALI_CALIPERSERVICE_H

#include <functional>

namespace cali
{

class Caliper;

struct CaliperService {
    const char* name;
    std::function<void(Caliper*)> register_fn;
};

} // namespace cali

#endif
