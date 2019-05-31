// Copyright (c) 2019, Lawrence Livermore National Security, LLC.  
// See top-level LICENSE file for details.

/// \file ConfigManager.hpp
/// \brief %Caliper ConfigManager class definition

#pragma once

#include <memory>
#include <string>
#include <vector>

namespace cali
{

class ChannelController;

class ConfigManager
{
    struct ConfigManagerImpl;
    std::shared_ptr<ConfigManagerImpl> mP;
    
public:

    ConfigManager();
    
    explicit ConfigManager(const char* config_string);

    ~ConfigManager();

    bool add(const char* config_string);

    void use_mpi(bool);
    void set_default_parameter(const char* key, const char* value);

    bool error() const;
    std::string error_msg() const;
    
    typedef std::shared_ptr<cali::ChannelController> ChannelPtr;
    typedef std::vector<ChannelPtr> ChannelList;

    ChannelList
    get_all_channels();

    ChannelPtr
    get_channel(const char* name);
};

} // namespace cali
