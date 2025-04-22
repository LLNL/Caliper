// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "../Services.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/common/Attribute.h"
#include "caliper/common/Log.h"

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <array>

using namespace cali;

namespace
{

inline std::array<uint64_t, 6> parse_statm(const char* buf, ssize_t max)
{
    std::array<uint64_t, 6> numbers = { 0, 0, 0, 0, 0, 0 };
    int                     nn      = 0;

    for (ssize_t n = 0; n < max && nn < 6; ++n) {
        char c = buf[n];
        if (c >= '0' && c <= '9')
            numbers[nn] = numbers[nn] * 10 + (static_cast<unsigned>(c) - static_cast<unsigned>('0'));
        else if (c == ' ')
            ++nn;
    }

    return numbers;
}

class MemstatService
{
    Attribute m_vmsize_attr;
    Attribute m_vmrss_attr;
    Attribute m_vmdata_attr;

    int m_fd;

    unsigned m_failed;

    void snapshot_cb(Caliper*, SnapshotBuilder& rec)
    {
        char    buf[80];
        ssize_t ret = pread(m_fd, buf, sizeof(buf), 0);

        if (ret < 0) {
            ++m_failed;
            return;
        }

        auto val = parse_statm(buf, ret);

        rec.append(m_vmsize_attr, cali_make_variant_from_uint(val[0]));
        rec.append(m_vmrss_attr, cali_make_variant_from_uint(val[1]));
        rec.append(m_vmdata_attr, cali_make_variant_from_uint(val[5]));
    }

    void finish_cb(Caliper*, Channel* channel)
    {
        if (m_failed > 0)
            Log(0).stream() << channel->name() << ": memstat: failed to read /proc/self/statm " << m_failed
                            << " times\n";
    }

    MemstatService(Caliper* c, int fd) : m_fd { fd }, m_failed { 0 }
    {
        m_vmsize_attr = c->create_attribute(
            "memstat.vmsize",
            CALI_TYPE_UINT,
            CALI_ATTR_SCOPE_PROCESS | CALI_ATTR_ASVALUE | CALI_ATTR_AGGREGATABLE
        );
        m_vmrss_attr = c->create_attribute(
            "memstat.vmrss",
            CALI_TYPE_UINT,
            CALI_ATTR_SCOPE_PROCESS | CALI_ATTR_ASVALUE | CALI_ATTR_AGGREGATABLE
        );
        m_vmdata_attr = c->create_attribute(
            "memstat.data",
            CALI_TYPE_UINT,
            CALI_ATTR_SCOPE_PROCESS | CALI_ATTR_ASVALUE | CALI_ATTR_AGGREGATABLE
        );
    }

public:

    static void memstat_register(Caliper* c, Channel* channel)
    {
        int fd = open("/proc/self/statm", O_RDONLY | O_NONBLOCK);

        if (fd < 0) {
            Log(0).perror(errno, "open(\"/proc/self/statm\")");
            return;
        }

        MemstatService* instance = new MemstatService(c, fd);

        channel->events().snapshot.connect([instance](Caliper* c, SnapshotView, SnapshotBuilder& rec) {
            instance->snapshot_cb(c, rec);
        });
        channel->events().finish_evt.connect([instance](Caliper* c, Channel* channel) {
            instance->finish_cb(c, channel);
            close(instance->m_fd);
            delete instance;
        });

        Log(1).stream() << channel->name() << ": registered memstat service\n";
    }
};

const char* memstat_spec = R"json(
{
    "name"        : "memstat",
    "description" : "Record process memory info from /proc/self/statm"
}
)json";

} // namespace

namespace cali
{

CaliperService memstat_service { ::memstat_spec, ::MemstatService::memstat_register };

}