#include "caliper/cali.h"
#include "caliper/cali-manager.h"

#include <sys/types.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <unistd.h>

int main(int argc, char* argv[])
{
    const char* cfg = argc > 1 ? argv[1] : "";

    cali::ConfigManager mgr;
    mgr.set_default_parameter("aggregate_across_ranks", "false");

    if (!mgr.add(cfg)) {
        std::cerr << "ci_test_io: error: " << mgr.error_msg() << std::endl;
        return 1;
    }

    mgr.start();

    CALI_MARK_FUNCTION_BEGIN;

    int fd = open(argv[0], O_RDONLY);

    if (fd == -1)
        return 2;

    char buf[16];
    if (read(fd, buf, 16) != 16)
        return 3;

    close(fd);

    CALI_MARK_FUNCTION_END;

    mgr.flush();
}
