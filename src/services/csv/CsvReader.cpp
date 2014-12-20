/// @file CsvReader.cpp
/// CsvReader implementation

#include "CsvReader.h"

#include "CsvSpec.h"

#include <fstream>

using namespace cali;
using namespace std;

struct CsvReader::CsvReaderImpl
{
    string m_filename;

    CsvReaderImpl(const string& filename)
        : m_filename { filename }
        { }

    bool read(function<void(const RecordMap&)> rec_handler) {
        ifstream is(m_filename.c_str());

        if (!is)
            return false;

        for (string line ; getline(is, line); )
            rec_handler(CsvSpec::read_record(line));

        return true;
    }
};

CsvReader::CsvReader(const string& filename)
    : mP { new CsvReaderImpl(filename) }
{ }

CsvReader::~CsvReader()
{ }

bool
CsvReader::read(function<void(const RecordMap&)> rec_handler)
{
    return mP->read(rec_handler);
}
