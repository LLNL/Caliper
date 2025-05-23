// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

/// \file Services.h
/// %Caliper services management

#ifndef CALI_SERVICES_H
#define CALI_SERVICES_H

#include "../common/RuntimeConfig.h"

#include <iostream>
#include <string>
#include <vector>

namespace cali
{

class Caliper;
class Channel;

typedef void (*ServiceRegisterFn)(Caliper* c, Channel* chn);

/// \brief Provides spec and entry point for a %Caliper service.
struct CaliperService {
    /// \brief Name (old style) or JSON spec (new) of the service
    const char* name_or_spec;
    /// \brief Registration function
    ServiceRegisterFn register_fn;
};

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

/// \brief Read and initialize runtime config set from given JSON spec
ConfigSet init_config_from_spec(RuntimeConfig cfg, const char* spec);

/// \brief Find and print service documentation (description and options)
std::ostream& print_service_documentation(std::ostream& os, const std::string& name);

/// \brief Get description string for service
std::string get_service_description(const std::string& name);

/// \brief Get all currently available service names.
std::vector<std::string> get_available_services();

} // namespace services

} // namespace cali

#endif // CALI_SERVICES_H
