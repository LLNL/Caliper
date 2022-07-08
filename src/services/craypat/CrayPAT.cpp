// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// Caliper RocTX bindings

#include "caliper/AnnotationBinding.h"

#include "caliper/common/Attribute.h"
#include "caliper/common/Log.h"

#include <pat_api.h>

#include <cstdlib>

#include <fstream>
#include <vector>

extern char** environ;

using namespace cali;

extern "C"
{

void *sbrk(__intptr_t increment);
void __pat_init (int, const char *[ ], const char *[ ], const void *, const void *);
void __pat_exit (void);

}

namespace
{

std::vector<std::string> read_cmdline()
{
    std::vector<std::string> ret;

    std::ifstream f("/proc/self/cmdline");
    std::string s;

    while (f) {
        char c = f.get();

        if (f.eof() || c == '\0') {
            if (s.size() > 0) {
                ret.emplace_back(std::move(s));
                s.clear();
            }
        } else {
            s.push_back(c);
        }
    }

    return ret;
}

class CrayPATBinding : public AnnotationBinding
{
    std::vector<std::string> m_args;
    std::vector<const char*> m_argv;

public:

    void on_begin(Caliper*, Channel*, const Attribute &attr, const Variant& value) {
        if (attr.is_nested() && attr.type() == CALI_TYPE_STRING)
            PAT_region_push(static_cast<const char*>(value.data()));
    }

    void on_end(Caliper*, Channel*, const Attribute& attr, const Variant& value) {
        if (attr.is_nested() && attr.type() == CALI_TYPE_STRING)
            PAT_region_pop(static_cast<const char*>(value.data()));
    }

    void initialize(Caliper*, Channel* channel) {
        m_args = read_cmdline();

        if (m_args.empty()) {
            Log(0).stream()
                << channel->name()
                << ": craypat: Unable to initialize CrayPAT: cannot read command line"
                << std::endl;

            return;
        }

        m_argv.resize(m_args.size());
        for (size_t i = 0; i < m_args.size(); ++i)
            m_argv[i] = m_args[i].data();

        m_argv.push_back(nullptr); // somehow craypat segfaults without this

        putenv(const_cast<char*>("PAT_RT_CALLSTACK_MODE=trace"));
        putenv(const_cast<char*>("PAT_RT_EXPERIMENT=trace"));

        __pat_init(static_cast<int>(m_argv.size()-1), m_argv.data(), const_cast<const char**>(environ), __builtin_frame_address (0), sbrk (0L));

        Log(1).stream() << channel->name() << ": craypat: CrayPAT initialized\n";
    }

    void finalize(Caliper*, Channel* channel) {
        Log(1).stream() << channel->name() << ": craypat: Closing CrayPAT\n";
        __pat_exit();
    }

    const char* service_tag() const { return "craypat"; }
};

} // namespace [anonymous]

namespace cali
{

CaliperService craypat_service { "craypat", AnnotationBinding::make_binding<CrayPATBinding> };

}
