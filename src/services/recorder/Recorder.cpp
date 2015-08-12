/// @file Recorder.cpp
/// @brief Caliper event recorder

#include "../CaliperService.h"

#include <Caliper.h>
#include <SigsafeRWLock.h>

#include <csv/CsvSpec.h>

#include <ContextRecord.h>
#include <Log.h>
#include <RuntimeConfig.h>

#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <ctime>
#include <iostream>
#include <iterator>
#include <mutex>
#include <fstream>
#include <random>
#include <string>
#include <vector>

using namespace cali;
using namespace std;

namespace 
{

class Recorder
{    
    static unique_ptr<Recorder>   s_instance;
    static const ConfigSet::Entry s_configdata[];

    enum class Stream { None, File, StdErr, StdOut };

    ConfigSet m_config;

    vector<RecordDescriptor>   m_record_buffer;
    vector<Variant>            m_data_buffer;

    bool                       m_buffer_can_grow;
    vector<RecordDescriptor>::size_type m_record_buffer_size;
    vector<Variant>::size_type m_data_buffer_size; 

    SigsafeRWLock m_lock;
    Stream    m_stream;
    ofstream  m_ofstream;

    // --- helpers

    string random_string(string::size_type len) {
        static std::mt19937 rgen(chrono::system_clock::now().time_since_epoch().count());
        
        static const char characters[] = "0123456789"
            "abcdefghiyklmnopqrstuvwxyz"
            "ABCDEFGHIJKLMNOPQRSTUVWXZY";

        std::uniform_int_distribution<> random(0, sizeof(characters)-2);

        string s(len, '-');

        generate_n(s.begin(), len, [&](){ return characters[random(rgen)]; });

        return s;
    }

    string create_filename() {
        char   timestring[16];
        time_t tm = chrono::system_clock::to_time_t(chrono::system_clock::now());
        strftime(timestring, sizeof(timestring), "%y%m%d-%H%M%S", localtime(&tm));

        int  pid = static_cast<int>(getpid());

        return string(timestring) + "_" + std::to_string(pid) + "_" + random_string(12) + ".cali";
    }

    void init_recorder() {
        string filename = m_config.get("filename").to_string();

        const map<string, Stream> strmap { 
            { "none",   Stream::None   },
            { "stdout", Stream::StdOut },
            { "stderr", Stream::StdErr } };

        auto it = strmap.find(filename);

        if (it == strmap.end()) {
            m_stream = Stream::None;

            if (filename.empty())
                filename = create_filename();

            string dirname = m_config.get("directory").to_string();
            struct stat s = { 0 };

            if (!dirname.empty()) {
                if (stat(dirname.c_str(), &s) < 0)
                    Log(0).stream() << "Warning: Could not open Caliper output directory " << dirname << endl;
                else
                    dirname += '/';
            }

            m_ofstream.open(dirname + filename);

            if (!m_ofstream)
                Log(0).stream() << "Could not open recording file " << filename << endl;
            else
                m_stream = Stream::File;
        } else
            m_stream = it->second;

        m_buffer_can_grow    = m_config.get("buffer_can_grow").to_bool();
        m_record_buffer_size = m_config.get("record_buffer_size").to_uint();
        m_data_buffer_size   = m_config.get("data_buffer_size").to_uint();

        m_record_buffer.reserve(m_record_buffer_size);
        m_data_buffer.reserve(m_data_buffer_size);
    }

    std::ostream& get_stream() {
        switch (m_stream) {
        case Stream::StdOut:
            return std::cout;
        case Stream::StdErr:
            return std::cerr;
        default:
            return m_ofstream;
        }
    }

    void flush_buffer() {
        const int MAX_RECORD = 16;

        int            count[MAX_RECORD];
        const Variant* data[MAX_RECORD];        

        vector<Variant>::size_type dptr = 0;

        for (const RecordDescriptor& rec : m_record_buffer) {
            assert(rec.num_entries < MAX_RECORD);

            for (unsigned n = 0; n < rec.num_entries; ++n)
                count[n] = m_data_buffer[dptr++].to_int();
            for (unsigned n = 0; n < rec.num_entries; ++n) {
                data[n]  = count[n] > 0 ? &m_data_buffer[dptr] : nullptr;
                dptr    += count[n];
            }

            CsvSpec::write_record(get_stream(), rec, count, data);
        }

        Log(1).stream() << "Wrote " << m_record_buffer.size() << " records." << endl;

        m_record_buffer.clear();
        m_data_buffer.clear();
    }

    void buffer_record(const RecordDescriptor& rec, const int* count, const Variant** data) {
        int total = rec.num_entries;

        for (unsigned n = 0; n < rec.num_entries; ++n)
            total += count[n];

        if (m_buffer_can_grow || (m_record_buffer.size() + 1   < m_record_buffer_size && 
                                  m_data_buffer.size() + total < m_data_buffer_size)) {
            for (unsigned n = 0; n < rec.num_entries; ++n)
                m_data_buffer.emplace_back(count[n]);

            for (unsigned entry = 0; entry < rec.num_entries; ++entry)
                for (int n = 0; n < count[entry]; ++n)
                    m_data_buffer.push_back(data[entry][n]);

            m_record_buffer.push_back(rec);
        } else {
            flush_buffer();
            CsvSpec::write_record(get_stream(), rec, count, data);
        }
    }

    void register_callbacks(Caliper* c) {
        auto recfn = [&](const RecordDescriptor& rec, const int* count, const Variant** data){
            m_lock.wlock();
            CsvSpec::write_record(get_stream(), rec, count, data);
            m_lock.unlock();
        };

        auto buffn = [&](const RecordDescriptor& rec, const int* count, const Variant** data){
            m_lock.wlock();
            buffer_record(rec, count, data);
            m_lock.unlock();
        };        

        if (!m_buffer_can_grow && m_record_buffer_size == 0)
            c->events().write_record.connect(recfn);
        else
            c->events().write_record.connect(buffn);

        c->events().finish_evt.connect([&](Caliper*){ m_lock.wlock(); flush_buffer(); m_lock.unlock(); });
    }

    Recorder(Caliper* c)
        : m_config { RuntimeConfig::init("recorder", s_configdata) }
    { 
        init_recorder();

        if (m_stream != Stream::None) {
            register_callbacks(c);

            Log(1).stream() << "Registered recorder service" << endl;
        }
    }

public:

    ~Recorder() 
        { }

    static void create(Caliper* c) {
        s_instance.reset(new Recorder(c));
    }
};

unique_ptr<Recorder>   Recorder::s_instance       { nullptr };

const ConfigSet::Entry Recorder::s_configdata[] = {
    { "filename", CALI_TYPE_STRING, "",
      "File name for event record stream. Auto-generated by default.",
      "File name for event record stream. Either one of\n"
      "   stdout: Standard output stream,\n"
      "   stderr: Standard error stream,\n"
      "   none:   No output,\n"
      " or a file name. By default, a filename is auto-generated.\n"
    },
    { "directory", CALI_TYPE_STRING, "",
      "Name of Caliper output directory (default: current working directory)",
      "Name of Caliper output directory (default: current working directory)"
    },
    { "record_buffer_size", CALI_TYPE_UINT, "8000",
      "Size of record buffer",
      "Size of record buffer. This is the number of records that can be buffered."
    },
    { "data_buffer_size", CALI_TYPE_UINT, "60000",
      "Size of data buffer",
      "Size of record buffer. This is the number of record entries that can be buffered."
    },
    { "buffer_can_grow", CALI_TYPE_BOOL, "true",
      "Allow record and data buffers to grow at runtime if necessary",
      "Allow record and data buffers to grow at runtime if necessary."
    },
    ConfigSet::Terminator
};

} // namespace

namespace cali
{
    CaliperService RecorderService { "recorder", { &(::Recorder::create) } };
}
