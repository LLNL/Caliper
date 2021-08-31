// Test CollectiveOutputChannel

#include <caliper/cali.h>
#include <caliper/cali-mpi.h>

#include <caliper/CollectiveOutputChannel.h>

#include <mpi.h>

#include <iostream>
#include <tuple>

int main(int argc, char* argv[])
{
    MPI_Init(&argc, &argv);

    int rank = 0;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    std::shared_ptr<cali::CollectiveOutputChannel> channel;
    std::string errmsg;

    std::tie(channel, errmsg) = cali::make_collective_output_channel(argc > 1 ? argv[1] : "");

    if (!channel) {
        std::cerr << "Caliper error: " << errmsg << std::endl;
        MPI_Abort(MPI_COMM_WORLD, -1);
    }

    channel->start();

    {
        CALI_CXX_MARK_FUNCTION;

        MPI_Barrier(MPI_COMM_WORLD);
    }

    channel->stop();

    if (argc > 2 && strcmp(argv[2], "channel_defined_stream") == 0)
        channel->collective_flush(MPI_COMM_WORLD);
    else
        channel->collective_flush(std::cout, MPI_COMM_WORLD);

    MPI_Finalize();
}