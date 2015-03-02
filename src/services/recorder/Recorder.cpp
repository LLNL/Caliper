/// @file Recorder.cpp
/// @brief Caliper event recorder

#include "../CaliperService.h"

#include <Caliper.h>

#include <CsvSpec.h>

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

    enum class Stream { None, File, StdErr, StdOut };

    ConfigSet m_config;

    mutex     m_stream_mutex;
    Stream    m_stream;
    ofstream  m_ofstream;

    // --- helpers

    void init_recorder() {
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

    void register_callbacks(Caliper* c) {
        using std::placeholders::_1;

        auto recfn = [&](const RecordDescriptor& rec, const int* count, const Variant** data){
            lock_guard<mutex> lock(m_stream_mutex);
            CsvSpec::write_record(get_stream(), rec, count, data);
        };

        c->events().writeRecord.connect(recfn);

        auto f = [&](Caliper* c, const Attribute&){ 
            c->push_context(CALI_SCOPE_THREAD | CALI_SCOPE_PROCESS); 
        };

        c->events().beginEvt.connect(f);
        c->events().endEvt  .connect(f);
        c->events().setEvt  .connect(f);
    }

    Recorder(Caliper* c)
        : m_config { RuntimeConfig::init("recorder", s_configdata) }
    { 
        init_recorder();

        if (m_stream != Stream::None) {
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
    { "filename", CALI_TYPE_STRING, "stdout",
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
