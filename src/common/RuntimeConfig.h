/// @file RuntimeConfig.h
/// RuntimeConfig definition

#ifndef CALI_RUNTIMECONFIG_H
#define CALI_RUNTIMECONFIG_H

#include "Variant.h"

#include <memory>

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
        const char*   key;         ///< Variable key
        cali_attr_type type;        ///< Variable type
        const char*   value;       ///< (Default) value as string
        const char*   descr;       ///< One-line description
        const char*   long_descr;  ///< Extensive, multi-line description
    };

    static constexpr Entry Terminator = { 0, CALI_TYPE_INV, 0, 0, 0 };

    constexpr ConfigSet() = default;

    Variant get(const char* key) const;
};


class RuntimeConfig
{

public:

    static Variant   get(const char* set, const char* key);
    static ConfigSet init(const char* name, const ConfigSet::Entry* set);

    static void print(std::ostream& os);

}; // class RuntimeConfig

} // namespace cali

#endif // CALI_RUNTIMECONFIG_H
