// utilCaliper.cpp
// This file is generated by Shroud 0.13.0. Do not edit.

#include "caliper/cali.h"
#include "caliper/ConfigManager.h"
#include <string>
#include "BufferedRegionProfile.h"
#include "typesCaliper.h"
#include <cstddef>
#include <cstring>

#ifdef __cplusplus
extern "C"
{
#endif

// helper copy_string
// Copy the char* or std::string in context into c_var.
// Called by Fortran to deal with allocatable character.
void cali_ShroudCopyStringAndFree(cali_SHROUD_array* data, char* c_var, size_t c_var_len)
{
    const char* cxx_var = data->addr.ccharp;
    size_t      n       = c_var_len;
    if (data->elem_len < n)
        n = data->elem_len;
    std::strncpy(c_var, cxx_var, n);
    cali_SHROUD_memory_destructor(&data->cxx); // delete data->cxx.addr
}

// Release library allocated memory.
void cali_SHROUD_memory_destructor(cali_SHROUD_capsule_data* cap)
{
    void* ptr = cap->addr;
    switch (cap->idtor) {
    case 0: // --none--
        {
            // Nothing to delete
            break;
        }
    case 1: // cali::ScopeAnnotation
        {
            cali::ScopeAnnotation* cxx_ptr = reinterpret_cast<cali::ScopeAnnotation*>(ptr);
            delete cxx_ptr;
            break;
        }
    case 2: // cali::Annotation
        {
            cali::Annotation* cxx_ptr = reinterpret_cast<cali::Annotation*>(ptr);
            delete cxx_ptr;
            break;
        }
    case 3: // cali::ConfigManager
        {
            cali::ConfigManager* cxx_ptr = reinterpret_cast<cali::ConfigManager*>(ptr);
            delete cxx_ptr;
            break;
        }
    case 4: // new_string
        {
            std::string* cxx_ptr = reinterpret_cast<std::string*>(ptr);
            delete cxx_ptr;
            break;
        }
    case 5: // cali::BufferedRegionProfile
        {
            cali::BufferedRegionProfile* cxx_ptr = reinterpret_cast<cali::BufferedRegionProfile*>(ptr);
            delete cxx_ptr;
            break;
        }
    default:
        {
            // Unexpected case in destructor
            break;
        }
    }
    cap->addr  = nullptr;
    cap->idtor = 0; // avoid deleting again
}

#ifdef __cplusplus
}
#endif
