/// @file Recorder.cpp
/// @brief Caliper event recorder

#include "../CaliperService.h"

#include <Caliper.h>

#include <ContextRecord.h>
#include <Log.h>
#include <RuntimeConfig.h>

#include <iostream>
#include <mutex>
#include <fstream>
#include <string>
#include <unordered_set>

using namespace cali;
using namespace std;

namespace 
{

class Recorder
{    
    static unique_ptr<Recorder>   s_instance;
    static const ConfigSet::Entry s_configdata[];

    enum class Format { None, Text, Unpacked };
    enum class Stream { None, File, StdErr, StdOut };

    ConfigSet m_config;

    mutex     m_active_envs_mutex;
    unordered_set<cali_id_t> m_active_envs;

    mutex     m_stream_mutex;
    Stream    m_stream;
    Format    m_format;
    ofstream  m_ofstream;

    // --- helpers

    void init_recorder() {
        {
            string fmtname = m_config.get("format").to_string();

            const map<string, Format> fmtmap { 
                { "none",     Format::None     },
                { "text",     Format::Text     },
                { "unpacked", Format::Unpacked } };

            auto it = fmtmap.find(fmtname);

            if (it == fmtmap.end()) {
                Log(0).stream() << "Invalid recorder format name: \" " << fmtname << "\"";

                m_format = Format::None;
            } else
                m_format = it->second;
        }

        {
            string strname = m_config.get("filename").to_string();

            const map<string, Stream> strmap { 
                { "none",   Stream::None   },
                { "stdout", Stream::StdOut },
                { "stderr", Stream::StdErr } };

            auto it = strmap.find(strname);

            if (it == strmap.end()) {
                m_stream = Stream::File;

                m_ofstream.open(strname);

                if (!m_ofstream)
                    Log(0).stream() << "Could not open recording file " << strname << endl;
            } else
                m_stream = it->second;
        }
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


    // writer 

    void write(const uint64_t buf[], size_t size) {
        ostream& str { get_stream() };

        switch (m_format) {
        case Format::Text:
            m_stream_mutex.lock();
            for (size_t p = 0; (p+1) < size; p += 2)
                str << buf[p] << ':' << buf[p+1] << ' ';
            str << endl;
            m_stream_mutex.unlock();

            break;
        case Format::Unpacked:
            vector<RecordMap> rec { Caliper::instance()->unpack(buf, size) };

            m_stream_mutex.lock();
            for (auto const &q : rec)
                str << q << '\n';
            str << endl;
            m_stream_mutex.unlock();

            break;
        }
    }


    // record callback

    void record(Caliper* c, cali_id_t env) {
        // prevent recursion from set()/begin() calls made by other services on get_context()
        {
            lock_guard<mutex> lock(m_active_envs_mutex);

            if (m_active_envs.count(env))
                return;

            m_active_envs.insert(env);
        }

        uint64_t buf[40];
        size_t   s = c->get_context(env, buf, 40);

        write(buf, s);
        
        m_active_envs_mutex.lock();
        m_active_envs.erase(env);
        m_active_envs_mutex.unlock();
    }

    void register_callbacks(Caliper* c) {
        using std::placeholders::_1;

        auto f = [&](Caliper* c, cali_id_t env, const Attribute&){ record(c, env); };

        c->events().beginEvt.connect(f);
        c->events().endEvt  .connect(f);
        c->events().setEvt  .connect(f);
    }

    Recorder(Caliper* c)
        : m_config { RuntimeConfig::init("recorder", s_configdata) }
    { 
        init_recorder();

        if (m_stream != Stream::None && m_format != Format::None) {
            register_callbacks(c);

            Log(2).stream() << "Registered recorder service" << endl;
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
    { "format", CALI_TYPE_STRING, "none",
      "Data recorder format",
      "Data recorder format. Possible values are\n"
      "  txt:      Text format\n"
      "  unpacked: Unpacked text format (high overhead)\n"
      "  none:     Disable data recording"
    },
    { "filename", CALI_TYPE_STRING, "none",
      "File name for event record stream",
      "File name for event record stream. Either one of\n"
      "   stdout: Standard output stream,\n"
      "   stderr: Standard error stream,\n"
      "   none:   No output,\n"
      " or a file name."
    },
    ConfigSet::Terminator
};

} // namespace

namespace cali
{
    CaliperService RecorderService { "recorder", { &(::Recorder::create) } };
}
