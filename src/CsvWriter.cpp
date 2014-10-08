/// @file CsvWriter.cpp
/// CsvWriter implementation

#include "CsvWriter.h"

#include "Attribute.h"
#include "CsvSpec.h"
#include "Query.h"

using namespace cali;
using namespace std;


//
// --- CsvWriter interface
// 

void CsvWriter::write_attribute(ostream& os, const Attribute& attr)
{
    os << attr.id() << m_sep;

    CaliperCsvSpec::write_type(os, attr.type());
    os << m_sep;

    CaliperCsvSpec::write_properties(os, attr.properties());
    os << m_sep;

    CaliperCsvSpec::write_string(os, attr.name());
    os << endl;
}

void CsvWriter::write_node(ostream& os, const NodeQuery& q)
{
    // tree info
    for ( ctx_id_t i : { q.id(), q.parent(), q.first_child(), q.next_sibling() } ) {
        if (i != CTX_INV_ID)
            os << i;

        os << m_sep;
    }

    // attribute / type info
    os << q.attribute() << m_sep;

    CaliperCsvSpec::write_type(os, q.type());
    os << m_sep;

    // data
    CaliperCsvSpec::write_data(os, q.type(), q.data(), q.size());
    os << endl;
}
