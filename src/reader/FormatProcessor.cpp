// Copyright (c) 2017, Lawrence Livermore National Security, LLC.  
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

#include "caliper/reader/FormatProcessor.h"

#include "caliper/reader/CaliWriter.h"
#include "caliper/reader/Expand.h"
#include "caliper/reader/JsonFormatter.h"
#include "caliper/reader/JsonSplitFormatter.h"
#include "caliper/reader/TableFormatter.h"
#include "caliper/reader/TreeFormatter.h"
#include "caliper/reader/UserFormatter.h"

#include "caliper/common/CaliperMetadataAccessInterface.h"
#include "caliper/common/OutputStream.h"

using namespace cali;

namespace
{

const char* format_kernel_args[] = { "format", "title" };
const char* tree_kernel_args[]   = { "path-attributes" }; 
const char* json_kernel_args[]   = { "split", "pretty", "quote-all" }; 

enum FormatterID {
    Cali        = 0,
    Json        = 1,
    Expand      = 2,
    Format      = 3,
    Table       = 4,
    Tree        = 5,
    JsonSplit   = 6
};

const QuerySpec::FunctionSignature formatters[] = {
    { FormatterID::Cali,      "cali",       0, 0, nullptr },
    { FormatterID::Cali,      "csv",        0, 0, nullptr }, // keep old "csv" name for backwards compatibility
    { FormatterID::Json,      "json",       0, 3, json_kernel_args },
    { FormatterID::Expand,    "expand",     0, 0, nullptr },
    { FormatterID::Format,    "format",     1, 2, format_kernel_args },
    { FormatterID::Table,     "table",      0, 0, nullptr },
    { FormatterID::Tree,      "tree",       0, 1, tree_kernel_args   },
    { FormatterID::JsonSplit, "json-split", 0, 0, nullptr },
    
    QuerySpec::FunctionSignatureTerminator
};

class CaliFormatter : public Formatter
{
    CaliWriter m_writer;

public:

    CaliFormatter(OutputStream& os)
        : m_writer(CaliWriter(os))
    { }

    void process_record(CaliperMetadataAccessInterface& db, const EntryList& list) {
        m_writer.write_snapshot(db, list);
    }

    void flush(CaliperMetadataAccessInterface& db, std::ostream&) {
        m_writer.write_globals(db, db.get_globals());
    }
};

}

struct FormatProcessor::FormatProcessorImpl
{
    Formatter*   m_formatter;
    OutputStream m_stream;

    void create_formatter(const QuerySpec& spec) {
        if (spec.format.opt == QuerySpec::FormatSpec::Default) {
            m_formatter = new CaliFormatter(m_stream);
        } else {
            switch (spec.format.formatter.id) {
            case FormatterID::Cali:
                m_formatter = new CaliFormatter(m_stream);
                break;
            case FormatterID::Json:
                m_formatter = new JsonFormatter(m_stream, spec);
                break;
            case FormatterID::Expand:
                m_formatter = new Expand(m_stream, spec);
                break;
            case FormatterID::Format:
                m_formatter = new UserFormatter(m_stream, spec);
                break;
            case FormatterID::Table:
                m_formatter = new TableFormatter(spec);
                break;
            case FormatterID::Tree:
                m_formatter = new TreeFormatter(spec);
                break;
            case FormatterID::JsonSplit:
                m_formatter = new JsonSplitFormatter(m_stream, spec);
                break;
            }
        }
    }
    
    FormatProcessorImpl(OutputStream& stream, const QuerySpec& spec)
        : m_formatter(nullptr), m_stream(stream)
    {
        create_formatter(spec);
    }

    ~FormatProcessorImpl()
    {
        delete m_formatter;
    }
};


FormatProcessor::FormatProcessor(const QuerySpec& spec, OutputStream& stream)
    : mP(new FormatProcessorImpl(stream, spec))
{ }

FormatProcessor::~FormatProcessor()
{
    mP.reset();
}

const QuerySpec::FunctionSignature*
FormatProcessor::formatter_defs()
{
    return ::formatters;
}

void
FormatProcessor::process_record(CaliperMetadataAccessInterface& db, const EntryList& rec)
{
    if (mP->m_formatter)
        mP->m_formatter->process_record(db, rec);
}

void
FormatProcessor::flush(CaliperMetadataAccessInterface& db)
{
    if (mP->m_formatter)
        mP->m_formatter->flush(db, mP->m_stream.stream());
}
