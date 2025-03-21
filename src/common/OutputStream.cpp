// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "caliper/common/OutputStream.h"

#include "SnapshotTextFormatter.h"

#include "caliper/common/Log.h"

#include <cstring>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>

// (1) MSVC has a low value of __cplusplus even though it support C++17.
// (2) std::filesystem support in libstdc++ prior to gcc 9 requires linking
// an extra libstdc++-fs library. Let's not bother with this.
#if defined(_WIN32) || (__cplusplus >= 201703L && (!defined(_GLIBCXX_RELEASE) || _GLIBCXX_RELEASE >= 9))
#define CALI_OSTREAM_USE_STD_FILESYSTEM
#endif

#ifdef CALI_OSTREAM_USE_STD_FILESYSTEM
#include <filesystem>
#else
#include <errno.h>
#include <sys/stat.h>
#endif

using namespace cali;

namespace
{

#ifdef CALI_OSTREAM_USE_STD_FILESYSTEM
bool check_and_create_directory(const std::filesystem::path& filepath)
{
    try {
        auto parent = filepath.parent_path();
        if (parent.empty())
            return true;
        bool result = std::filesystem::create_directories(parent);
        if (result) {
            Log(2).stream() << "OutputStream: created directories for " << filepath << std::endl;
        }
    } catch (std::filesystem::filesystem_error const& e) {
        Log(0).stream() << "OutputStream: create_directories failed: " << e.what() << std::endl;
        return false;
    }
    return true;
}
#else
bool check_and_create_directory(const std::string& filepath)
{
    auto pos = filepath.find_last_of('/');

    if (pos == 0 || pos == std::string::npos)
        return true;

    // Check and create parent directories
    std::string dir = filepath.substr(0, pos);

    // Check if the directory exists
    struct stat sb;
    if (stat(dir.c_str(), &sb) == -1) {
        // Doesn't exist - recursively descend and create the path
        if (errno == ENOENT) {
            if (!check_and_create_directory(dir))
                return false;

            Log(2).stream() << "OutputStream: creating directory " << dir << std::endl;

            if (mkdir(dir.c_str(), 0755) == -1) {
                Log(0).perror(errno, "OutputStream: mkdir: ") << ": " << dir << std::endl;
                return false;
            }
        } else {
            Log(0).perror(errno, "OutputStream: stat: ") << ": " << dir << std::endl;
            return false;
        }
    } else if (!S_ISDIR(sb.st_mode)) {
        Log(0).stream() << "OutputStream: " << dir << " is not a directory" << std::endl;
        return false;
    }

    return true;
}
#endif

} // namespace

struct OutputStream::OutputStreamImpl {
    StreamType type;
    Mode       mode;

    bool       is_initialized;
    std::mutex init_mutex;

#ifdef CALI_OSTREAM_USE_STD_FILESYSTEM
    std::filesystem::path filename;
#else
    std::string filename;
#endif
    std::ofstream fs;

    std::ostream* user_os;

    void init()
    {
        if (is_initialized)
            return;

        std::lock_guard<std::mutex> g(init_mutex);

        is_initialized = true;

        if (type == StreamType::File) {
            check_and_create_directory(filename);
            fs.open(filename, mode == Mode::Append ? std::ios::app : std::ios::trunc);

            if (!fs.is_open()) {
                type = StreamType::None;

                Log(0).stream() << "Could not open output stream " << filename << std::endl;
            }
        }
    }

    std::ostream* stream()
    {
        init();

        switch (type) {
        case None:
            return &fs;
        case StdOut:
            return &std::cout;
        case StdErr:
            return &std::cerr;
        case File:
            return &fs;
        case User:
            return user_os;
        }

        return &fs;
    }

    void reset()
    {
        fs.close();
        filename.clear();
        user_os        = nullptr;
        type           = StreamType::None;
        is_initialized = false;
    }

    OutputStreamImpl() : type(StreamType::None), mode(Truncate), is_initialized(false), user_os(nullptr) {}

    OutputStreamImpl(const char* name)
        : type(StreamType::None), mode(Truncate), is_initialized(false), filename(name), user_os(nullptr)
    {}
};

OutputStream::OutputStream() : mP(new OutputStreamImpl)
{}

OutputStream::~OutputStream()
{
    mP.reset();
}

OutputStream::operator bool () const
{
    return mP->is_initialized;
}

OutputStream::StreamType OutputStream::type() const
{
    return mP->type;
}

std::ostream* OutputStream::stream()
{
    return mP->stream();
}

void OutputStream::set_mode(OutputStream::Mode mode)
{
    mP->mode = mode;
}

void OutputStream::set_stream(StreamType type)
{
    mP->reset();
    mP->type = type;
}

void OutputStream::set_stream(std::ostream* os)
{
    mP->reset();

    mP->type    = StreamType::User;
    mP->user_os = os;
}

void OutputStream::set_filename(const char* filename)
{
    mP->reset();

    mP->filename = filename;
    mP->type     = StreamType::File;
}

void OutputStream::set_filename(
    const char*                           formatstr,
    const CaliperMetadataAccessInterface& db,
    const std::vector<Entry>&             rec
)
{
    mP->reset();

    if (strcmp(formatstr, "stdout") == 0)
        mP->type = StreamType::StdOut;
    else if (strcmp(formatstr, "stderr") == 0)
        mP->type = StreamType::StdErr;
    else {
        SnapshotTextFormatter formatter(formatstr);
        std::ostringstream    fnamestr;

        formatter.print(fnamestr, db, rec);

        mP->filename = fnamestr.str();
        mP->type     = StreamType::File;
    }
}
