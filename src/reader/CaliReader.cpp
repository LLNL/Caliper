// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "caliper/reader/CaliReader.h"

#include "caliper/reader/CaliperMetadataDB.h"

#include "caliper/common/Log.h"

#include "../common/StringConverter.h"

#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>

using namespace cali;

namespace
{

class fast_istringstream
{
    std::string::iterator it_;
    std::string::iterator end_;

public:

    fast_istringstream(std::string::iterator b, std::string::iterator e) : it_ { b }, end_ { e } {}

    inline bool good() const { return it_ != end_; }
    inline char get() { return *it_++; }
    inline void unget() { --it_; }

    inline bool matches(char c)
    {
        if (it_ != end_) {
            if (c == *it_) {
                ++it_;
                return true;
            }
        }
        return false;
    }

    inline bool matches(size_t N, const char* key)
    {
        if (it_ + N < end_) {
            if (std::equal(it_, it_ + N, key)) {
                it_ += N;
                return true;
            }
        }
        return false;
    }

    std::string context() { return std::string { it_, end_ }; }
};

inline uint64_t read_uint64_element(fast_istringstream& is)
{
    uint64_t ret = 0;

    while (is.good()) {
        char c = is.get();

        if (c == '=' || c == ',') {
            is.unget();
            break;
        }

        ret = ret * 10 + static_cast<uint64_t>(c - '0');
    }

    return ret;
}

inline std::string read_escaped_word(fast_istringstream& is)
{
    std::string w;
    w.reserve(32);
    bool escape = false;

    while (is.good()) {
        char c = is.get();

        if (escape) {
            w.push_back(c == 'n' ? '\n' : c);
            escape = false;
        } else {
            if (c == '=' || c == ',') {
                is.unget();
                break;
            } else if (c == '\\') {
                escape = true;
            } else {
                w.push_back(c);
            }
        }
    }

    return w;
}

inline std::vector<cali_id_t> read_id_list(fast_istringstream& is)
{
    std::vector<cali_id_t> ret;
    ret.reserve(8);

    do {
        ret.push_back(read_uint64_element(is));
    } while (is.matches('='));

    return ret;
}

inline std::vector<std::string> read_string_list(fast_istringstream& is)
{
    std::vector<std::string> ret;
    ret.reserve(8);

    do {
        ret.emplace_back(read_escaped_word(is));
    } while (is.matches('='));

    return ret;
}

} // namespace

struct CaliReader::CaliReaderImpl {
    bool         m_error;
    std::string  m_error_msg;
    unsigned int m_num_read;

    CaliReaderImpl() : m_error { false } {}

    void set_error(const std::string& msg)
    {
        m_error     = true;
        m_error_msg = msg;
    }

    void read_node(fast_istringstream& is, CaliperMetadataDB& db, IdMap& idmap, NodeProcessFn& node_proc)
    {
        cali_id_t   attr_id = CALI_INV_ID;
        cali_id_t   node_id = CALI_INV_ID;
        cali_id_t   prnt_id = CALI_INV_ID;
        std::string data_str;

        do {
            if (is.matches(5, "attr="))
                attr_id = read_uint64_element(is);
            else if (is.matches(5, "data="))
                data_str = read_escaped_word(is);
            else if (is.matches(3, "id="))
                node_id = read_uint64_element(is);
            else if (is.matches(7, "parent="))
                prnt_id = read_uint64_element(is);
            else
                break; // unknown key
        } while (is.matches(','));

        if (node_id == CALI_INV_ID || attr_id == CALI_INV_ID) {
            set_error("Node or attribute ID missing in node record");
            return;
        }

        const Node* node = db.merge_node(node_id, attr_id, prnt_id, data_str, idmap);

        if (node)
            node_proc(db, node);
        else
            set_error("Invalid node record");
    }

    void read_snapshot(fast_istringstream& is, CaliperMetadataDB& db, IdMap& idmap, SnapshotProcessFn& snap_proc)
    {
        std::vector<cali_id_t>   refs;
        std::vector<cali_id_t>   attr;
        std::vector<std::string> data;

        do {
            if (is.matches(4, "ref="))
                refs = read_id_list(is);
            else if (is.matches(5, "attr="))
                attr = read_id_list(is);
            else if (is.matches(5, "data="))
                data = read_string_list(is);
            else
                break;
        } while (is.matches(','));

        if (attr.size() != data.size())
            set_error("attr / data size mismatch");

        std::vector<Entry> rec;
        rec.reserve(refs.size() + std::min(attr.size(), data.size()));

        for (cali_id_t id : refs)
            rec.push_back(db.merge_entry(id, idmap));
        for (size_t i = 0; i < std::min(attr.size(), data.size()); ++i)
            rec.push_back(db.merge_entry(attr[i], data[i], idmap));

        snap_proc(db, rec);
    }

    void read_globals(fast_istringstream& is, CaliperMetadataDB& db, IdMap& idmap)
    {
        std::vector<cali_id_t>   refs;
        std::vector<cali_id_t>   attr;
        std::vector<std::string> data;

        do {
            if (is.matches(4, "ref="))
                refs = read_id_list(is);
            else if (is.matches(5, "attr="))
                attr = read_id_list(is);
            else if (is.matches(5, "data="))
                data = read_string_list(is);
            else
                break;
        } while (is.matches(','));

        if (attr.size() != data.size())
            set_error("attr / data size mismatch");

        for (cali_id_t id : refs)
            db.merge_global(id, idmap);
        for (size_t i = 0; i < std::min(attr.size(), data.size()); ++i)
            db.merge_global(attr[i], data[i], idmap);
    }

    void read_record(
        fast_istringstream& is,
        CaliperMetadataDB&  db,
        IdMap&              idmap,
        NodeProcessFn&      node_proc,
        SnapshotProcessFn&  snap_proc
    )
    {
        if (is.matches(11, "__rec=node,")) {
            read_node(is, db, idmap, node_proc);
        } else if (is.matches(10, "__rec=ctx,")) {
            read_snapshot(is, db, idmap, snap_proc);
        } else if (is.matches(14, "__rec=globals,")) {
            read_globals(is, db, idmap);
        } else {
            set_error(std::string("Unknown/invalid record: ") + is.context());
        }
    }

    void read(std::istream& is, CaliperMetadataDB& db, NodeProcessFn node_proc, SnapshotProcessFn snap_proc)
    {
        IdMap idmap;

        for (std::string line; std::getline(is, line);) {
            if (line.empty())
                continue;
            fast_istringstream isstream { line.begin(), line.end() };
            read_record(isstream, db, idmap, node_proc, snap_proc);
        }
    }
};

CaliReader::CaliReader() : mP { new CaliReaderImpl() }
{}

CaliReader::~CaliReader()
{}

bool CaliReader::error() const
{
    return mP->m_error;
}

std::string CaliReader::error_msg() const
{
    return mP->m_error_msg;
}

void CaliReader::read(std::istream& is, CaliperMetadataDB& db, NodeProcessFn node_proc, SnapshotProcessFn snap_proc)
{
    mP->read(is, db, node_proc, snap_proc);
}

void CaliReader::read(
    const std::string& filename,
    CaliperMetadataDB& db,
    NodeProcessFn      node_proc,
    SnapshotProcessFn  snap_proc
)
{
    if (filename.empty())
        mP->read(std::cin, db, node_proc, snap_proc);
    else {
        std::ifstream is(filename.c_str());

        if (!is) {
            mP->m_error     = true;
            mP->m_error_msg = std::string("Cannot open file ") + filename;
            return;
        }

        mP->read(is, db, node_proc, snap_proc);
    }
}