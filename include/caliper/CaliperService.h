// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

/// \file  CaliperService.h
/// \brief Definition of CaliperService struct

#pragma once

namespace cali
{

class Caliper;
class Channel;

typedef void (*ServiceRegisterFn)(Caliper* c, Channel* chn);

/// \brief Name and entry point for services.
///
/// To register services, provide a list of CaliperService entries
/// to Caliper::add_services() _before_ %Caliper is initialized.
struct CaliperService {
    const char*       name;        ///< Service name (short, no spaces).
    ServiceRegisterFn register_fn; ///< Registration function.
};

} // namespace cali
