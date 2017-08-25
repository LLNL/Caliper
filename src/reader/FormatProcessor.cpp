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

#include "caliper/reader/Expand.h"
#include "caliper/reader/Format.h"
#include "caliper/reader/JsonFormatter.h"
#include "caliper/reader/TableFormatter.h"
#include "caliper/reader/TreeFormatter.h"

#include "caliper/common/CaliperMetadataAccessInterface.h"

#include "caliper/common/csv/CsvWriter.h"

using namespace cali;

namespace
{

const char* format_kernel_args[] = { "formatstring"    };
const char* tree_kernel_args[]   = { "path-attributes" }; 

enum FormatterID {
    Csv    = 0,
    Json   = 1,
    Expand = 2,
    Format = 3,
    Table  = 4,
    Tree   = 5
};

const QuerySpec::FunctionSignature formatters[] = {
    { FormatterID::Csv,    "csv",    0, 0, nullptr },
    { FormatterID::Json,   "json",   0, 0, nullptr },
    { FormatterID::Expand, "expand", 0, 0, nullptr },
    { FormatterID::Format, "format", 1, 1, format_kernel_args },
    { FormatterID::Table,  "table",  0, 0, nullptr },
    { FormatterID::Tree,   "tree",   0, 1, tree_kernel_args   },
    
    QuerySpec::FunctionSignatureTerminator
};

class CsvFormatter : public Formatter
{
    CsvWriter m_writer;

public:

    CsvFormatter(std::ostream& os)
        : m_writer(CsvWriter(os))
    { }

    void process_record(CaliperMetadataAccessInterface& db, const EntryList& list) {
        m_writer(db, list);
    }
};

}

struct FormatProcessor::FormatProcessorImpl
{
    std::ostream&     m_os;
    Formatter*        m_formatter;

    void create_formatter(const QuerySpec& spec) {
        if (spec.format.opt == QuerySpec::FormatSpec::Default) {
            m_formatter = new CsvFormatter(m_os);
        } else {
            switch (spec.format.formatter.id) {
            case FormatterID::Csv:
                m_formatter = new CsvFormatter(m_os);
                break;
            case FormatterID::Json:
                m_formatter = new JsonFormatter(spec);
                break;
            case FormatterID::Expand:
                m_formatter = new Expand(m_os, spec);
                break;
            case FormatterID::Format:
                // m_formatter = Format(spec);
                break;
            case FormatterID::Table:
                m_formatter = new TableFormatter(spec);
                break;
            case FormatterID::Tree:
                m_formatter = new TreeFormatter(spec);
                break;
            }
        }
    }
    
    FormatProcessorImpl(const QuerySpec& spec, std::ostream& os)
        : m_os(os),
          m_formatter(nullptr)
    {
        create_formatter(spec);
    }

    ~FormatProcessorImpl()
    {
        delete m_formatter;
    }
};


FormatProcessor::FormatProcessor(const QuerySpec& spec, std::ostream& os)
    : mP(new FormatProcessorImpl(spec, os))
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
        mP->m_formatter->flush(db, mP->m_os);
}
