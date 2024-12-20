// wrapConfigManager.h
// This file is generated by Shroud 0.13.0. Do not edit.
/**
 * \file wrapConfigManager.h
 * \brief Shroud generated wrapper for ConfigManager class
 */
// For C users and C++ implementation

#ifndef WRAPCONFIGMANAGER_H
#define WRAPCONFIGMANAGER_H

#ifndef __cplusplus
#include <stdbool.h>
#endif
#include "typesCaliper.h"

// splicer begin class.ConfigManager.CXX_declarations
// splicer end class.ConfigManager.CXX_declarations

#ifdef __cplusplus
extern "C"
{
#endif

// splicer begin class.ConfigManager.C_declarations
// splicer end class.ConfigManager.C_declarations

cali_ConfigManager* cali_ConfigManager_new(cali_ConfigManager* SHC_rv);

void cali_ConfigManager_delete(cali_ConfigManager* self);

void cali_ConfigManager_set_default_parameter(cali_ConfigManager* self, const char* option, const char* val);

void cali_ConfigManager_set_default_parameter_for_config(
    cali_ConfigManager* self,
    const char*         config,
    const char*         option,
    const char*         val
);

void cali_ConfigManager_add_config_spec(cali_ConfigManager* self, const char* spec);

void cali_ConfigManager_add_option_spec(cali_ConfigManager* self, const char* spec);

void cali_ConfigManager_add(cali_ConfigManager* self, const char* config);

bool cali_ConfigManager_error(cali_ConfigManager* self);

void cali_ConfigManager_error_msg_bufferify(cali_ConfigManager* self, cali_SHROUD_array* SHT_rv_cdesc);

void cali_ConfigManager_start(cali_ConfigManager* self);

void cali_ConfigManager_stop(cali_ConfigManager* self);

void cali_ConfigManager_flush(cali_ConfigManager* self);

#ifdef __cplusplus
}
#endif

#endif // WRAPCONFIGMANAGER_H
