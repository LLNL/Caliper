// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "caliper/reader/CaliReader.h"

#include "caliper/reader/CaliperMetadataDB.h"

#include "caliper/common/Log.h"
#include "caliper/common/StringConverter.h"

#include <algorithm>
#include <iostream>
#include <fstream>
#include <vector>

using namespace cali;
using namespace std;

namespace
{

vector<string> split(const string& line, char sep, bool keep_escape = false) {
    vector<string> vec;
    vec.reserve(8);

    string str;
    str.reserve(line.size());

    bool   escaped = false;

    for (auto it = line.begin(); it != line.end(); ++it) {
        if (!escaped && *it == '\\') {
            escaped = true;

            if (keep_escape)
                str.push_back('\\');
        } else if (!escaped && *it == sep) {
            vec.emplace_back(std::move(str));
            str.clear();
            str.reserve(line.size());
        } else {
            str.push_back(*it);
            escaped = false;
        }
    }

    vec.emplace_back(std::move(str));

    return vec;
}

void process_node(const string& line, const vector<string>& entries, CaliperMetadataDB& db, IdMap& idmap, NodeProcessFn node_proc)
{
    cali_id_t node_id = CALI_INV_ID;
    cali_id_t attr_id = CALI_INV_ID;
    cali_id_t prnt_id = CALI_INV_ID;
    string data;

    for (const string& s : entries) {
        vector<string> keyval = split(s, '=', false);

        if (keyval.size() > 1) {
            if      (keyval[0] == "id"    )
                node_id = StringConverter(keyval[1]).to_uint();
            else if (keyval[0] == "attr"  )
                attr_id = StringConverter(keyval[1]).to_uint();
            else if (keyval[0] == "parent")
                prnt_id = StringConverter(keyval[1]).to_uint();
            else if (keyval[0] == "data"  )
                data    = std::move(keyval[1]);
        }
    }

    const Node* node = db.merge_node(node_id, attr_id, prnt_id, data, idmap);

    if (node)
        node_proc(db, node);
    else
        Log(0).stream() << "CaliReader: Invalid node record: " << line << endl;
}

void process_snapshot(const string& line, const vector<string>& entries, CaliperMetadataDB& db, IdMap& idmap, SnapshotProcessFn snap_proc)
{
    vector<string> refs;
    vector<string> attr;
    vector<string> data;

    for (const string& s : entries) {
        vector<string> keyval = split(s, '=', false);

        if (keyval.size() > 1) {
            if        (keyval[0] == "ref" ) {
                keyval.erase(keyval.begin());
                refs.swap(keyval);
            } else if (keyval[0] == "attr") {
                keyval.erase(keyval.begin());
                attr.swap(keyval);
            } else if (keyval[0] == "data") {
                keyval.erase(keyval.begin());
                data.swap(keyval);
            }
        }
    }

    if (attr.size() != data.size())
        Log(0).stream() << "CaliReader: attr/data length mismatch: " << line << endl;

    EntryList rec;
    rec.reserve(refs.size() + attr.size());

    for (const string& s : refs)
        rec.push_back(db.merge_entry(StringConverter(s).to_uint(), idmap));
    for (size_t i = 0; i < min(attr.size(), data.size()); ++i)
        rec.push_back(db.merge_entry(StringConverter(attr[i]).to_uint(), data[i], idmap));

    snap_proc(db, rec);
}

void process_globals(const string& line, const vector<string>& entries, CaliperMetadataDB& db, IdMap& idmap)
{
    vector<string> refs;
    vector<string> attr;
    vector<string> data;

    for (const string& s : entries) {
        vector<string> keyval = split(s, '=', false);

        if (keyval.size() > 1) {
            if        (keyval[0] == "ref" ) {
                keyval.erase(keyval.begin());
                refs.swap(keyval);
            } else if (keyval[0] == "attr") {
                keyval.erase(keyval.begin());
                attr.swap(keyval);
            } else if (keyval[0] == "data") {
                keyval.erase(keyval.begin());
                data.swap(keyval);
            }
        }
    }

    if (attr.size() != data.size())
        Log(0).stream() << "CaliReader: attr/data length mismatch: " << line << endl;

    for (const string& s : refs)
        db.merge_global(StringConverter(s).to_uint(), idmap);
    for (size_t i = 0; i < min(attr.size(), data.size()); ++i)
        db.merge_global(StringConverter(attr[i]).to_uint(), data[i], idmap);
}

void read_record(const string& line, CaliperMetadataDB& db, IdMap& idmap, NodeProcessFn node_proc, SnapshotProcessFn snap_proc)
{
    vector<string> entries = split(line, ',', true /* keep escape */);

    auto it = find_if(entries.begin(), entries.end(),
                           [](const string& s) {
                          return s.compare(0, 6, "__rec=") == 0;
                           });

    if (it == entries.end()) {
        Log(0).stream() << "Invalid CSV entry: " << line << endl;
        return;
    }

    if      (it->compare(6, string::npos, "ctx"    ) == 0)
        process_snapshot(line, entries, db, idmap, snap_proc);
    else if (it->compare(6, string::npos, "node"   ) == 0)
        process_node    (line, entries, db, idmap, node_proc);
    else if (it->compare(6, string::npos, "globals") == 0)
        process_globals (line, entries, db, idmap);
    else
        Log(0).stream() << "CaliReader: Invalid record: " << line << endl;
}

} // namespace [anonymous]

struct CaliReader::CaliReaderImpl
{
    string m_filename;

    CaliReaderImpl(const string& filename)
        : m_filename { filename }
        { }

    bool read(CaliperMetadataDB& db, NodeProcessFn node_proc, SnapshotProcessFn snap_proc) {
        IdMap idmap;

        if (m_filename.empty()) {
            // empty file: read from stdin

            for (string line ; getline(std::cin, line); )
                ::read_record(line, db, idmap, node_proc, snap_proc);
        } else {
            // read from file

            ifstream is(m_filename.c_str());

            if (!is)
                return false;

            for (string line ; getline(is, line); )
                ::read_record(line, db, idmap, node_proc, snap_proc);
        }

        return true;
    }
};

CaliReader::CaliReader(const string& filename)
    : mP { new CaliReaderImpl(filename) }
{ }

CaliReader::~CaliReader()
{ }

bool
CaliReader::read(CaliperMetadataDB& db, NodeProcessFn node_proc, SnapshotProcessFn snap_proc)
{
    return mP->read(db, node_proc, snap_proc);
}
