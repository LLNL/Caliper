// Copyright (c) 2021, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#pragma once

#ifndef CALI_LOOPREPORTCONTROLLER_H
#define CALI_LOOPREPORTCONTROLLER_H

#include "caliper/ConfigManager.h"

#include "../CustomOutputController.h"

namespace cali
{

class OutputStream;

class LoopReportController : public cali::internal::CustomOutputController
{
    struct LoopReportControllerImpl;
    std::shared_ptr<LoopReportControllerImpl> mP;

public:

    static const char* spec;

    void collective_flush(cali::internal::CustomOutputController::Comm& comm, OutputStream& stream) override;

    LoopReportController(const char* name, const config_map_t& initial_cfg, const ConfigManager::Options& opts);
};

} // namespace cali

#endif
