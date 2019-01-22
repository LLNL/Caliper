// Copyright (c) 2015, Lawrence Livermore National Security, LLC.  
// Produced at the Lawrence Livermore National Laboratory.
//
// This file is part of Caliper.
// Written by David Boehme, boehme3@llnl.gov.
// LLNL-CODE-678900
// All rights reserved.
//
// For details, see https://github.com/scalability-llnl/Caliper.
// Please also see the LICENSE file for our additional BSD notice.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
//  * Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the disclaimer below.
//  * Redistributions in binary form must reproduce the above copyright notice, this list of
//    conditions and the disclaimer (as noted below) in the documentation and/or other materials
//    provided with the distribution.
//  * Neither the name of the LLNS/LLNL nor the names of its contributors may be used to endorse
//    or promote products derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS
// OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// LAWRENCE LIVERMORE NATIONAL SECURITY, LLC, THE U.S. DEPARTMENT OF ENERGY OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
// ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

/// \file RuntimeConfig.h
/// RuntimeConfig definition

#ifndef CALI_RUNTIMECONFIG_H
#define CALI_RUNTIMECONFIG_H

#include "StringConverter.h"

#include "cali_types.h"

#include <map>
#include <memory>
#include <string>

namespace cali
{

struct ConfigSetImpl;

class ConfigSet 
{
    std::shared_ptr<ConfigSetImpl> mP;

    ConfigSet(const std::shared_ptr<ConfigSetImpl>& p);

    friend class RuntimeConfig;

public:

    struct Entry {
        const char*    key;         ///< Variable key
        cali_attr_type type;        ///< Variable type
        const char*    value;       ///< (Default) value as string
        const char*    descr;       ///< One-line description
        const char*    long_descr;  ///< Extensive, multi-line description
    };

    static constexpr Entry Terminator = { 0, CALI_TYPE_INV, 0, 0, 0 };

    constexpr ConfigSet() = default;

    StringConverter  get(const char* key) const;
};


/// \brief Functionality to read and retrieve %Caliper config settings.
class RuntimeConfig
{
    struct RuntimeConfigImpl;

    std::shared_ptr<RuntimeConfigImpl> mP;
    
public:

    RuntimeConfig();

    /// \brief Get config entry with given \a key from given \a set
    StringConverter get(const char* set, const char* key);

    /// \brief Initialize a ConfigSet.
    ConfigSet       init(const char* name, const ConfigSet::Entry* set);

    /// \brief Pre-set config entry \a key to \a value.
    ///
    /// The value may be overwritten by configuration files or environment
    /// variables.
    void            preset(const char* key, const std::string& value);

    /// \brief Set config entry \a key to \a value.
    ///
    /// The value will *not* be overwritten by configuration files,
    /// profile settings, or environment variables.
    void            set(const char* key, const std::string& value);

    /// \brief Import config values from the given the \a values map
    void            import(const std::map<std::string, std::string>& values);
    
    bool            allow_read_env();

    /// \brief Enable or disable reading of configuration settings
    ///   from environment variables.
    ///
    /// Use with caution: it is generally not recommended to disable
    /// configuration through environment variables, as this limits the
    /// ability to debug %Caliper (e.g., increasing the log level).
    /// Consider providing other means to modify configuration settings
    /// at runtime in this case, e.g. command-line arguments.
    ///
    /// \note Only effective *before* initialization of the %Caliper
    ///   runtime system.
    bool            allow_read_env(bool allow);

    /// \brief Print the current configuration settings.
    ///
    /// \note Only effective after initialization of the %Caliper
    ///   runtime system.
    void print(std::ostream& os);

    //
    // --- Static API (temporary)
    //

    static RuntimeConfig  get_default_config(); 

}; // class RuntimeConfig

} // namespace cali

#endif // CALI_RUNTIMECONFIG_H
