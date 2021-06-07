// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// Aggregator implementation

#include "caliper/reader/Aggregator.h"

#include "caliper/reader/QuerySpec.h"

#include "caliper/common/Attribute.h"
#include "caliper/common/CaliperMetadataAccessInterface.h"
#include "caliper/common/Log.h"
#include "caliper/common/Node.h"

#include "caliper/common/cali_types.h"

#include "caliper/common/c-util/vlenc.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <iostream>
#include <iterator>
#include <mutex>

using namespace cali;
using namespace std;

#define MAX_KEYLEN 32

namespace
{

class AggregateKernelConfig;

class AggregateKernel {
public:

    virtual ~AggregateKernel()
        { }

    virtual const AggregateKernelConfig* config() = 0;

    // For inclusive metrics, parent_aggregate is invoked for parent nodes
    virtual void parent_aggregate(CaliperMetadataAccessInterface& db, const EntryList& list) {
        aggregate(db, list);
    }

    virtual void aggregate(CaliperMetadataAccessInterface& db, const EntryList& list) = 0;
    virtual void append_result(CaliperMetadataAccessInterface& db, EntryList& list) = 0;
};

class AggregateKernelConfig
{
public:

    virtual ~AggregateKernelConfig()
        { }

    virtual bool is_inclusive() const { return false; }

    virtual AggregateKernel* make_kernel() = 0;
};

//
// --- CountKernel
//

class CountKernel : public AggregateKernel {
public:

    class Config : public AggregateKernelConfig {
        Attribute m_attr;

    public:

        Attribute attribute(CaliperMetadataAccessInterface& db) {
            if (m_attr == Attribute::invalid)
                m_attr = db.create_attribute("count", CALI_TYPE_UINT, CALI_ATTR_ASVALUE);

            return m_attr;
        }

        AggregateKernel* make_kernel() {
            return new CountKernel(this);
        }

        Config()
            : m_attr { Attribute::invalid }
            { }

        static AggregateKernelConfig* create(const std::vector<std::string>&) {
            return new Config;
        }
    };

    CountKernel(Config* config)
        : m_count(0), m_config(config)
        { }

    const AggregateKernelConfig* config() { return m_config; }

    void aggregate(CaliperMetadataAccessInterface& db, const EntryList& list) {
        cali_id_t count_attr_id = m_config->attribute(db).id();

        for (const Entry& e : list)
            if (e.attribute() == count_attr_id) {
                m_count += e.value().to_uint();
                return;
            }

        ++m_count;
    }

    void append_result(CaliperMetadataAccessInterface& db, EntryList& list) {
        uint64_t count = m_count.load();

        if (count > 0)
            list.push_back(Entry(m_config->attribute(db),
                                 Variant(CALI_TYPE_UINT, &count, sizeof(uint64_t))));
    }

private:

    std::atomic<uint64_t> m_count;
    Config*  m_config;
};

class ScaledCountKernel : public AggregateKernel {
public:

    class Config : public AggregateKernelConfig {
        Attribute   m_count_attr;
        Attribute   m_res_attr;

        double      m_scale;
        std::string m_scale_str;

    public:

        Attribute get_count_attr(CaliperMetadataAccessInterface& db) {
            if (m_count_attr == Attribute::invalid) {
                m_count_attr =
                    db.create_attribute(std::string("scount#")+m_scale_str, CALI_TYPE_UINT,
                                        CALI_ATTR_ASVALUE |
                                        CALI_ATTR_HIDDEN);
            }

            return m_count_attr;
        }

        Attribute get_result_attr(CaliperMetadataAccessInterface& db) {
            if (m_res_attr == Attribute::invalid)
                m_res_attr =
                    db.create_attribute(std::string("scount"), CALI_TYPE_DOUBLE,
                                        CALI_ATTR_ASVALUE);

            return m_res_attr;
        }

        double get_scale()    const { return m_scale;     }

        AggregateKernel* make_kernel() {
            return new ScaledCountKernel(this);
        }

        explicit Config(const std::vector<std::string>& cfg)
            : m_count_attr(Attribute::invalid),
              m_res_attr(Attribute::invalid),
              m_scale(0.0),
              m_scale_str(cfg[0])
            {
                m_scale = std::stod(m_scale_str);
            }

        static AggregateKernelConfig* create(const std::vector<std::string>& cfg) {
            return new Config(cfg);
        }
    };

    ScaledCountKernel(Config* config)
        : m_count(0), m_config(config)
        { }

    const AggregateKernelConfig* config() { return m_config; }

    virtual void aggregate(CaliperMetadataAccessInterface& db, const EntryList& list) {
        std::lock_guard<std::mutex>
            g(m_lock);

        Attribute count_attr = m_config->get_count_attr(db);

        for (const Entry& e : list)
            if (e.attribute() == count_attr.id()) {
                m_count += e.value().to_uint();
                return;
            }

        ++m_count;
    }

    virtual void append_result(CaliperMetadataAccessInterface& db, EntryList& list) {
        if (m_count > 0) {
            list.push_back(Entry(m_config->get_count_attr(db),  Variant(m_count)));
            list.push_back(Entry(m_config->get_result_attr(db), Variant(m_config->get_scale() * m_count)));
        }
    }

private:

    uint64_t   m_count;
    std::mutex m_lock;

    Config*    m_config;
};

//
// --- SumKernel
//

class SumKernel : public AggregateKernel {
public:

    class Config : public AggregateKernelConfig {
        std::string m_target_attr_name;

        Attribute   m_target_attr;
        Attribute   m_sum_attr;

        bool        m_is_inclusive;

    public:

        Attribute get_target_attr(CaliperMetadataAccessInterface& db) {
            if (m_target_attr == Attribute::invalid) {
                m_target_attr = db.get_attribute(m_target_attr_name);

                if (m_target_attr != Attribute::invalid)
                    if (!m_target_attr.store_as_value())
                        Log(0).stream() << "sum(" << m_target_attr_name << "): Attribute "
                                        << m_target_attr_name
                                        << " does not have CALI_ATTR_ASVALUE property!"
                                        << std::endl;
            }

            return m_target_attr;
        }

        Attribute get_sum_attr(CaliperMetadataAccessInterface& db) {
            if (m_target_attr == Attribute::invalid)
                return Attribute::invalid;

            if (m_sum_attr == Attribute::invalid)
                m_sum_attr = db.create_attribute(std::string(m_is_inclusive ? "inclusive#" : "sum#") + m_target_attr_name, m_target_attr.type(),
                                                 CALI_ATTR_SKIP_EVENTS | CALI_ATTR_ASVALUE);

            return m_sum_attr;
        }

        bool is_inclusive() const { return m_is_inclusive; }

        AggregateKernel* make_kernel() {
            return new SumKernel(this);
        }

        Config(const std::string& name, bool inclusive)
            : m_target_attr_name(name),
              m_target_attr(Attribute::invalid),
              m_sum_attr(Attribute::invalid),
              m_is_inclusive(inclusive)
            { }

        static AggregateKernelConfig* create(const std::vector<std::string>& cfg) {
            return new Config(cfg.front(), false);
        }

        static AggregateKernelConfig* create_inclusive(const std::vector<std::string>& cfg) {
            return new Config(cfg.front(), true);
        }
    };

    SumKernel(Config* config)
        : m_count(0), m_config(config)
        { }

    const AggregateKernelConfig* config() { return m_config; }

    virtual void aggregate(CaliperMetadataAccessInterface& db, const EntryList& rec) {
        std::lock_guard<std::mutex>
            g(m_lock);

        Attribute target_attr = m_config->get_target_attr(db);

        if (target_attr == Attribute::invalid)
            return;

        Attribute sum_attr = m_config->get_sum_attr(db);

        cali_id_t tgt_id = target_attr.id();
        cali_id_t sum_id = sum_attr.id();

        for (const Entry& e : rec) {
            if (e.attribute() == tgt_id || e.attribute() == sum_id) {
                switch (target_attr.type()) {
                case CALI_TYPE_DOUBLE:
                    m_sum = Variant(m_sum.to_double() + e.value().to_double());
                    break;
                case CALI_TYPE_INT:
                    {
                        int64_t s = m_sum.to_int64()  + e.value().to_int64();
                        m_sum = Variant(cali_make_variant_from_int64(s));
                    }
                    break;
                case CALI_TYPE_UINT:
                    m_sum = Variant(m_sum.to_uint()   + e.value().to_uint()  );
                    break;
                default:
                    ;
                    // Some error?!
                }

                ++m_count;
                break;
            }
        }
    }

    virtual void append_result(CaliperMetadataAccessInterface& db, EntryList& rec) {
        if (m_count > 0)
            rec.push_back(Entry(m_config->get_sum_attr(db), m_sum));
    }

private:

    unsigned   m_count;
    Variant    m_sum;
    std::mutex m_lock;
    Config*    m_config;
};

class ScaledSumKernel : public AggregateKernel {
public:

    class Config : public AggregateKernelConfig {
        std::string m_target_attr_name;
        Attribute   m_target_attr;
        Attribute   m_sum_attr;

        Attribute   m_res_attr;

        double      m_scale;
        bool        m_inclusive;

    public:

        Attribute get_target_attr(CaliperMetadataAccessInterface& db) {
            if (m_target_attr == Attribute::invalid)
                m_target_attr = db.get_attribute(m_target_attr_name);

            return m_target_attr;
        }

        Attribute get_sum_attr(CaliperMetadataAccessInterface& db) {
            if (m_sum_attr == Attribute::invalid)
                m_sum_attr =
                    db.create_attribute(std::string(m_inclusive ? "iscsum#" : "scsum#")+m_target_attr_name, CALI_TYPE_DOUBLE,
                                        CALI_ATTR_ASVALUE |
                                        CALI_ATTR_HIDDEN);

            return m_sum_attr;
        }

        Attribute get_result_attr(CaliperMetadataAccessInterface& db) {
            if (m_res_attr == Attribute::invalid)
                m_res_attr =
                    db.create_attribute(std::string(m_inclusive ? "iscale#" : "scale#")+m_target_attr_name, CALI_TYPE_DOUBLE,
                                        CALI_ATTR_ASVALUE);

            return m_res_attr;
        }

        double get_scale()    const { return m_scale;     }
        bool   is_inclusive() const { return m_inclusive; }

        AggregateKernel* make_kernel() {
            return new ScaledSumKernel(this);
        }

        Config(const std::vector<std::string>& cfg, bool inclusive)
            : m_target_attr_name(cfg[0]),
              m_target_attr(Attribute::invalid),
              m_sum_attr(Attribute::invalid),
              m_res_attr(Attribute::invalid),
              m_scale(0.0),
              m_inclusive(inclusive)
            {
                if (cfg.size() > 1)
                    m_scale = std::stod(cfg[1]);
            }

        static AggregateKernelConfig* create(const std::vector<std::string>& cfg) {
            return new Config(cfg, false);
        }

        static AggregateKernelConfig* create_inclusive(const std::vector<std::string>& cfg) {
            return new Config(cfg, true);
        }
    };

    ScaledSumKernel(Config* config)
        : m_count(0), m_sum(0.0), m_config(config)
        { }

    const AggregateKernelConfig* config() { return m_config; }

    virtual void aggregate(CaliperMetadataAccessInterface& db, const EntryList& list) {
        std::lock_guard<std::mutex>
            g(m_lock);

        Attribute target_attr = m_config->get_target_attr(db);
        Attribute sum_attr = m_config->get_sum_attr(db);

        for (const Entry& e : list) {
            if (e.attribute() == target_attr.id() || e.attribute() == sum_attr.id()) {
                m_sum += e.value().to_double();
                ++m_count;
            }
        }
    }

    virtual void append_result(CaliperMetadataAccessInterface& db, EntryList& list) {
        if (m_count > 0) {
            list.push_back(Entry(m_config->get_sum_attr(db),    Variant(m_sum)));
            list.push_back(Entry(m_config->get_result_attr(db), Variant(m_config->get_scale() * m_sum)));
        }
    }

private:

    unsigned   m_count;
    double     m_sum;

    std::mutex m_lock;

    Config*    m_config;
};

class MinKernel : public AggregateKernel {
public:

    struct StatisticsAttributes {
        Attribute min;
    };

    class Config : public AggregateKernelConfig {
        std::string          m_target_attr_name;
        Attribute            m_target_attr;

        StatisticsAttributes m_stat_attrs;

    public:

        Attribute get_target_attr(CaliperMetadataAccessInterface& db) {
            if (m_target_attr == Attribute::invalid) {
                m_target_attr = db.get_attribute(m_target_attr_name);

                if (m_target_attr != Attribute::invalid)
                    if (!m_target_attr.store_as_value())
                        Log(0).stream() << "min(" << m_target_attr_name << "): Attribute "
                                        << m_target_attr_name
                                        << " does not have CALI_ATTR_ASVALUE property!"
                                        << std::endl;
            }

            return m_target_attr;
        }

        bool get_statistics_attributes(CaliperMetadataAccessInterface& db, StatisticsAttributes& a) {
            if (m_target_attr == Attribute::invalid)
                return false;
            if (a.min != Attribute::invalid) {
                a = m_stat_attrs;
                return true;
            }

            cali_attr_type type = m_target_attr.type();
            int            prop = CALI_ATTR_SKIP_EVENTS | CALI_ATTR_ASVALUE;

            m_stat_attrs.min =
                db.create_attribute("min#" + m_target_attr_name, type, prop);

            a = m_stat_attrs;
            return true;
        }

        AggregateKernel* make_kernel() {
            return new MinKernel(this);
        }

        Config(const std::string& name)
            : m_target_attr_name(name),
              m_target_attr(Attribute::invalid)
            { }

        static AggregateKernelConfig* create(const std::vector<std::string>& cfg) {
            return new Config(cfg.front());
        }
    };

    MinKernel(Config* config)
        : m_config(config)
        { }

    const AggregateKernelConfig* config() { return m_config; }

    virtual void aggregate(CaliperMetadataAccessInterface& db, const EntryList& list) {
        std::lock_guard<std::mutex>
            g(m_lock);

        Attribute target_attr = m_config->get_target_attr(db);
        StatisticsAttributes stat_attr;

        if (!m_config->get_statistics_attributes(db, stat_attr))
            return;

        for (const Entry& e : list) {
            if (e.attribute() == target_attr.id() || e.attribute() == stat_attr.min.id()) {
                if (m_min.empty()) {
                    m_min = e.value();
                } else {
                    switch (target_attr.type()) {
                    case CALI_TYPE_INT:
                        {
                            int64_t m = std::min(m_min.to_int64(), e.value().to_int64());
                            m_min = Variant(cali_make_variant_from_int64(m));
                        }
                        break;
                    case CALI_TYPE_DOUBLE:
                        m_min = Variant(std::min(m_min.to_double(), e.value().to_double()));
                        break;
                    case CALI_TYPE_UINT:
                        m_min = Variant(std::min(m_min.to_uint(),   e.value().to_uint()));
                        break;
                    default:
                        ;
                    }
                }
            }
        }
    }

    virtual void append_result(CaliperMetadataAccessInterface& db, EntryList& list) {
        if (!m_min.empty()) {
            StatisticsAttributes stat_attr;

            if (!m_config->get_statistics_attributes(db, stat_attr))
                return;

            list.push_back(Entry(stat_attr.min, m_min));
        }
    }

private:

    Variant    m_min;
    std::mutex m_lock;
    Config*    m_config;
};

class MaxKernel : public AggregateKernel {
public:

    struct StatisticsAttributes {
        Attribute max;
    };

    class Config : public AggregateKernelConfig {
        std::string          m_target_attr_name;
        Attribute            m_target_attr;

        StatisticsAttributes m_stat_attrs;

    public:

        Attribute get_target_attr(CaliperMetadataAccessInterface& db) {
            if (m_target_attr == Attribute::invalid) {
                m_target_attr = db.get_attribute(m_target_attr_name);

                if (m_target_attr != Attribute::invalid)
                    if (!m_target_attr.store_as_value())
                        Log(0).stream() << "max(" << m_target_attr_name << "): Attribute "
                                        << m_target_attr_name
                                        << " does not have CALI_ATTR_ASVALUE property!"
                                        << std::endl;
            }

            return m_target_attr;
        }

        bool get_statistics_attributes(CaliperMetadataAccessInterface& db, StatisticsAttributes& a) {
            if (m_target_attr == Attribute::invalid)
                return false;
            if (a.max != Attribute::invalid) {
                a = m_stat_attrs;
                return true;
            }

            cali_attr_type type = m_target_attr.type();
            int            prop = CALI_ATTR_SKIP_EVENTS | CALI_ATTR_ASVALUE;

            m_stat_attrs.max =
                db.create_attribute("max#" + m_target_attr_name, type, prop);

            a = m_stat_attrs;
            return true;
        }

        AggregateKernel* make_kernel() {
            return new MaxKernel(this);
        }

        Config(const std::string& name)
            : m_target_attr_name(name),
              m_target_attr(Attribute::invalid)
            { }

        static AggregateKernelConfig* create(const std::vector<std::string>& cfg) {
            return new Config(cfg.front());
        }
    };

    MaxKernel(Config* config)
        : m_config(config)
        { }

    const AggregateKernelConfig* config() { return m_config; }

    virtual void aggregate(CaliperMetadataAccessInterface& db, const EntryList& list) {
        std::lock_guard<std::mutex>
            g(m_lock);

        Attribute target_attr = m_config->get_target_attr(db);
        StatisticsAttributes stat_attr;

        if (!m_config->get_statistics_attributes(db, stat_attr))
            return;

        for (const Entry& e : list) {
            if (e.attribute() == target_attr.id() || e.attribute() == stat_attr.max.id()) {
                if (m_max.empty()) {
                    m_max = e.value();
                } else {
                    switch (target_attr.type()) {
                    case CALI_TYPE_INT:
                        {
                            int64_t m = std::max(m_max.to_int64(), e.value().to_int64());
                            m_max = Variant(cali_make_variant_from_int64(m));
                        }
                        break;
                    case CALI_TYPE_DOUBLE:
                        m_max = Variant(std::max(m_max.to_double(), e.value().to_double()));
                        break;
                    case CALI_TYPE_UINT:
                        m_max = Variant(std::max(m_max.to_uint(),   e.value().to_uint()));
                        break;
                    default:
                        ;
                    }
                }
            }
        }
    }

    virtual void append_result(CaliperMetadataAccessInterface& db, EntryList& list) {
        if (!m_max.empty()) {
            StatisticsAttributes stat_attr;

            if (!m_config->get_statistics_attributes(db, stat_attr))
                return;

            list.push_back(Entry(stat_attr.max, m_max));
        }
    }

private:

    Variant    m_max;
    std::mutex m_lock;
    Config*    m_config;
};

class AvgKernel : public AggregateKernel {
public:

    struct StatisticsAttributes {
        Attribute avg;
        Attribute sum;
        Attribute count;
    };

    class Config : public AggregateKernelConfig {
        std::string          m_target_attr_name;
        Attribute            m_target_attr;

        StatisticsAttributes m_stat_attrs;

    public:

        Attribute get_target_attr(CaliperMetadataAccessInterface& db) {
            if (m_target_attr == Attribute::invalid) {
                m_target_attr = db.get_attribute(m_target_attr_name);

                if (m_target_attr != Attribute::invalid)
                    if (!m_target_attr.store_as_value())
                        Log(0).stream() << "avg(" << m_target_attr_name << "): Attribute "
                                        << m_target_attr_name
                                        << " does not have CALI_ATTR_ASVALUE property!"
                                        << std::endl;
            }


            return m_target_attr;
        }

        bool get_statistics_attributes(CaliperMetadataAccessInterface& db, StatisticsAttributes& a) {
            if (m_target_attr == Attribute::invalid)
                return false;
            if (a.sum != Attribute::invalid) {
                a = m_stat_attrs;
                return true;
            }

            int prop = CALI_ATTR_SKIP_EVENTS | CALI_ATTR_ASVALUE;

            m_stat_attrs.avg =
                db.create_attribute("avg#" + m_target_attr_name, CALI_TYPE_DOUBLE, prop);
            m_stat_attrs.count =
                db.create_attribute("avg.count#" + m_target_attr_name, CALI_TYPE_UINT,   prop | CALI_ATTR_HIDDEN);
            m_stat_attrs.sum =
                db.create_attribute("avg.sum#"   + m_target_attr_name, CALI_TYPE_DOUBLE, prop | CALI_ATTR_HIDDEN);

            a = m_stat_attrs;
            return true;
        }

        AggregateKernel* make_kernel() {
            return new AvgKernel(this);
        }

        Config(const std::string& name)
            : m_target_attr_name(name),
              m_target_attr(Attribute::invalid)
            { }

        static AggregateKernelConfig* create(const std::vector<std::string>& cfg) {
            return new Config(cfg.front());
        }
    };

    AvgKernel(Config* config)
        : m_count(0), m_sum(0.0), m_config(config)
        { }

    const AggregateKernelConfig* config() { return m_config; }

    virtual void aggregate(CaliperMetadataAccessInterface& db, const EntryList& list) {
        std::lock_guard<std::mutex>
            g(m_lock);

        Attribute target_attr = m_config->get_target_attr(db);
        StatisticsAttributes stat_attr;

        if (!m_config->get_statistics_attributes(db, stat_attr))
            return;

        for (const Entry& e : list) {
            if (e.attribute() == target_attr.id()) {
                m_sum += e.value().to_double();
                ++m_count;
            } else if (e.attribute() == stat_attr.sum.id()) {
                m_sum += e.value().to_double();
            } else if (e.attribute() == stat_attr.count.id()) {
                m_count += e.value().to_uint();
            }
        }
    }

    virtual void append_result(CaliperMetadataAccessInterface& db, EntryList& list) {
        if (m_count > 0) {
            StatisticsAttributes stat_attr;

            if (!m_config->get_statistics_attributes(db, stat_attr))
                return;

            list.push_back(Entry(stat_attr.avg, Variant(m_sum / m_count)));
            list.push_back(Entry(stat_attr.sum, Variant(m_sum)));
            list.push_back(Entry(stat_attr.count, Variant(cali_make_variant_from_uint(m_count))));
        }
    }

private:

    unsigned   m_count;
    double     m_sum;

    std::mutex m_lock;

    Config*    m_config;
};

//
// --- ScaledRatioKernel
//

class ScaledRatioKernel : public AggregateKernel {
public:

    class Config : public AggregateKernelConfig {
        std::string m_tgt1_attr_name;
        std::string m_tgt2_attr_name;

        Attribute   m_tgt1_attr;
        Attribute   m_tgt2_attr;
        Attribute   m_sum1_attr;
        Attribute   m_sum2_attr;

        Attribute   m_ratio_attr;

        double      m_scale;
        bool        m_inclusive;

    public:

        double get_scale()    const { return m_scale;     }
        bool   is_inclusive() const { return m_inclusive; }

        std::pair<Attribute,Attribute> get_target_attributes(CaliperMetadataAccessInterface& db) {
            if (m_tgt1_attr == Attribute::invalid)
                m_tgt1_attr = db.get_attribute(m_tgt1_attr_name);
            if (m_tgt2_attr == Attribute::invalid)
                m_tgt2_attr = db.get_attribute(m_tgt2_attr_name);

            return std::pair<Attribute,Attribute>(m_tgt1_attr, m_tgt2_attr);
        }

        std::pair<Attribute,Attribute> get_sum_attributes(CaliperMetadataAccessInterface& db) {
            if (m_sum1_attr == Attribute::invalid)
                m_sum1_attr =
                    db.create_attribute(std::string(m_inclusive ? "isr.sum#" : "sr.sum#") + m_tgt1_attr_name, CALI_TYPE_DOUBLE,
                                        CALI_ATTR_SKIP_EVENTS |
                                        CALI_ATTR_ASVALUE     |
                                        CALI_ATTR_HIDDEN);
            if (m_sum2_attr == Attribute::invalid)
                m_sum2_attr =
                    db.create_attribute(std::string(m_inclusive ? "isr.sum#" : "sr.sum#") + m_tgt2_attr_name, CALI_TYPE_DOUBLE,
                                        CALI_ATTR_SKIP_EVENTS |
                                        CALI_ATTR_ASVALUE     |
                                        CALI_ATTR_HIDDEN);

            return std::make_pair(m_sum1_attr, m_sum2_attr);
        }

        Attribute get_ratio_attribute(CaliperMetadataAccessInterface& db) {
            if (m_ratio_attr == Attribute::invalid)
                m_ratio_attr =
                    db.create_attribute(std::string(m_inclusive ? "iratio#" : "ratio#") + m_tgt1_attr_name + "/" + m_tgt2_attr_name,
                                        CALI_TYPE_DOUBLE,
                                        CALI_ATTR_SKIP_EVENTS | CALI_ATTR_ASVALUE);

            return m_ratio_attr;
        }

        AggregateKernel* make_kernel() {
            return new ScaledRatioKernel(this);
        }

        Config(const std::vector<std::string>& cfg, bool is_inclusive)
            : m_tgt1_attr_name(cfg[0]), // We have already checked that there are two strings given
              m_tgt2_attr_name(cfg[1]),
              m_tgt1_attr(Attribute::invalid),
              m_tgt2_attr(Attribute::invalid),
              m_scale(1.0),
              m_inclusive(is_inclusive)
            {
                if (cfg.size() > 2)
                    m_scale = std::stod(cfg[2]);
            }

        static AggregateKernelConfig* create(const std::vector<std::string>& cfg) {
            return new Config(cfg, false);
        }

        static AggregateKernelConfig* create_inclusive(const std::vector<std::string>& cfg) {
            return new Config(cfg, true);
        }
    };

    ScaledRatioKernel(Config* config)
        : m_sum1(0), m_sum2(0), m_count(0), m_config(config)
        { }

    const AggregateKernelConfig* config() { return m_config; }

    virtual void aggregate(CaliperMetadataAccessInterface& db, const EntryList& list) {
        std::lock_guard<std::mutex>
            g(m_lock);

        auto tattrs = m_config->get_target_attributes(db);
        auto sattrs = m_config->get_sum_attributes(db);

        for (const Entry& e : list) {
            cali_id_t attr = e.attribute();

            if        (attr == tattrs.first.id()  || attr == sattrs.first.id())  {
                m_sum1 += e.value().to_double();
                ++m_count;
            } else if (attr == tattrs.second.id() || attr == sattrs.second.id()) {
                m_sum2 += e.value().to_double();
            }
        }
    }

    virtual void append_result(CaliperMetadataAccessInterface& db, EntryList& list) {
        auto sum_attrs = m_config->get_sum_attributes(db);

        if (m_count > 0 && m_sum1 > 0)
            list.push_back(Entry(sum_attrs.first,  Variant(m_sum1)));
        if (m_sum2 > 0) {
            list.push_back(Entry(sum_attrs.second, Variant(m_sum2)));

            if (m_count > 0)
                list.push_back(Entry(m_config->get_ratio_attribute(db),
                                     Variant(m_config->get_scale() * m_sum1 / m_sum2)));
        }
    }

private:

    double  m_sum1;
    double  m_sum2;
    int     m_count;

    std::mutex m_lock;

    Config* m_config;
};

//
// --- PercentTotalKernel
//

class PercentTotalKernel : public AggregateKernel {
public:

    class Config : public AggregateKernelConfig {
        std::string m_target_attr_name;
        Attribute   m_target_attr;
        Attribute   m_sum_attr;

        Attribute   m_percentage_attr;

        std::mutex  m_total_lock;
        double      m_total;

        bool        m_is_inclusive;

    public:

        Attribute get_target_attr(CaliperMetadataAccessInterface& db) {
            if (m_target_attr == Attribute::invalid) {
                m_target_attr = db.get_attribute(m_target_attr_name);

                if (m_target_attr != Attribute::invalid)
                    if (!m_target_attr.store_as_value())
                        Log(0).stream() << "percent_total(" << m_target_attr_name << "): Attribute "
                                        << m_target_attr_name
                                        << " does not have CALI_ATTR_ASVALUE property!"
                                        << std::endl;
            }

            return m_target_attr;
        }

        bool get_percentage_attribute(CaliperMetadataAccessInterface& db,
                                      Attribute& percentage_attr,
                                      Attribute& sum_attr) {
            if (m_target_attr == Attribute::invalid)
                return false;
            if (m_percentage_attr != Attribute::invalid) {
                percentage_attr = m_percentage_attr;
                sum_attr = m_sum_attr;
                return true;
            }

            m_percentage_attr =
                db.create_attribute(std::string(m_is_inclusive ? "ipercent_total#" : "percent_total#") + m_target_attr_name,
                                    CALI_TYPE_DOUBLE,
                                    CALI_ATTR_SKIP_EVENTS | CALI_ATTR_ASVALUE);

            m_sum_attr =
                db.create_attribute(std::string(m_is_inclusive ? "ipct.sum#" : "pct.sum#") + m_target_attr_name,
                                    CALI_TYPE_DOUBLE,
                                    CALI_ATTR_SKIP_EVENTS | CALI_ATTR_ASVALUE | CALI_ATTR_HIDDEN);

            percentage_attr = m_percentage_attr;
            sum_attr = m_sum_attr;

            return true;
        }

        AggregateKernel* make_kernel() {
            return new PercentTotalKernel(this);
        }

        void add(double val) {
            std::lock_guard<std::mutex>
                g(m_total_lock);

            m_total += val;
        }

        double get_total() {
            return m_total;
        }

        bool is_inclusive() const {
            return m_is_inclusive;
        }

        Config(const std::vector<std::string>& names, bool inclusive)
            : m_target_attr_name(names.front()),
              m_target_attr(Attribute::invalid),
              m_total(0),
              m_is_inclusive(inclusive)
        { }

        static AggregateKernelConfig* create(const std::vector<std::string>& cfg) {
            return new Config(cfg, false);
        }

        static AggregateKernelConfig* create_inclusive(const std::vector<std::string>& cfg) {
            return new Config(cfg, true);
        }
    };

    PercentTotalKernel(Config* config)
        : m_sum(0), m_isum(0), m_config(config)
        { }

    const AggregateKernelConfig* config() { return m_config; }

    void aggregate(CaliperMetadataAccessInterface& db, const EntryList& list) {
        Attribute target_attr = m_config->get_target_attr(db);
        Attribute percentage_attr, sum_attr;

        if (!m_config->get_percentage_attribute(db, percentage_attr, sum_attr))
            return;

        cali_id_t target_id = target_attr.id();
        cali_id_t sum_id    = sum_attr.id();

        for (const Entry& e : list) {
            cali_id_t id = e.attribute();

            if (id == target_id || id == sum_id) {
                double val = e.value().to_double();
                m_sum  += val;
                m_isum += val;
                m_config->add(val);
            }
        }
    }

    void parent_aggregate(CaliperMetadataAccessInterface& db, const EntryList& list) {
        Attribute target_attr = m_config->get_target_attr(db);
        Attribute percentage_attr, sum_attr;

        if (!m_config->get_percentage_attribute(db, percentage_attr, sum_attr))
            return;

        cali_id_t target_id = target_attr.id();
        cali_id_t sum_id    = sum_attr.id();

        for (const Entry& e : list) {
            cali_id_t id = e.attribute();

            if (id == target_id || id == sum_id)
                m_isum += e.value().to_double();
        }
    }

    virtual void append_result(CaliperMetadataAccessInterface& db, EntryList& list) {
        double total = m_config->get_total();

        if (total > 0) {
            Attribute percentage_attr, sum_attr;

            if (!m_config->get_percentage_attribute(db, percentage_attr, sum_attr))
                return;

            list.push_back(Entry(sum_attr, Variant(m_sum)));
            list.push_back(Entry(percentage_attr, Variant(100.0 * m_isum / total)));
        }
    }

private:

    double     m_sum;
    double     m_isum; // inclusive sum

    std::mutex m_lock;
    Config*    m_config;
};

//
// --- AnyKernel
//

class AnyKernel : public AggregateKernel {
public:

    class Config : public AggregateKernelConfig {
        std::string m_target_attr_name;

        Attribute   m_target_attr;
        Attribute   m_any_attr;

    public:

        Attribute get_target_attr(CaliperMetadataAccessInterface& db) {
            if (m_target_attr == Attribute::invalid) {
                m_target_attr = db.get_attribute(m_target_attr_name);

                if (m_target_attr != Attribute::invalid)
                    if (!m_target_attr.store_as_value())
                        Log(0).stream() << "any(" << m_target_attr_name << "): Attribute "
                                        << m_target_attr_name
                                        << " does not have CALI_ATTR_ASVALUE property!"
                                        << std::endl;
            }

            return m_target_attr;
        }

        Attribute get_any_attr(CaliperMetadataAccessInterface& db) {
            if (m_target_attr == Attribute::invalid)
                return Attribute::invalid;

            if (m_any_attr == Attribute::invalid)
                m_any_attr = db.create_attribute(std::string("any#") + m_target_attr_name, m_target_attr.type(),
                                                 CALI_ATTR_SKIP_EVENTS | CALI_ATTR_ASVALUE);

            return m_any_attr;
        }

        AggregateKernel* make_kernel() {
            return new AnyKernel(this);
        }

        Config(const std::string& name, bool inclusive)
            : m_target_attr_name(name),
              m_target_attr(Attribute::invalid),
              m_any_attr(Attribute::invalid)
            { }

        static AggregateKernelConfig* create(const std::vector<std::string>& cfg) {
            return new Config(cfg.front(), false);
        }
    };

    AnyKernel(Config* config)
        : m_count(0), m_config(config)
        { }

    const AggregateKernelConfig* config() { return m_config; }

    virtual void aggregate(CaliperMetadataAccessInterface& db, const EntryList& rec) {
        std::lock_guard<std::mutex>
            g(m_lock);

        if (m_val.empty()) {
            Attribute target_attr = m_config->get_target_attr(db);

            if (target_attr == Attribute::invalid)
                return;

            Attribute any_attr = m_config->get_any_attr(db);

            cali_id_t tgt_id = target_attr.id();
            cali_id_t any_id = any_attr.id();

            for (const Entry& e : rec) {
                if (e.attribute() == tgt_id || e.attribute() == any_id) {
                    m_val = e.value();
                    ++m_count;
                    break;
                }
            }
        }
    }

    virtual void append_result(CaliperMetadataAccessInterface& db, EntryList& rec) {
        if (m_count > 0)
            rec.push_back(Entry(m_config->get_any_attr(db), m_val));
    }

private:

    unsigned   m_count;
    Variant    m_val;
    std::mutex m_lock;
    Config*    m_config;
};


enum KernelID {
    Count         = 0,
    Sum           = 1,
    ScaledRatio   = 2,
    PercentTotal  = 3,
    InclusiveSum  = 4,
    Min           = 5,
    Max           = 6,
    Avg           = 7,
    ScaledSum     = 8,
    IScaledSum    = 9,
    IPercentTotal = 10,
    Any           = 11,
    ScaledCount   = 12,
    IRatio        = 13
};

#define MAX_KERNEL_ID IRatio

const char* kernel_args[]  = { "attribute" };
const char* sratio_args[]  = { "numerator", "denominator", "scale" };
const char* scale_args[]   = { "attribute", "scale" };
const char* scount_args[]  = { "scale" };

const QuerySpec::FunctionSignature kernel_signatures[] = {
    { KernelID::Count,         "count",         0, 0, nullptr      },
    { KernelID::Sum,           "sum",           1, 1, kernel_args  },
    { KernelID::ScaledRatio,   "ratio",         2, 3, sratio_args  },
    { KernelID::PercentTotal,  "percent_total", 1, 1, kernel_args  },
    { KernelID::InclusiveSum,  "inclusive_sum", 1, 1, kernel_args  },
    { KernelID::Min,           "min",           1, 1, kernel_args  },
    { KernelID::Max,           "max",           1, 1, kernel_args  },
    { KernelID::Avg,           "avg",           1, 1, kernel_args  },
    { KernelID::ScaledSum,     "scale",         2, 2, scale_args   },
    { KernelID::IScaledSum,    "inclusive_scale", 2, 2, scale_args },
    { KernelID::IPercentTotal, "inclusive_percent_total", 1, 1, kernel_args },
    { KernelID::Any,           "any",           1, 1, kernel_args  },
    { KernelID::ScaledCount,   "scale_count",   1, 1, scount_args  },
    { KernelID::IRatio,        "inclusive_ratio", 2, 3, sratio_args },

    QuerySpec::FunctionSignatureTerminator
};

const struct KernelInfo {
    const char* name;
    AggregateKernelConfig* (*create)(const std::vector<std::string>& cfg);
} kernel_list[] = {
    { "count",           CountKernel::Config::create         },
    { "sum",             SumKernel::Config::create           },
    { "ratio",           ScaledRatioKernel::Config::create   },
    { "percent_total",   PercentTotalKernel::Config::create  },
    { "inclusive_sum",   SumKernel::Config::create_inclusive },
    { "min",             MinKernel::Config::create           },
    { "max",             MaxKernel::Config::create           },
    { "avg",             AvgKernel::Config::create           },
    { "scale",           ScaledSumKernel::Config::create     },
    { "inclusive_scale", ScaledSumKernel::Config::create_inclusive },
    { "inclusive_percent_total", PercentTotalKernel::Config::create_inclusive },
    { "any",             AnyKernel::Config::create           },
    { "scale_count",     ScaledCountKernel::Config::create   },
    { "inclusive_ratio", ScaledRatioKernel::Config::create_inclusive },
    { 0, 0 }
};

} // namespace [anonymous]


struct Aggregator::AggregatorImpl
{
    // --- data

    vector<string>         m_key_strings;
    vector<Attribute>      m_key_attrs;
    std::mutex             m_key_lock;

    bool                   m_select_all;
    bool                   m_select_nested;

    vector<AggregateKernelConfig*> m_kernel_configs;

    struct TrieNode {
        uint32_t next[256] = { 0 };
        vector<AggregateKernel*> kernels;

        ~TrieNode() {
            for (AggregateKernel* k : kernels)
                delete k;
            kernels.clear();
        }
    };

    std::vector<TrieNode*> m_trie;
    std::mutex             m_trie_lock;

    //
    // --- parse config
    //

    void configure(const QuerySpec& spec) {
        //
        // --- key config
        //

        m_kernel_configs.clear();
        m_key_strings.clear();

        m_select_all    = false;
        m_select_nested = false;

        switch (spec.aggregation_key.selection) {
        case QuerySpec::AttributeSelection::Default:
        case QuerySpec::AttributeSelection::All:
            m_select_all  = true;
            break;
        case QuerySpec::AttributeSelection::List:
            m_key_strings = spec.aggregation_key.list;
            break;
        default:
            ;
        }

        {
            auto it = std::find(m_key_strings.begin(), m_key_strings.end(),
                                "prop:nested");

            if (it != m_key_strings.end()) {
                m_select_nested = true;
                m_key_strings.erase(it);
            }
        }

        //
        // --- kernel config
        //

        m_kernel_configs.clear();

        switch (spec.aggregation_ops.selection) {
        case QuerySpec::AggregationSelection::Default:
        case QuerySpec::AggregationSelection::All:
            m_kernel_configs.push_back(CountKernel::Config::create(vector<string>()));
            // TODO: pick class.aggregatable attributes
            break;
        case QuerySpec::AggregationSelection::List:
            for (const QuerySpec::AggregationOp& k : spec.aggregation_ops.list) {
                if (k.op.id >= 0 && k.op.id <= MAX_KERNEL_ID) {
                    m_kernel_configs.push_back((*::kernel_list[k.op.id].create)(k.args));
                } else {
                    Log(0).stream() << "aggregator: Error: Unknown aggregation kernel "
                                    << k.op.id << " (" << (k.op.name ? k.op.name : "") << ")"
                                    << std::endl;
                }
            }
            break;
        case QuerySpec::AggregationSelection::None:
            break;
        }
    }

    //
    // --- aggregation db ops
    //

    TrieNode* get_trienode(uint32_t id) {
        // Assume trie is locked!!

        if (id >= m_trie.size())
            m_trie.resize(id+1);

        if (!m_trie[id])
            m_trie[id] = new TrieNode;

        return m_trie[id];
    }

    TrieNode* find_trienode(size_t n, unsigned char* key) {
        std::lock_guard<std::mutex>
            g(m_trie_lock);

        TrieNode* trie = get_trienode(0);

        for ( size_t i = 0; trie && i < n; ++i ) {
            uint32_t id = trie->next[key[i]];

            if (!id) {
                id = static_cast<uint32_t>(m_trie.size()+1);
                trie->next[key[i]] = id;
            }

            trie = get_trienode(id);
        }

        if (trie->kernels.empty())
            for (AggregateKernelConfig* c : m_kernel_configs)
                trie->kernels.push_back(c->make_kernel());

        return trie;
    }

    //
    // --- snapshot processing
    //

    std::vector<Attribute> update_key_attributes(CaliperMetadataAccessInterface& db) {
        std::lock_guard<std::mutex>
            g(m_key_lock);

        auto it = m_key_strings.begin();

        while (it != m_key_strings.end()) {
            Attribute attr = db.get_attribute(*it);

            if (attr != Attribute::invalid) {
                m_key_attrs.push_back(attr);
                it = m_key_strings.erase(it);
            } else
                ++it;
        }

        return m_key_attrs;
    }

    inline bool is_key(const CaliperMetadataAccessInterface& db, const std::vector<Attribute>& key_attrs, cali_id_t attr_id) {
        Attribute attr = db.get_attribute(attr_id);

        if (m_select_nested && attr.is_nested())
            return true;

        for (const Attribute& key_attr : key_attrs) {
            if (key_attr == attr)
                return true;
            if (!attr.get(key_attr).empty())
                return true;
        }

        return false;
    }

    inline size_t pack_key(const Node* key_node, const vector<Entry>& immediates, unsigned char* key) {
        unsigned char node_key[10];
        size_t        node_key_len  = 0;

        if (key_node)
            node_key_len = vlenc_u64(key_node->id(), node_key);

        uint64_t      num_immediate = 0;
        unsigned char imm_key[MAX_KEYLEN];
        size_t        imm_key_len = 0;

        for (const Entry& e : immediates) {
            unsigned char buf[32]; // max space for one immediate entry
            size_t        p = 0;

            p += vlenc_u64(e.attribute(), buf+p);
            p += e.value().pack(buf+p);

            // discard entry if it won't fit in the key buf :-(
            if (p + imm_key_len + node_key_len + 1 >= MAX_KEYLEN)
                break;

            ++num_immediate;
            memcpy(imm_key+imm_key_len, buf, p);
            imm_key_len += p;
        }

        size_t pos = vlenc_u64(2*num_immediate + (key_node ? 1 : 0), key);

        memcpy(key+pos, node_key, node_key_len);
        pos += node_key_len;
        memcpy(key+pos, imm_key,  imm_key_len);
        pos += imm_key_len;

        return pos;
    }

    TrieNode* get_aggregation_entry(std::vector<const Node*>::const_iterator nodes_begin,
                                    std::vector<const Node*>::const_iterator nodes_end,
                                    const std::vector<Entry>& immediates, CaliperMetadataAccessInterface& db) {
        std::vector<const Node*> rv_nodes(nodes_end - nodes_begin);

        std::reverse_copy(nodes_begin, nodes_end,
                          rv_nodes.begin());

        const Node*   key_node = db.make_tree_entry(rv_nodes.size(), rv_nodes.data());

        // --- Pack key

        unsigned char key[MAX_KEYLEN];
        size_t        pos  = pack_key(key_node, immediates, key);

        return find_trienode(pos, key);
    }

    void process(CaliperMetadataAccessInterface& db, const EntryList& list) {
        std::vector<Attribute>   key_attrs = update_key_attributes(db);

        // --- Unravel nodes, filter for key attributes

        std::vector<const Node*> nodes;
        std::vector<Entry>       immediates;

        nodes.reserve(80);
        immediates.reserve(key_attrs.size());

        bool select_all = m_select_all;

        for (const Entry& e : list)
            for (const Node* node = e.node(); node && node->attribute() != CALI_INV_ID; node = node->parent())
                if (select_all || is_key(db, key_attrs, node->attribute()))
                    nodes.push_back(node);

        // Only include explicitly selected immediate entries in the key.
        for (const Entry& e : list)
            if (e.is_immediate() && is_key(db, key_attrs, e.attribute()))
                immediates.push_back(e);

        // --- Group by attribute, reverse nodes (restores original order) and get/create tree node.
        //       Keeps nested attributes separate.

        auto nonnested_begin = std::stable_partition(nodes.begin(), nodes.end(), [&db](const Node* node) {
                return db.get_attribute(node->attribute()).is_nested(); } );

        std::stable_sort(nonnested_begin, nodes.end(), [](const Node* a, const Node* b) {
                return a->attribute() < b->attribute(); } );
        std::sort(immediates.begin(), immediates.end(), [](const Entry& a, const Entry& b){
                return a.attribute() < b.attribute(); } );

        TrieNode* trie = get_aggregation_entry(nodes.begin(), nodes.end(), immediates, db);

        if (!trie)
            return;

        // --- Aggregate

        for (size_t k = 0; k < trie->kernels.size(); ++k) {
            trie->kernels[k]->aggregate(db, list);

            // for inclusive kernels, aggregate for all parent nodes as well

            if (trie->kernels[k]->config()->is_inclusive() && nodes.begin() != nonnested_begin) {
                auto it = nodes.begin();

                for (++it; it != nonnested_begin; ++it) {
                    TrieNode* p_trie = get_aggregation_entry(it, nodes.end(), immediates, db);

                    if (!p_trie)
                        break;

                    p_trie->kernels[k]->parent_aggregate(db, list);
                }
            }
        }
    }

    //
    // --- Flush
    //

    void unpack_key(const unsigned char* key, const CaliperMetadataAccessInterface& db, EntryList& list) {
        // key format: 2*N + node flag, node id, N * (attr id, Variant)

        size_t   p = 0;
        uint64_t n = vldec_u64(key + p, &p);

        if (n % 2 == 1)
            list.push_back(Entry(db.node(vldec_u64(key + p, &p))));

        for (unsigned i = 0; i < n/2; ++i) {
            bool      ok   = true;

            cali_id_t id   = static_cast<cali_id_t>(vldec_u64(key+p, &p));
            Variant   data = Variant::unpack(key+p, &p, &ok);

            if (ok)
                list.push_back(Entry(db.get_attribute(id), data));
        }
    }

    void recursive_flush(size_t n, unsigned char* key, TrieNode* trie,
                         CaliperMetadataAccessInterface& db, const SnapshotProcessFn push) {
        if (!trie)
            return;

        // --- Write current entry (if it represents a snapshot)

        if (!trie->kernels.empty()) {
            EntryList list;

            // Decode & add key node entry
            unpack_key(key, db, list);

            // Write aggregation variables
            for (AggregateKernel* k : trie->kernels)
                k->append_result(db, list);

            push(db, list);
        }

        // --- Recursively iterate over subsequent DB entries

        unsigned char* next_key = static_cast<unsigned char*>(alloca(n+2));

        memset(next_key, 0, n+2);
        memcpy(next_key, key, n);

        for (size_t i = 0; i < 256; ++i) {
            if (trie->next[i] == 0)
                continue;

            TrieNode* next = get_trienode(trie->next[i]);
            next_key[n]    = static_cast<unsigned char>(i);

            recursive_flush(n+1, next_key, next, db, push);
        }
    }

    void flush(CaliperMetadataAccessInterface& db, const SnapshotProcessFn push) {
        // NOTE: No locking: we assume flush() runs serially!

        TrieNode*     trie = get_trienode(0);
        unsigned char key  = 0;

        recursive_flush(0, &key, trie, db, push);
    }

    AggregatorImpl()
        : m_select_all(false)
    {
        m_trie.reserve(4096);
    }

    AggregatorImpl(const QuerySpec& spec)
        : m_select_all(false)
    {
        configure(spec);
        m_trie.reserve(4096);
    }

    ~AggregatorImpl() {
        for (TrieNode* e : m_trie)
            delete e;

        m_trie.clear();

        for (AggregateKernelConfig* c : m_kernel_configs)
            delete c;

        m_kernel_configs.clear();
    }
};

Aggregator::Aggregator(const QuerySpec& spec)
    : mP { new AggregatorImpl(spec) }
{
}

Aggregator::~Aggregator()
{
    mP.reset();
}

void
Aggregator::flush(CaliperMetadataAccessInterface& db, SnapshotProcessFn push)
{
    mP->flush(db, push);
}

void
Aggregator::add(CaliperMetadataAccessInterface& db, const EntryList& list)
{
    mP->process(db, list);
}

const QuerySpec::FunctionSignature*
Aggregator::aggregation_defs()
{
    return ::kernel_signatures;
}

std::string
Aggregator::get_aggregation_attribute_name(const QuerySpec::AggregationOp& op)
{
    switch (op.op.id) {
    case KernelID::Count:
        return "count";
    case KernelID::Sum:
        return std::string("sum#") + op.args[0];
    case KernelID::ScaledRatio:
        return std::string("ratio#") + op.args[0] + std::string("/") + op.args[1];
    case KernelID::PercentTotal:
        return std::string("percent_total#") + op.args[0];
    case KernelID::InclusiveSum:
        return std::string("inclusive#") + op.args[0];
    case KernelID::Min:
        return std::string("min#") + op.args[0];
    case KernelID::Max:
        return std::string("max#") + op.args[0];
    case KernelID::Avg:
        return std::string("avg#") + op.args[0];
    case KernelID::ScaledSum:
        return std::string("scale#") + op.args[0];
    case KernelID::IScaledSum:
        return std::string("iscale#") + op.args[0];
    case KernelID::IPercentTotal:
        return std::string("ipercent_total#") + op.args[0];
    case KernelID::Any:
        return std::string("any#") + op.args[0];
    case KernelID::ScaledCount:
        return std::string("scount");
    case KernelID::IRatio:
        return std::string("iratio#") + op.args[0] + std::string("/") + op.args[1];
    }

    return std::string();
}
