// Copyright (c) 2021, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#pragma once

#ifndef CALI_SPOTCONTROLLER_H
#define CALI_SPOTCONTROLLER_H

#include "caliper/ConfigManager.h"

#include "../CustomOutputController.h"

namespace cali
{

class OutputStream;

class SpotController : public cali::internal::CustomOutputController
{
    struct SpotControllerImpl;
    std::shared_ptr<SpotControllerImpl> mP;

    void on_create(Caliper*, Channel*) override;

public:

    static const char* spec;

    static std::string check_options(const ConfigManager::Options& opts);

    void collective_flush(cali::internal::CustomOutputController::Comm& comm, OutputStream& stream) override;

    SpotController(const char* name, const config_map_t& initial_cfg, const ConfigManager::Options& opts);
};

} // namespace cali

#endif
