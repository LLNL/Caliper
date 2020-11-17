// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// Spot.cpp
// Outputs Caliper performance results to Spot-parseable formats


#include "caliper/CaliperService.h"

#include "caliper/caliper-config.h"
#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/reader/Aggregator.h"
#include "caliper/reader/RecordSelector.h"
#include "caliper/reader/CalQLParser.h"
#include "caliper/reader/QueryProcessor.h"

#include "caliper/common/Log.h"
#include "caliper/common/Node.h"
#include "caliper/common/OutputStream.h"
#include "caliper/common/RuntimeConfig.h"
#include "caliper/common/util/split.hpp"
#ifdef CALIPER_REDUCED_CONSTEXPR_USAGE
#define OLD_GNUC __GNUC__
#undef __GNUC__
#endif

#if defined(__GNUC__) && (__GNUC__ > 7)
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wclass-memaccess"
#endif
#define RAPIDJSON_NAMESPACE cali_rapidjson
#include "rapidjson/rapidjson.h"
#include "rapidjson/document.h"
#include "rapidjson/ostreamwrapper.h"
#include "rapidjson/writer.h"
#if defined(__GNUC__) && (__GNUC__ > 7)
#    pragma GCC diagnostic pop
#endif

#ifdef CALIPER_REDUCED_CONSTEXPR_USAGE
#define __GNUC_ OLD_GNUC
#endif
#include <iostream>
#include <sstream>
#include <iterator>
#include <fstream>
#include <time.h>
using namespace cali;

namespace
{

    class Spot {

        static const ConfigSet::Entry  s_configdata[];

        int divisor;

        template<typename T>
        using List = std::vector<T>;

        using AggregationHandler = cali::Aggregator;
        using SelectionHandler = cali::RecordSelector;

        using TimeType = unsigned int;

        using SingleJsonEntryType = List<std::pair<std::string,TimeType>>;
        using JsonListType = List<SingleJsonEntryType>;

        struct AggregationDescriptor {
          std::string hierarchical_annotation_name;
          std::string metric_name;
          std::string json_location;
        };

        using AggregationDescriptorList = List<AggregationDescriptor>;

        struct QueryProcessingPipeline {
            QueryProcessingPipeline(AggregationHandler agg_in, SelectionHandler sel_in) : aggregator(agg_in), selector(sel_in) {}
            AggregationHandler aggregator;
            SelectionHandler   selector;
        };

        List<QueryProcessingPipeline> m_queries;

        std::vector<std::string> y_axes;

        AggregationDescriptorList m_annotations_and_places;
        JsonListType m_jsons;
        std::string code_version;
        std::string recorded_time;
        std::vector<std::string> title;

        void write_output_cb(Caliper* c, Channel* chn, const SnapshotRecord* flush_info) {
            int num_queries = m_queries.size();
            for (int i = 0; i < num_queries; ++i) {
                Log(2).stream() << "spot: Flushing query " << i << std::endl;

                auto &query = m_queries[i];

                c->flush(chn, flush_info,
                    [&query](CaliperMetadataAccessInterface& db, const std::vector<Entry>& rec){
                        if (query.selector.pass(db, rec)) {
                            query.aggregator.add(db, rec);
                        }
                    }
                );

                auto &json = m_jsons[i];

                query.aggregator.flush(*c,
                    [&json](CaliperMetadataAccessInterface& db,const EntryList& list) {
                        std::string name;
                        TimeType value = 0;

                        for (const Entry& e : list) {
                            if (e.is_reference()) {
                                for (const Node* node = e.node(); node; node = node->parent()) {
                                    if (db.get_attribute(node->attribute()).is_nested()) {
                                        name = node->data().to_string() + (name.empty() ? "": "/") + name;
                                    }
                                }
                            } else if (db.get_attribute(e.attribute()).name() == "inclusive#sum#time.duration") {
                                value = e.value().to_uint();
                            }
                            else{
                            }
                        }
                        if (!name.empty()) {
                            json.push_back(std::make_pair(name, value));
                        }
                    });
            }

            write_jsons(num_queries);
        }
        void write_jsons(int num_queries) {
          for(int i=0;i<num_queries;i++) {
             auto place = m_annotations_and_places[i].json_location;
             auto json = m_jsons[i];
             std::string title_string = title[i];
             std::ifstream ifs(place);
             std::string str(std::istreambuf_iterator<char>{ifs}, {});
             cali_rapidjson::Document doc;
             cali_rapidjson::Value xtic_value;
             cali_rapidjson::Value commit_value;
             xtic_value.SetString(recorded_time.c_str(),doc.GetAllocator());
             commit_value.SetString(code_version.c_str(),doc.GetAllocator());
             if(str.size() > 0){
               doc.Parse(str.c_str());
               auto& json_commits = doc["commits"];
               auto& json_times = doc["XTics"];
               json_commits.GetArray().PushBack(commit_value,doc.GetAllocator());
               json_times.GetArray().PushBack(xtic_value,doc.GetAllocator());
               for(auto datum : json){
                  std::string series_name = datum.first;
                       auto series_data = doc[series_name.c_str()].GetArray();
                       cali_rapidjson::Value arrarr;
                       arrarr.SetArray();
                       arrarr.PushBack(0,doc.GetAllocator());
                       arrarr.PushBack(((float)datum.second)/(1.0*divisor),doc.GetAllocator());
                       series_data.PushBack(arrarr,doc.GetAllocator());
               }
             }
             else {
               const char* json_string = "{\"show_exclusive\" : false}";
               std::string y_axis = y_axes[i];
               cali_rapidjson::Value y_axis_value;
               cali_rapidjson::Value title_value;
               title_value.SetString(title_string.c_str(),doc.GetAllocator());
               doc.Parse(json_string);
               y_axis_value.SetString(y_axis.c_str(), doc.GetAllocator());
               cali_rapidjson::Value series_array_create;
               series_array_create.SetArray();
               cali_rapidjson::Value commit_array_create;
               commit_array_create.SetArray();
               cali_rapidjson::Value xtic_array_create;
               xtic_array_create.SetArray();
               doc.AddMember("series",series_array_create,doc.GetAllocator());
               doc.AddMember("XTics",xtic_array_create,doc.GetAllocator());
               doc.AddMember("commits",commit_array_create,doc.GetAllocator());
               doc.AddMember("yAxis",y_axis_value,doc.GetAllocator());
               doc.AddMember("title",title_value,doc.GetAllocator());
               auto& json_commits = doc["commits"];
               auto& json_times = doc["XTics"];
               json_commits.GetArray().PushBack(commit_value,doc.GetAllocator());
               json_times.GetArray().PushBack(xtic_value,doc.GetAllocator());
               cali_rapidjson::Value& series_array = doc["series"];
               for(auto datum : json) {
                  std::string series_name = datum.first;
                  if(series_name.size()>1){
                    cali_rapidjson::Value value_series_name;
                    value_series_name.SetString(series_name.c_str(),doc.GetAllocator());
                    series_array.GetArray().PushBack(value_series_name,doc.GetAllocator());
                    cali_rapidjson::Value outarr;
                    outarr.SetArray();
                    cali_rapidjson::Value arrarr;
                    arrarr.SetArray();
                    arrarr.PushBack(0,doc.GetAllocator());
                    arrarr.PushBack(((float)datum.second)/(1.0*divisor),doc.GetAllocator());
                    outarr.PushBack(arrarr,doc.GetAllocator());
                    value_series_name.SetString(series_name.c_str(),doc.GetAllocator());
                    doc.AddMember(value_series_name,outarr,doc.GetAllocator());
                  }
               }
             }

             std::ofstream ofs(place.c_str());
             cali_rapidjson::OStreamWrapper osw(ofs);
             cali_rapidjson::Writer<cali_rapidjson::OStreamWrapper> writer(osw);
             doc.Accept(writer);
          }
        }

        Spot(Caliper* c, Channel* chn) {
            ConfigSet    config = chn->config().init("spot", s_configdata);
            std::string  config_string = config.get("config").to_string().c_str();
            divisor = config.get("time_divisor").to_int();
            code_version = config.get("code_version").to_string();
            recorded_time = config.get("recorded_time").to_string();
            if(recorded_time.size()==0) {
              time_t rawtime;
              struct tm * timeinfo;
              time (&rawtime);
              timeinfo = localtime (&rawtime);
              recorded_time = std::string(asctime(timeinfo));
            }
            std::string title_string = config.get("title").to_string();
            bool use_default_title = (title_string.size() == 0);
            if(use_default_title){
              Log(2).stream() << "Spot: using default titles for graphs\n";
            }
            std::string y_axes_string = config.get("y_axes").to_string();
            util::split(y_axes_string,':',std::back_inserter(y_axes));
            std::vector<std::string> logging_configurations;
            util::split(config_string,',',std::back_inserter(logging_configurations));
            for(const auto& log_config : logging_configurations){
              m_jsons.emplace_back(std::vector<std::pair<std::string,TimeType>>());
              std::vector<std::string> annotation_and_place;
              util::split(log_config,':',std::back_inserter(annotation_and_place));
              std::string annotation = annotation_and_place[0];
              std::string& place = annotation_and_place[1];
              std::string query = query_for_annotation(annotation);
              Log(2).stream() << "Spot: establishing query \"" <<query<<'"'<<std::endl;
              m_queries.emplace_back(create_query_processor(query));
              m_annotations_and_places.push_back({annotation,"time.duration",place});
              if(use_default_title){
                title.push_back(place);
              }
            }
            if(!use_default_title){
              util::split(title_string,',',std::back_inserter(title));
            }

        }

        QueryProcessingPipeline create_query_processor(std::string query){
           CalQLParser parser(query.c_str());
           if (parser.error()) {
               CalQLParser default_parser("SELECT *");
               QuerySpec default_spec(default_parser.spec());
               Log(0).stream() << "spot: config parse error: " << parser.error_msg() << std::endl;
               return QueryProcessingPipeline(Aggregator(default_spec),RecordSelector(default_spec));
           }
           QuerySpec    spec(parser.spec());
           return { Aggregator(spec), RecordSelector(spec) };
        }

        static std::string query_for_annotation(std::string grouping, std::string metric = "inclusive_sum(sum#time.duration)"){
          std::string end_grouping = "event.end#"+grouping;
          return "SELECT " + grouping+","+metric+",* WHERE " +grouping;
        }

    public:

        ~Spot()
            {
            }

        static void create(Caliper* c, Channel* chn) {
            Spot* instance = new Spot(c, chn);

            chn->events().write_output_evt.connect(
                [instance](Caliper* c, Channel* chn, const SnapshotRecord* info){
                    instance->write_output_cb(c, chn, info);
                });
            chn->events().finish_evt.connect(
                [instance](Caliper*, Channel*){
                    delete instance;
                });

            Log(1).stream() << "["<<chn->name() << "] : Registered Spot service" << std::endl;
        }
    };

    const ConfigSet::Entry  Spot::s_configdata[] = {
        { "config", CALI_TYPE_STRING, "function:default.json",
          "Attribute:Filename pairs in which to dump Spot data",
          "Attribute:Filename pairs in which to dump Spot data\n"
          "Example: function:testname.json,physics_package:packages.json"
          " or a file name.\n"
        },
        { "recorded_time", CALI_TYPE_STRING, "",
          "Time to use for this version of the code",
          "Time to use for this version of the code"
        },
        { "code_version", CALI_TYPE_STRING, "unspecified",
          "Version number (or git hash) to represent this run of the code",
          "Version number (or git hash) to represent this run of the code"
        },
        { "time_divisor", CALI_TYPE_INT, "1000000",
          "Caliper records time in microseconds, this is what we divide by to get time in your units",
          "Caliper records time in microseconds, this is what we divide by to get time in your units. 1000 if you record in milliseconds, 1000000 if seconds"
        },
        { "y_axes", CALI_TYPE_STRING, "microseconds",
          "If this is the first time Spot has seen a test, tell it what Y Axis to display on the resulting graphs. If multiple graphs, separate entries with a colon (:)",
          "If this is the first time Spot has seen a test, tell it what Y Axis to display on the resulting graphs. If multiple graphs, separate entries with a colon (:)"},
        { "title", CALI_TYPE_INT, "",
          "Title for this test",
          "Title for this test"
        },
        ConfigSet::Terminator
    };

} // namespace

namespace cali
{
    CaliperService spot_service { "spot", ::Spot::create };
}

