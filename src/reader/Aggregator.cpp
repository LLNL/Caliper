/// @file Aggregator.cpp
/// Aggregator implementation

#include "Aggregator.h"

#include "CaliperMetadataDB.h"

#include <Attribute.h>
#include <Log.h>
#include <Node.h>

#include <cali_types.h>

#include <util/split.hpp>

#include <algorithm>
#include <cassert>
#include <iostream>
#include <iterator>

using namespace cali;
using namespace std;

struct Aggregator::AggregatorImpl
{
    // --- data

    vector<string>    m_aggr_attribute_strings;
    vector<cali_id_t> m_aggr_attribute_ids;

    map< vector<string>, RecordMap > m_aggr_db;


    // --- interface

    void parse(const string& aggr_config) {
        vector<string> attributes;

        util::split(aggr_config, ':', back_inserter(m_aggr_attribute_strings));
    }

    vector<int> find_aggr_attribute_indices(CaliperMetadataDB& db, const vector<Variant>& v_attr_ids) {
        vector<int> vec;

        for (int i = 0; i < v_attr_ids.size(); ++i) {
            if (find(m_aggr_attribute_ids.begin(), m_aggr_attribute_ids.end(), v_attr_ids[i].to_id()) != m_aggr_attribute_ids.end())
                vec.push_back(i);
            else if (!m_aggr_attribute_strings.empty()) {
                Attribute attr = db.attribute(v_attr_ids[i].to_id());

                if (attr == Attribute::invalid)
                    continue;

                auto it = find(m_aggr_attribute_strings.begin(), m_aggr_attribute_strings.end(), attr.name());

                if (it != m_aggr_attribute_strings.end()) {
                    vec.push_back(i);
                    m_aggr_attribute_ids.push_back(attr.id());
                    m_aggr_attribute_strings.erase(it);
                }
            }
        }
            
        return vec;
    }

    Variant aggregate(const Attribute& attr, const Variant& l, const Variant& r) {
        switch (attr.type()) {
        case CALI_TYPE_INT:
            return Variant(l.to_int() + r.to_int());
        case CALI_TYPE_UINT:
            return Variant(l.to_uint() + r.to_uint());
        case CALI_TYPE_DOUBLE:
            return Variant(l.to_double() + r.to_double());
        default:
            Log(0).stream() << "Cannot aggregate attribute " << attr.name() 
                            << " with non-aggregatable type " 
                            << cali_type2string(attr.type()) 
                            << endl;
        }

        return Variant();
    }

    bool process(CaliperMetadataDB& db, const RecordMap& rec) {
        if (get_record_type(rec) != "ctx")
            return false;

        auto attr_it = rec.find("attr");

        if (attr_it == rec.end())
            return false;

        // --- get indices in attribute/data array of the values we want to aggregate

        vector<int> idx_vec = find_aggr_attribute_indices(db, attr_it->second);

        if (idx_vec.empty())
            return false;

        auto ctxt_it = rec.find("ref");
        auto data_it = rec.find("data");

        // --- build key vec

        vector<string> key_vec;

        // add strings of context node ids and immediate attribute ids
        for (const auto &rec_it : { ctxt_it, attr_it }) {
            if (rec_it == rec.end())
                continue;
            for (const Variant& v : rec_it->second)
                key_vec.emplace_back(v.to_string());
        }

        auto num_id_entries = key_vec.size();

        // add non-aggregate immediate data entries to key vec
        if (data_it != rec.end())
            for (int n = 0; n < data_it->second.size(); ++n)
                if (find(idx_vec.begin(), idx_vec.end(), n) == idx_vec.end())
                    key_vec.push_back(data_it->second[n].to_string());

        // sort to account for differently ordered elements
        sort(key_vec.begin(), key_vec.begin() + num_id_entries);
        sort(key_vec.begin() + num_id_entries, key_vec.end());

        // --- aggregate context records and enter into db

        auto aggr_db_it = m_aggr_db.find(key_vec);

        if (aggr_db_it == m_aggr_db.end())
            m_aggr_db.emplace(std::move(key_vec), rec);
        else {
            auto arec_attr_it = aggr_db_it->second.find("attr");
            auto arec_data_it = aggr_db_it->second.find("data");

            // there must be immediate data entries in aggregation record, otherwise it shouldn't be in db
            assert(arec_attr_it != aggr_db_it->second.end());
            assert(arec_data_it != aggr_db_it->second.end());

            // shortcuts
            vector<Variant>&       arec_attr_vec  { arec_attr_it->second };
            vector<Variant>&       arec_data_vec  { arec_data_it->second };
            const vector<Variant>& attr_vec       { attr_it->second };
            const vector<Variant>& data_vec       { data_it->second };

            assert(arec_attr_vec.size() == arec_data_vec.size());
            assert(attr_vec.size()      == data_vec.size()     );

            // aggregate values
            for (int i : idx_vec) {
                auto it = find(arec_attr_vec.begin(), arec_attr_vec.end(), attr_vec[i]);

                if (it != arec_attr_vec.end()) {
                    auto arec_i = (it - arec_attr_vec.begin());

                    arec_data_vec[arec_i] = 
                        aggregate(db.attribute(attr_vec[i].to_id()), arec_data_vec[arec_i], data_vec[i]);
                }
            }
        }

        return true;
    }

    void flush(CaliperMetadataDB& db, RecordProcessFn push) {
        for (const auto &p : m_aggr_db)
            push(db, p.second);
    }
};

Aggregator::Aggregator(const string& aggr_config)
    : mP { new AggregatorImpl() }
{
    mP->parse(aggr_config);
}

Aggregator::~Aggregator()
{
    mP.reset();
}

void 
Aggregator::flush(CaliperMetadataDB& db, RecordProcessFn push)
{
    mP->flush(db, push);
}

void
Aggregator::operator()(CaliperMetadataDB& db, const RecordMap& rec, RecordProcessFn push)
{
    if (!mP->process(db, rec))
        push(db, rec);
}
