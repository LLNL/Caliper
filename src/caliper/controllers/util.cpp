// Copyright (c) 2021, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "util.h"

#include "../../services/Services.h"

#include <algorithm>

using namespace cali;

std::string util::build_tree_format_spec(config_map_t& config, const ConfigManager::Options& opts, const char* initial)
{
    std::string format = initial;

    if (opts.is_set("max_column_width"))
        format.append("column-width=").append(opts.get("max_column_width").to_string());
    if (opts.is_enabled("print.metadata")) {
        auto avail_services = services::get_available_services();
        bool have_adiak =
            std::find(avail_services.begin(), avail_services.end(), "adiak_import") != avail_services.end();

        if (have_adiak)
            config["CALI_SERVICES_ENABLE"].append(",adiak_import");

        format.append(format.length() > 0 ? "," : "").append("print-globals");
    }

    return std::string("tree(") + format + ")";
}
