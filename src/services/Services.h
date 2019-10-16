// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

/// \file Services.h
/// Services class declaration

#ifndef CALI_SERVICES_H
#define CALI_SERVICES_H

#include <memory>
#include <string>
#include <vector>

namespace cali
{

class Caliper;
struct CaliperService;
class Channel;

class Services
{
    struct ServicesImpl;
    std::unique_ptr<ServicesImpl> mP;

public:

    static void add_services(const CaliperService* services);

    static void add_default_services();

    static void register_services(Caliper* c, Channel* chn);

    static std::vector<std::string> get_available_services();
};

} // namespace cali

#endif // CALI_SERVICES_H
