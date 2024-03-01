// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "caliper/reader/CaliReader.h"

#include "caliper/reader/CaliperMetadataDB.h"

#include "caliper/common/Log.h"
#include "caliper/common/StringConverter.h"

#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>

using namespace cali;
using namespace std;

namespace
{

inline void
read_word_inplace(std::istream& is, std::string& w)
{
    w.clear();
    bool escape = false;

    while (is.good()) {
        char c = is.get();

        if (!escape && c == '\\') {
            escape = true;
        } else if (!escape && (c == '=' || c == ',' || c == '\n')) {
            is.unget();
            break;
        } else if (escape && c == 'n') {
            w.push_back('\n');
            escape = false;
        } else if (is.good()) {
            w.push_back(c);
            escape = false;
        }
    }
}

inline std::string
read_word(std::istream& is)
{
    std::string w;
    w.reserve(32);
    read_word_inplace(is, w);
    return w;
}

inline std::vector<cali_id_t>
read_id_list(std::istream& is)
{
    std::vector<cali_id_t> ret;
    ret.reserve(8);

    std::string w;
    w.reserve(16);
    char c = 0;

    do {
        read_word_inplace(is, w);
        ret.push_back(std::stoull(w, nullptr, 10));
        c = is.get();
    } while (is.good() && c == '=');

    if (is.good())
        is.unget();

    return ret;
}

inline std::vector<std::string>
read_string_list(std::istream& is)
{
    std::vector<std::string> ret;
    ret.reserve(8);

    char c = 0;

    do {
        ret.emplace_back(read_word(is));
        c = is.get();
    } while (is.good() && c == '=');

    if (is.good())
        is.unget();

    return ret;
}

} // namespace [anonymous]

struct CaliReader::CaliReaderImpl
{
    bool m_error;
    std::string m_error_msg;
    unsigned int m_num_read;

    CaliReaderImpl()
        : m_error { false }
        { }

    void set_error(const std::string& msg) {
        m_error = true;
        m_error_msg = msg;
        std::cerr << msg << std::endl;
    }

    void read_node(std::istream& is, CaliperMetadataDB& db, IdMap& idmap, NodeProcessFn node_proc)
    {
        std::string attr_id_str;
        std::string node_id_str;
        std::string prnt_id_str;
        std::string data_str;

        std::string key;
        key.reserve(8);
        char c = 0;

        do {
            read_word_inplace(is, key);
            c = is.get();

            if (c != '=')
                break;

            if (key == "attr")
                attr_id_str = read_word(is);
            else if (key == "data")
                data_str = read_word(is);
            else if (key == "id")
                node_id_str = read_word(is);
            else if (key == "parent")
                prnt_id_str = read_word(is);
            else
                read_word(is); // unknown key

            c = is.get();
        } while (is.good() && c == ',');

        if (node_id_str.empty() || attr_id_str.empty()) {
            set_error("Node or attribute ID missing in node record");
            return;
        }

        cali_id_t attr_id = std::stoull(attr_id_str, nullptr, 10);
        cali_id_t node_id = std::stoull(node_id_str, nullptr, 10);
        cali_id_t prnt_id = CALI_INV_ID;

        if (!prnt_id_str.empty())
            prnt_id = std::stoull(prnt_id_str, nullptr, 10);

        const Node* node = db.merge_node(node_id, attr_id, prnt_id, data_str, idmap);

        if (node)
            node_proc(db, node);
        else
            set_error("Invalid node record");
    }

    void read_snapshot(std::istream& is, CaliperMetadataDB& db, IdMap& idmap, SnapshotProcessFn snap_proc)
    {
        std::vector<cali_id_t>   refs;
        std::vector<cali_id_t>   attr;
        std::vector<std::string> data;

        std::string key;
        key.reserve(8);
        char c = 0;

        do {
            read_word_inplace(is, key);
            c = is.get();

            if (c != '=')
                break;

            if (key == "ref")
                refs = read_id_list(is);
            else if (key == "attr")
                attr = read_id_list(is);
            else if (key == "data")
                data = read_string_list(is);
            else
                read_string_list(is);

            c = is.get();
        } while (is.good() && c == ',');

        if (attr.size() != data.size())
            set_error("attr / data size mismatch");

        std::vector<Entry> rec;
        rec.reserve(refs.size() + std::min(attr.size(), data.size()));

        for (cali_id_t id : refs)
            rec.push_back(db.merge_entry(id, idmap));
        for (size_t i = 0; i < min(attr.size(), data.size()); ++i)
            rec.push_back(db.merge_entry(attr[i], data[i], idmap));

        snap_proc(db, rec);
    }

    void read_globals(std::istream& is, CaliperMetadataDB& db, IdMap& idmap)
    {
        std::vector<cali_id_t>   refs;
        std::vector<cali_id_t>   attr;
        std::vector<std::string> data;

        char c = 0;

        do {
            std::string key = read_word(is);
            c = is.get();

            if (c != '=')
                break;

            if (key == "ref")
                refs = read_id_list(is);
            else if (key == "attr")
                attr = read_id_list(is);
            else if (key == "data")
                data = read_string_list(is);
            else
                read_string_list(is);

            c = is.get();
        } while (is.good() && c == ',');

        if (attr.size() != data.size())
            set_error("attr / data size mismatch");

        std::vector<Entry> rec;
        rec.reserve(refs.size() + std::min(attr.size(), data.size()));

        for (cali_id_t id : refs)
            db.merge_global(id, idmap);
        for (size_t i = 0; i < min(attr.size(), data.size()); ++i)
            db.merge_global(attr[i], data[i], idmap);
    }

    void read_record(std::istream& is, CaliperMetadataDB& db, IdMap& idmap, NodeProcessFn node_proc, SnapshotProcessFn snap_proc)
    {
        std::string w = ::read_word(is);
        char c = is.get();

        if (w != "__rec" || c != '=') {
            set_error("Missing \"__rec=\" entry in input record");
            return;
        }

        read_word_inplace(is, w);
        c = is.get();

        if (c != ',') {
            set_error("Expected ',' after __rec entry");
            return;
        }

        if (w == "node") {
            read_node(is, db, idmap, node_proc);
        } else if (w == "ctx") {
            read_snapshot(is, db, idmap, snap_proc);
        } else if (w == "globals") {
            read_globals(is, db, idmap);
        } else {
            set_error(std::string("Unknown record type") + w);
        }
    }

    void read(std::istream& is, CaliperMetadataDB& db, NodeProcessFn node_proc, SnapshotProcessFn snap_proc) {
        IdMap idmap;

        for (std::string line; std::getline(is, line); ) {
            std::istringstream iss(line);
            read_record(iss, db, idmap, node_proc, snap_proc);
        }
    }
};

CaliReader::CaliReader()
    : mP { new CaliReaderImpl() }
{ }

CaliReader::~CaliReader()
{ }

bool
CaliReader::error() const
{
    return mP->m_error;
}

std::string
CaliReader::error_msg() const
{
    return mP->m_error_msg;
}

void
CaliReader::read(std::istream& is, CaliperMetadataDB& db, NodeProcessFn node_proc, SnapshotProcessFn snap_proc)
{
    mP->read(is, db, node_proc, snap_proc);
}

void
CaliReader::read(const std::string& filename, CaliperMetadataDB& db, NodeProcessFn node_proc, SnapshotProcessFn snap_proc)
{
    if (filename.empty())
        mP->read(std::cin, db, node_proc, snap_proc);
    else {
        std::ifstream is(filename.c_str());

        if (!is) {
            mP->m_error = true;
            mP->m_error_msg = std::string("Cannot open file ") + filename;
            return;
        }

        mP->read(is, db, node_proc, snap_proc);
    }
}