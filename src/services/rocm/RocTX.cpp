// Copyright (c) 2020, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// Caliper RocTX bindings

#include "caliper/AnnotationBinding.h"

#include "caliper/common/Attribute.h"
#include "caliper/common/Log.h"

#include <roctx.h>

#include <map>
#include <mutex>
#include <vector>

using namespace cali;

namespace
{

class RocTXBinding : public AnnotationBinding
{
    std::map< cali_id_t, std::vector<roctx_range_id_t> > m_range_map;
    std::mutex m_range_map_mutex;

    unsigned m_num_stack_errors { 0 };
    unsigned m_num_range_errors { 0 };

public:

    void on_begin(Caliper*, Channel*, const Attribute &attr, const Variant& value) {
        const char* msg = nullptr;
        std::string str; // string obj must not be deleted until end of function

        if (attr.type() == CALI_TYPE_STRING) {
            msg = static_cast<const char*>(value.data());
        } else {
            str = value.to_string();
            msg = str.c_str();
        }

        // Use range stack for nested attributes, range start/stop for others
        if (attr.is_nested()) {
            roctxRangePush(msg);
        } else {
            roctx_range_id_t roctx_id =
                roctxRangeStart(msg);

            std::lock_guard<std::mutex>
                g(m_range_map_mutex);

            m_range_map[attr.id()].push_back(roctx_id);
        }

    }

    void on_end(Caliper*, Channel*, const Attribute& attr, const Variant&) {
        if (attr.is_nested()) {
            if (roctxRangePop() < 0)
                ++m_num_stack_errors;
        } else {
            bool found = false;
            roctx_range_id_t roctx_id = 0;

            {
                std::lock_guard<std::mutex>
                    g(m_range_map_mutex);

                auto it = m_range_map.find(attr.id());

                if (it != m_range_map.end() && it->second.size() > 0) {
                    roctx_id = it->second.back();
                    found = true;
                    it->second.pop_back();
                }
            }

            if (found)
                roctxRangeStop(roctx_id);
            else
                ++m_num_range_errors;
        }
    }

    void finalize(Caliper*, Channel* channel) {
        if (m_num_range_errors > 0)
            Log(0).stream() << channel->name() << "roctx: "
                            << m_num_range_errors
                            << " range start/stop errors!"
                            << std::endl;
        if (m_num_stack_errors > 0)
            Log(0).stream() << channel->name() << "roctx: "
                            << m_num_stack_errors
                            << " region stack errors!"
                            << std::endl;
    }

    const char* service_tag() const { return "roctx"; }
};

} // namespace [anonymous]

namespace cali
{

CaliperService roctx_service { "roctx", AnnotationBinding::make_binding<RocTXBinding> };

}
