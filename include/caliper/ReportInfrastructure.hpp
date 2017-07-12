// Copyright (c) 2016, Lawrence Livermore National Security, LLC.  
// Produced at the Lawrence Livermore National Laboratory.
//
// This file is part of Caliper.
// Written by David Boehme, boehme3@llnl.gov.
// LLNL-CODE-678900
// All rights reserved.
//
// For details, see https://github.com/scalability-llnl/Caliper.
// Please also see the LICENSE file for our additional BSD notice.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
//  * Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the disclaimer below.
//  * Redistributions in binary form must reproduce the above copyright notice, this list of
//    conditions and the disclaimer (as noted below) in the documentation and/or other materials
//    provided with the distribution.
//  * Neither the name of the LLNS/LLNL nor the names of its contributors may be used to endorse
//    or promote products derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS
// OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// LAWRENCE LIVERMORE NATIONAL SECURITY, LLC, THE U.S. DEPARTMENT OF ENERGY OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
// ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

/// \file  Report.cpp
/// \brief Generates text reports from Caliper snapshots on flush() events 

#ifndef CALIPER_COMMON_REPORTINFRASTRUCTURE_HPP
#define CALIPER_COMMON_REPORTINFRASTRUCTURE_HPP

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/reader/Format.h"
#include "caliper/reader/RecordSelector.h"
#include "caliper/reader/Table.h"

#include "caliper/common/Log.h"
#include "caliper/common/Node.h"
#include "caliper/common/RuntimeConfig.h"
#include "caliper/common/SnapshotTextFormatter.h"

#include <iostream>
#include <fstream>
#include <sstream>

using namespace cali;

namespace cali
{
namespace reporting
{
    
    class FileBufferStream : public std::basic_streambuf<char>{
      FILE* fp;
      public:
      FileBufferStream(FILE* m_fp) : fp(m_fp) {}
      virtual int_type overflow(int_type ch){
        fputc((char)ch, fp);
        return 0;
      }
    };

    class Reporter {
        using output_stream_type = std::ostream*;
        Caliper m_cali;
        output_stream_type m_output_stream;
        Table          m_table_writer;
        RecordSelector m_selector;

        std::vector<Entry> make_entrylist(Caliper* c, const SnapshotRecord* snapshot) {
            std::vector<Entry> list;

            if (!snapshot)
                return list;

            SnapshotRecord::Data  data(snapshot->data());
            SnapshotRecord::Sizes size(snapshot->size());

            for (size_t n = 0; n < size.n_nodes; ++n)
                list.push_back(Entry(data.node_entries[n]));
            for (size_t n = 0; n < size.n_immediate; ++n)
                list.push_back(Entry(c->get_attribute(data.immediate_attr[n]), data.immediate_data[n]));

            return list;
        }

        void process_snapshot(Caliper* c, const SnapshotRecord* snapshot) {
            SnapshotProcessFn fn(m_table_writer);
            m_selector(*c, make_entrylist(c, snapshot), fn);
        }

        void flush(Caliper* c, const SnapshotRecord* flush_info) {
            m_table_writer.flush(*c, *m_output_stream);
        }
        public:
        void report(){
          m_cali.flush(NULL);
        }
        void connectCaliper(){
                m_cali.events().flush_snapshot.connect([=](Caliper* c, const SnapshotRecord* toss, const SnapshotRecord* snapshot){
                  this->process_snapshot(c,snapshot);
                });
                m_cali.events().flush_finish_evt.connect([=](Caliper* c, const SnapshotRecord* snapshot){
                  this->flush(c,snapshot);
                });
        }
        Reporter(output_stream_type out, const std::string& attributes, const std::string& sort, const std::string& filter, Caliper c = Caliper::instance()) :
              m_cali(c),
              m_output_stream(out),
              m_table_writer(attributes,
                             sort),
              m_selector(filter)
                
            {
                connectCaliper();
            }
        Reporter(std::ostream& out, std::string attributes = "", std::string sort= "", std::string filter= "", Caliper c = Caliper::instance() ) : 
              m_cali(c),
              m_output_stream(&out),
              m_table_writer(attributes,
                             sort),
              m_selector(filter)
        {
                connectCaliper();
        }

        Reporter(FILE* fp, std::string attributes= "", std::string sort= "", std::string filter= "", Caliper c = Caliper::instance()) :
              m_cali(c),
              m_output_stream(new std::ostream(new FileBufferStream(fp))),
              m_table_writer(attributes,
                             sort),
              m_selector(filter)
        {
          connectCaliper();
        }

        ~Reporter()
            { }

    };


} // namespace reporting
} // namespace cali
#endif 
