/// @file CaliperService.h
/// @definition of CaliperService struct

#ifndef CALI_CALIPERSERVICE_H
#define CALI_CALIPERSERVICE_H

namespace cali
{

class Caliper;

typedef void (*ServiceRegisterFn)(Caliper* c);

struct CaliperService {
    const char*       name;
    ServiceRegisterFn register_fn;
};

} // namespace cali

#endif
