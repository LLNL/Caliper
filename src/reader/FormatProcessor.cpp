// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "caliper/reader/FormatProcessor.h"

#include "Expand.h"
#include "JsonFormatter.h"
#include "JsonSplitFormatter.h"
#include "TableFormatter.h"
#include "TreeFormatter.h"
#include "UserFormatter.h"

#include "caliper/reader/CaliWriter.h"

#include "caliper/common/CaliperMetadataAccessInterface.h"
#include "caliper/common/OutputStream.h"

using namespace cali;

namespace
{

const char* format_kernel_args[] = { "format", "title" };
const char* tree_kernel_args[]   = { "path-attributes", "column-width", "print-globals" };
const char* table_kernel_args[]  = { "column-width", "print-globals" };
const char* json_kernel_args[]   = { "object", "pretty", "quote-all", "separate-nested", "records", "split" };

enum FormatterID { Cali = 0, Json = 1, Expand = 2, Format = 3, Table = 4, Tree = 5, JsonSplit = 6 };

const QuerySpec::FunctionSignature formatters[] = { { FormatterID::Cali, "cali", 0, 0, nullptr },
                                                    { FormatterID::Json, "json", 0, 6, json_kernel_args },
                                                    { FormatterID::Expand, "expand", 0, 0, nullptr },
                                                    { FormatterID::Format, "format", 1, 2, format_kernel_args },
                                                    { FormatterID::Table, "table", 0, 2, table_kernel_args },
                                                    { FormatterID::Tree, "tree", 0, 3, tree_kernel_args },
                                                    { FormatterID::JsonSplit, "json-split", 0, 0, nullptr },

                                                    QuerySpec::FunctionSignatureTerminator };

class CaliFormatter : public Formatter
{
    CaliWriter m_writer;

public:

    CaliFormatter(OutputStream& os) : m_writer(CaliWriter(os)) {}

    void process_record(CaliperMetadataAccessInterface& db, const EntryList& list)
    {
        m_writer.write_snapshot(db, list);
    }

    void flush(CaliperMetadataAccessInterface& db, std::ostream&) { m_writer.write_globals(db, db.get_globals()); }
};

} // namespace

struct FormatProcessor::FormatProcessorImpl {
    Formatter*   m_formatter;
    OutputStream m_stream;

    void create_formatter(const QuerySpec& spec)
    {
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
                m_formatter = new JsonSplitFormatter(spec);
                break;
            }
        }
    }

    FormatProcessorImpl(OutputStream& stream, const QuerySpec& spec) : m_formatter(nullptr), m_stream(stream)
    {
        create_formatter(spec);
    }

    ~FormatProcessorImpl() { delete m_formatter; }
};

FormatProcessor::FormatProcessor(const QuerySpec& spec, OutputStream& stream)
    : mP(new FormatProcessorImpl(stream, spec))
{}

FormatProcessor::~FormatProcessor()
{
    mP.reset();
}

const QuerySpec::FunctionSignature* FormatProcessor::formatter_defs()
{
    return ::formatters;
}

void FormatProcessor::process_record(CaliperMetadataAccessInterface& db, const EntryList& rec)
{
    if (mP->m_formatter)
        mP->m_formatter->process_record(db, rec);
}

void FormatProcessor::flush(CaliperMetadataAccessInterface& db)
{
    if (mP->m_formatter) {
        std::ostream* os = mP->m_stream.stream();
        mP->m_formatter->flush(db, *os);
    }
}
