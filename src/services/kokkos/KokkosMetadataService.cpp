//
// Created by Poliakoff, David Zoeller on 4/21/21.
//

#include "caliper/CaliperService.h"

#include "caliper/Caliper.h"
#include "../../caliper/MemoryPool.h"

#include "caliper/common/Log.h"
#include "caliper/common/RuntimeConfig.h"
#include "caliper/Annotation.h"

#include <iterator>
#include <cstdint>

#include "types.hpp"
#include "adiak.hpp"
using namespace cali;

namespace {

    class KokkosAdiak {
        static const ConfigSet::Entry s_configdata[];

        KokkosAdiak(Caliper *c, Channel *chn) {
           adiak_init(nullptr);
           adiak::user();
           adiak::launchdate();
           adiak::executablepath();
           adiak::libraries();
           adiak::cmdline();
           adiak::clustername();
           adiak::jobsize();
        }

    public:

        void declare(const char* key, const char* value){
            std::string parseme(value);
            try {
                size_t end;
                float v = std::stof(parseme,&end);
                adiak::value(key, v);
                return;
            }
            catch(std::invalid_argument){
            }
            try {
                size_t end;
                int i = std::stoi(parseme,&end);
                adiak::value(key, i);
                return;
            }
            catch(std::invalid_argument){
            }
            adiak::value(key,value);
        }
        static void kokkosadiak_register(Caliper *c, Channel *chn) {

            auto *instance = new KokkosAdiak(c, chn);
            chn->events().post_init_evt.connect(
                    [instance](Caliper *c, Channel *chn) {
                        kokkosp_callbacks.kokkosp_declare_metadata.connect([&](const char* name, const char* value){
                            instance->declare(name,value);
                        });
                    });
            chn->events().finish_evt.connect(
                    [instance](Caliper*, Channel*){
                        delete instance;
                    });

            Log(1).stream() << chn->name() << ": Registered kokkosadiak service" << std::endl;
        }
    };

    const ConfigSet::Entry KokkosAdiak::s_configdata[] = {
            ConfigSet::Terminator
    };

} // namespace [anonymous]


namespace cali {

    CaliperService kokkosadiak_service{"kokkosadiak", ::KokkosAdiak::kokkosadiak_register};

}
