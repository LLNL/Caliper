// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
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
    /// \brief Name (old style) or JSON spec (new) of the service
    const char*       name_or_spec;
    /// \brief Registration function
    ServiceRegisterFn register_fn;
};

} // namespace cali
