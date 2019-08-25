#include "caliper/cali.h"

#include <sys/types.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <unistd.h>

int main(int argc, char* argv[])
{
    CALI_CXX_MARK_FUNCTION;

    int fd = open(argv[0], O_RDONLY);

    if (fd == -1)
        return -1;
    
    char buf[16];
    if (read(fd, buf, 16) != 16)
        return -2;
    
    close(fd);
}