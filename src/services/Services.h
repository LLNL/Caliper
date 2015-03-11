/// @file Services.h
/// Services class declaration

#ifndef CALI_SERVICES_H
#define CALI_SERVICES_H

#include <memory>

namespace cali
{

class Caliper;

class Services
{
    struct ServicesImpl;
    std::unique_ptr<ServicesImpl> mP;

public:

    static void register_services(Caliper* c = nullptr);
};

} // namespace cali

#endif // CALI_SERVICES_H
