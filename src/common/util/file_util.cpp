// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "file_util.h"

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

#include <algorithm>
#include <chrono>
#include <random>

namespace
{

// --- helpers

std::string random_string(std::string::size_type len)
{
    static std::mt19937 rgen(std::chrono::system_clock::now().time_since_epoch().count());

    static const char characters[] =
        "0123456789"
        "abcdefghiyklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXZY";

    std::uniform_int_distribution<> random(0, sizeof(characters) - 2);
    std::string s(len, '-');

    generate_n(s.begin(), len, [&]() { return characters[random(rgen)]; });

    return s;
}

} // namespace

namespace cali
{

namespace util
{

std::string create_filename(const char* ext)
{
    char   timestring[16];
    time_t tm = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    strftime(timestring, sizeof(timestring), "%y%m%d-%H%M%S", localtime(&tm));

    int pid = static_cast<int>(getpid());

    return std::string(timestring) + "_" + std::to_string(pid) + "_" + random_string(12) + ext;
}

} // namespace util

} // namespace cali
