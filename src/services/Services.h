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

namespace services
{

/// \brief Add Caliper service specs.
void add_service_specs(const CaliperService* specs);

/// \brief Add the default built-in service specs.
void add_default_service_specs();

/// \brief Register service \a name in channel \a chn.
bool register_service(Caliper* c, Channel* chn, const char* name);
/// \brief Register all services in the channel config
void register_configured_services(Caliper* c, Channel* chn);

/// \brief Get all currently available service names.
std::vector<std::string> get_available_services();

} // namespace services

} // namespace cali

#endif // CALI_SERVICES_H
