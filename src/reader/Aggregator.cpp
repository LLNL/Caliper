// Copyright (c) 2015, Lawrence Livermore National Security, LLC.
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

// Aggregator implementation

#include "caliper/reader/Aggregator.h"

#include "caliper/reader/QuerySpec.h"

#include "caliper/common/Attribute.h"
#include "caliper/common/CaliperMetadataAccessInterface.h"
#include "caliper/common/Log.h"
#include "caliper/common/Node.h"

#include "caliper/common/cali_types.h"

#include "caliper/common/c-util/vlenc.h"

#include "caliper/common/util/split.hpp"

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

        static AggregateKernelConfig* create(const std::vector<std::string>& cfg) {
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


//
// --- SumKernel
//

class SumKernel : public AggregateKernel {
public:

    class Config : public AggregateKernelConfig {
        std::string m_aggr_attr_name;
        Attribute   m_aggr_attr;

    public:

        Attribute get_aggr_attr(CaliperMetadataAccessInterface& db) {
            if (m_aggr_attr == Attribute::invalid) {
                m_aggr_attr = db.get_attribute(m_aggr_attr_name);

                if (m_aggr_attr != Attribute::invalid)
                    if (!m_aggr_attr.store_as_value())
                        Log(0).stream() << "sum(" << m_aggr_attr_name << "): Attribute "
                                        << m_aggr_attr_name
                                        << " does not have CALI_ATTR_ASVALUE property!"
                                        << std::endl;
            }

            return m_aggr_attr;
        }

        AggregateKernel* make_kernel() {
            return new SumKernel(this);
        }

        Config(const std::string& name)
            : m_aggr_attr_name(name),
              m_aggr_attr(Attribute::invalid)
            { }

        static AggregateKernelConfig* create(const std::vector<std::string>& cfg) {
            return new Config(cfg.front());
        }
    };

    SumKernel(Config* config)
        : m_count(0), m_config(config)
        { }

    const AggregateKernelConfig* config() { return m_config; }

    virtual void aggregate(CaliperMetadataAccessInterface& db, const EntryList& list) {
        std::lock_guard<std::mutex>
            g(m_lock);

        Attribute aggr_attr = m_config->get_aggr_attr(db);

        if (aggr_attr == Attribute::invalid)
            return;

        for (const Entry& e : list) {
            if (e.attribute() == aggr_attr.id()) {
                switch (aggr_attr.type()) {
                case CALI_TYPE_DOUBLE:
                    m_sum = Variant(m_sum.to_double() + e.value().to_double());
                    break;
                case CALI_TYPE_INT:
                    m_sum = Variant(m_sum.to_int()    + e.value().to_int()   );
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

    virtual void append_result(CaliperMetadataAccessInterface& db, EntryList& list) {
        if (m_count > 0)
            list.push_back(Entry(m_config->get_aggr_attr(db), m_sum));
    }

private:

    unsigned   m_count;
    Variant    m_sum;
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
            {
                Log(2).stream() << "aggregate: creating min kernel for attribute " << m_target_attr_name << std::endl;
            }

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

        switch (target_attr.type()) {
        case CALI_TYPE_DOUBLE:
        {
            for (const Entry& e : list) {
                if (e.attribute() == target_attr.id()) {
                    double val = e.value().to_double();

                    if (m_min.empty())
                        m_min = e.value();
                    else
                        m_min = Variant(std::min(m_min.to_double(), val));

                    ++m_count;
                } else if (e.attribute() == stat_attr.min.id()) {
                    if (m_min.empty())
                        m_min = e.value();
                    else
                        m_min = Variant(std::min(e.value().to_double(), m_min.to_double()));
                }
            }
        }
        break;
        case CALI_TYPE_INT:
        {
            for (const Entry& e : list) {
                if (e.attribute() == target_attr.id()) {
                    int val = e.value().to_int();

                    if (m_min.empty())
                        m_min = e.value();
                    else
                        m_min = Variant(std::min(m_min.to_int(), val));
                } else if (e.attribute() == stat_attr.min.id()) {
                    if (m_min.empty())
                        m_min = e.value();
                    else
                        m_min = Variant(std::min(e.value().to_int(), m_min.to_int()));
                }
            }
        }
        break;
        case CALI_TYPE_UINT:
        {
            for (const Entry& e : list) {
                if (e.attribute() == target_attr.id()) {
                    uint64_t val = e.value().to_uint();

                    if (m_min.empty())
                        m_min = e.value();
                    else
                        m_min = Variant(std::min(m_min.to_uint(), val));
                } else if (e.attribute() == stat_attr.min.id()) {
                    if (m_min.empty())
                        m_min = e.value();
                    else
                        m_min = Variant(std::min(e.value().to_uint(), m_min.to_uint()));
                }
            }
        }
        break;
        default:
            // some error?
            ;
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

    unsigned   m_count;
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
            {
                Log(2).stream() << "aggregate: creating max kernel for attribute " << m_target_attr_name << std::endl;
            }

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

        switch (target_attr.type()) {
        case CALI_TYPE_DOUBLE:
        {
            for (const Entry& e : list) {
                if (e.attribute() == target_attr.id()) {
                    double val = e.value().to_double();

                    if (m_max.empty())
                        m_max = e.value();
                    else
                        m_max = Variant(std::max(m_max.to_double(), val));
                } else if (e.attribute() == stat_attr.max.id()) {
                    if (m_max.empty())
                        m_max = e.value();
                    else
                        m_max = Variant(std::max(e.value().to_double(), m_max.to_double()));
                }
            }
        }
        break;
        case CALI_TYPE_INT:
        {
            for (const Entry& e : list) {
                if (e.attribute() == target_attr.id()) {
                    int val = e.value().to_int();

                    if (m_max.empty())
                        m_max = e.value();
                    else
                        m_max = Variant(std::max(m_max.to_int(), val));
                } else if (e.attribute() == stat_attr.max.id()) {
                    if (m_max.empty())
                        m_max = e.value();
                    else
                        m_max = Variant(std::max(e.value().to_int(), m_max.to_int()));
                }
            }
        }
        break;
        case CALI_TYPE_UINT:
        {
            for (const Entry& e : list) {
                if (e.attribute() == target_attr.id()) {
                    uint64_t val = e.value().to_uint();

                    if (m_max.empty())
                        m_max = e.value();
                    else
                        m_max = Variant(std::max(m_max.to_uint(), val));
                } else if (e.attribute() == stat_attr.max.id()) {
                    if (m_max.empty())
                        m_max = e.value();
                    else
                        m_max = Variant(std::max(e.value().to_uint(), m_max.to_uint()));
                }
            }
        }
        break;
        default:
            // some error?
            ;
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

            cali_attr_type type = m_target_attr.type();
            int            prop = CALI_ATTR_SKIP_EVENTS | CALI_ATTR_ASVALUE;

            m_stat_attrs.sum =
                db.create_attribute("sum#" + m_target_attr_name, type, prop);
            m_stat_attrs.avg =
                db.create_attribute("avg#" + m_target_attr_name, CALI_TYPE_DOUBLE, prop);
            m_stat_attrs.count =
                db.create_attribute("avg.count#" + m_target_attr_name, CALI_TYPE_UINT, prop | CALI_ATTR_HIDDEN);

            a = m_stat_attrs;
            return true;
        }

        AggregateKernel* make_kernel() {
            return new AvgKernel(this);
        }

        Config(const std::string& name)
            : m_target_attr_name(name),
              m_target_attr(Attribute::invalid)
            {
                Log(2).stream() << "aggregate: creating avg kernel for attribute " << m_target_attr_name << std::endl;
            }

        static AggregateKernelConfig* create(const std::vector<std::string>& cfg) {
            return new Config(cfg.front());
        }
    };

    AvgKernel(Config* config)
        : m_count(0), m_sum(0), m_config(config)
        { }

    const AggregateKernelConfig* config() { return m_config; }

    virtual void aggregate(CaliperMetadataAccessInterface& db, const EntryList& list) {
        std::lock_guard<std::mutex>
            g(m_lock);

        Attribute target_attr = m_config->get_target_attr(db);
        StatisticsAttributes stat_attr;

        if (!m_config->get_statistics_attributes(db, stat_attr))
            return;

        switch (target_attr.type()) {
        case CALI_TYPE_DOUBLE:
        {
            for (const Entry& e : list) {
                if (e.attribute() == target_attr.id()) {
                    double val = e.value().to_double();

                    m_sum = Variant(m_sum.to_double() + val);

                    ++m_count;
                } else if (e.attribute() == stat_attr.sum.id()) {
                    m_sum = Variant(e.value().to_double() + m_sum.to_double());
                } else if (e.attribute() == stat_attr.count.id()) {
                    m_count += e.value().to_uint();
                }
            }
        }
        break;
        case CALI_TYPE_INT:
        {
            for (const Entry& e : list) {
                if (e.attribute() == target_attr.id()) {
                    int val = e.value().to_int();

                    m_sum = Variant(m_sum.to_int() + val);
                    ++m_count;
                } else if (e.attribute() == stat_attr.sum.id()) {
                    m_sum = Variant(e.value().to_int() + m_sum.to_int());
                } else if (e.attribute() == stat_attr.count.id()) {
                    m_count += e.value().to_uint();
                }
            }
        }
        break;
        case CALI_TYPE_UINT:
        {
            for (const Entry& e : list) {
                if (e.attribute() == target_attr.id()) {
                    uint64_t val = e.value().to_uint();

                    m_sum = Variant(m_sum.to_uint() + val);

                    ++m_count;
                } else if (e.attribute() == stat_attr.sum.id()) {
                    m_sum = Variant(e.value().to_uint() + m_sum.to_uint());
                } else if (e.attribute() == stat_attr.count.id()) {
                    m_count += e.value().to_uint();
                }
            }
        }
        break;
        default:
            // some error?
            ;
        }
    }

    virtual void append_result(CaliperMetadataAccessInterface& db, EntryList& list) {
        if (m_count > 0) {
            StatisticsAttributes stat_attr;

            if (!m_config->get_statistics_attributes(db, stat_attr))
                return;

            list.push_back(Entry(stat_attr.avg, Variant(m_sum.to_double() / m_count)));
            list.push_back(Entry(stat_attr.sum, m_sum));
            list.push_back(Entry(stat_attr.count, Variant(cali_make_variant_from_uint(m_count))));
        }
    }

private:

    unsigned   m_count;
    Variant    m_sum;

    std::mutex m_lock;

    Config*    m_config;
};

//
// --- PercentageKernel
//

class PercentageKernel : public AggregateKernel {
public:

    class Config : public AggregateKernelConfig {
        std::string          m_target_attr1_name;
        std::string          m_target_attr2_name;
        Attribute            m_target_attr1;
        Attribute            m_target_attr2;
        Attribute            m_sum1_attr;
        Attribute            m_sum2_attr;

        Attribute m_percentage_attr;

    public:

        std::pair<Attribute,Attribute> get_target_attrs(CaliperMetadataAccessInterface& db) {
            if (m_target_attr1 == Attribute::invalid) {
                m_target_attr1 = db.get_attribute(m_target_attr1_name);

                if (m_target_attr1 != Attribute::invalid)
                    if (!m_target_attr1.store_as_value())
                        Log(0).stream() << "percentage(): Attribute "
                                        << m_target_attr1_name
                                        << " does not have CALI_ATTR_ASVALUE property!"
                                        << std::endl;
            }

            if (m_target_attr2 == Attribute::invalid) {
                m_target_attr2 = db.get_attribute(m_target_attr2_name);

                if (m_target_attr2 != Attribute::invalid)
                    if (!m_target_attr2.store_as_value())
                        Log(0).stream() << "percentage(): Attribute "
                                        << m_target_attr2_name
                                        << " does not have CALI_ATTR_ASVALUE property!"
                                        << std::endl;
            }


            return std::pair<Attribute,Attribute>(m_target_attr1, m_target_attr2);
        }

        bool get_percentage_attributes(CaliperMetadataAccessInterface& db,
                                       Attribute& percentage_attr,
                                       Attribute& sum1_attr,
                                       Attribute& sum2_attr) {
            if (m_target_attr1 == Attribute::invalid)
                return false;
            if (m_target_attr2 == Attribute::invalid)
                return false;
            if (m_percentage_attr != Attribute::invalid) {
                percentage_attr = m_percentage_attr;
                sum1_attr = m_sum1_attr;
                sum2_attr = m_sum2_attr;
                return true;
            }

            m_percentage_attr =
                db.create_attribute(m_target_attr1_name + "/" + m_target_attr2_name,
                        CALI_TYPE_DOUBLE, CALI_ATTR_SKIP_EVENTS | CALI_ATTR_ASVALUE);

            m_sum1_attr =
                db.create_attribute("pc.sum#" + m_target_attr1_name,
                        CALI_TYPE_DOUBLE, CALI_ATTR_SKIP_EVENTS | CALI_ATTR_ASVALUE | CALI_ATTR_HIDDEN);

            m_sum2_attr =
                db.create_attribute("pc.sum#" + m_target_attr2_name,
                        CALI_TYPE_DOUBLE, CALI_ATTR_SKIP_EVENTS | CALI_ATTR_ASVALUE | CALI_ATTR_HIDDEN);

            percentage_attr = m_percentage_attr;
            sum1_attr = m_sum1_attr;
            sum2_attr = m_sum2_attr;

            return true;
        }

        AggregateKernel* make_kernel() {
            return new PercentageKernel(this);
        }

        Config(const std::vector<std::string>& names)
            : m_target_attr1_name(names.front()), // We have already checked that there are two strings given
              m_target_attr2_name(names.back()),
              m_target_attr1(Attribute::invalid),
              m_target_attr2(Attribute::invalid)
            {
                Log(2).stream() << "aggregate: creating percentage kernel for attributes "
                                << m_target_attr1_name << " / " << m_target_attr2_name <<std::endl;
            }

        static AggregateKernelConfig* create(const std::vector<std::string>& cfg) {
            return new Config(cfg);
        }
    };

    PercentageKernel(Config* config)
        : m_sum1(0), m_sum2(0), m_config(config)
        { }

    const AggregateKernelConfig* config() { return m_config; }

    virtual void aggregate(CaliperMetadataAccessInterface& db, const EntryList& list) {
        std::lock_guard<std::mutex>
            g(m_lock);

        std::pair<Attribute,Attribute> target_attrs = m_config->get_target_attrs(db);
        Attribute percentage_attr, sum1_attr, sum2_attr;

        if (!m_config->get_percentage_attributes(db, percentage_attr, sum1_attr, sum2_attr))
            return;

        for (const Entry& e : list) {
            if (e.attribute() == target_attrs.first.id()) {
                m_sum1 += e.value().to_double();
            } else if (e.attribute() == target_attrs.second.id()) {
                m_sum2 += e.value().to_double();
            } else if (e.attribute() == sum1_attr.id()) {
                m_sum1 += e.value().to_double();
            } else if (e.attribute() == sum2_attr.id()) {
                m_sum2 += e.value().to_double();
            }
        }
    }

    virtual void append_result(CaliperMetadataAccessInterface& db, EntryList& list) {
        if (m_sum2 > 0) {
            Attribute percentage_attr, sum1_attr, sum2_attr;

            if (!m_config->get_percentage_attributes(db, percentage_attr, sum1_attr, sum2_attr))
                return;

            list.push_back(Entry(sum1_attr, Variant(m_sum1)));
            list.push_back(Entry(sum2_attr, Variant(m_sum2)));
            list.push_back(Entry(percentage_attr, Variant(m_sum1 / m_sum2)));
        }
    }

private:

    double    m_sum1;
    double    m_sum2;

    std::mutex m_lock;

    Config*    m_config;
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
                db.create_attribute("percent_total#" + m_target_attr_name,
                                    CALI_TYPE_DOUBLE,
                                    CALI_ATTR_SKIP_EVENTS | CALI_ATTR_ASVALUE);

            m_sum_attr =
                db.create_attribute("pct.sum#" + m_target_attr_name,
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

        Config(const std::vector<std::string>& names)
            : m_target_attr_name(names.front()),
              m_target_attr(Attribute::invalid),
              m_total(0)
        {
            Log(2).stream() << "aggregate: creating percent_total kernel for attribute "
                            << m_target_attr_name << std::endl;
        }

        static AggregateKernelConfig* create(const std::vector<std::string>& cfg) {
            return new Config(cfg);
        }
    };

    PercentTotalKernel(Config* config)
        : m_sum(0), m_config(config)
        { }

    const AggregateKernelConfig* config() { return m_config; }

    virtual void aggregate(CaliperMetadataAccessInterface& db, const EntryList& list) {
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
                m_sum += val;
                m_config->add(val);
            }
        }
    }

    virtual void append_result(CaliperMetadataAccessInterface& db, EntryList& list) {
        double total = m_config->get_total();

        if (total > 0) {
            Attribute percentage_attr, sum_attr;

            if (!m_config->get_percentage_attribute(db, percentage_attr, sum_attr))
                return;

            list.push_back(Entry(sum_attr, Variant(m_sum)));
            list.push_back(Entry(percentage_attr, Variant(100.0 * m_sum / total)));
        }
    }

private:

    double     m_sum;

    std::mutex m_lock;
    Config*    m_config;
};

//
// --- InclusiveSumKernel
//

class InclusiveSumKernel : public AggregateKernel {
public:

    class Config : public AggregateKernelConfig {
        std::string m_target_attr_name;
        Attribute   m_target_attr;
        Attribute   m_sum_attr;

    public:

        Attribute get_target_attr(CaliperMetadataAccessInterface& db) {
            if (m_target_attr == Attribute::invalid) {
                m_target_attr = db.get_attribute(m_target_attr_name);

                if (m_target_attr != Attribute::invalid)
                    if (!m_target_attr.store_as_value())
                        Log(0).stream() << "inclusive_sum(" << m_target_attr_name << "): Attribute "
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
                m_sum_attr = db.create_attribute("inclusive#" + m_target_attr_name,
                                                 CALI_TYPE_DOUBLE,
                                                 CALI_ATTR_SKIP_EVENTS |
                                                 CALI_ATTR_ASVALUE     |
                                                 CALI_ATTR_HIDDEN);

            return m_sum_attr;
        }

        AggregateKernel* make_kernel() {
            return new InclusiveSumKernel(this);
        }

        bool is_inclusive() const { return true; }

        Config(const std::string& name)
            : m_target_attr_name(name),
              m_target_attr(Attribute::invalid)
            {
                Log(2).stream() << "creating inclusive sum kernel for " << m_target_attr_name << std::endl;
            }

        static AggregateKernelConfig* create(const std::vector<std::string>& cfg) {
            return new Config(cfg.front());
        }
    };

    InclusiveSumKernel(Config* config)
        : m_count(0), m_config(config)
        {
        }

    const AggregateKernelConfig* config() { return m_config; }

    virtual void aggregate(CaliperMetadataAccessInterface& db, const EntryList& list) {
        std::lock_guard<std::mutex>
            g(m_lock);

        Attribute aggr_attr = m_config->get_target_attr(db);

        if (aggr_attr == Attribute::invalid)
            return;

        for (const Entry& e : list) {
            if (e.attribute() == aggr_attr.id()) {
                switch (aggr_attr.type()) {
                case CALI_TYPE_DOUBLE:
                    m_sum = Variant(m_sum.to_double() + e.value().to_double());
                    break;
                case CALI_TYPE_INT:
                    m_sum = Variant(m_sum.to_int()    + e.value().to_int()   );
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

    virtual void append_result(CaliperMetadataAccessInterface& db, EntryList& list) {
        if (m_count > 0)
            list.push_back(Entry(m_config->get_sum_attr(db), m_sum));
    }

private:

    unsigned   m_count;
    Variant    m_sum;
    std::mutex m_lock;
    Config*    m_config;
};


enum KernelID {
    Count        = 0,
    Sum          = 1,
    Percentage   = 2,
    PercentTotal = 3,
    InclusiveSum = 4,
    Min          = 5,
    Max          = 6,
    Avg          = 7
};

#define MAX_KERNEL_ID 7

const char* kernel_args[] = { "attribute" };
const char* kernel_2args[] = { "numerator", "denominator" };

const QuerySpec::FunctionSignature kernel_signatures[] = {
    { KernelID::Count,        "count",         0, 0, nullptr      },
    { KernelID::Sum,          "sum",           1, 1, kernel_args  },
    { KernelID::Percentage,   "percentage",    2, 2, kernel_2args },
    { KernelID::PercentTotal, "percent_total", 1, 1, kernel_args  },
    { KernelID::InclusiveSum, "inclusive_sum", 1, 1, kernel_args  },
    { KernelID::Min,          "min",           1, 1, kernel_args  },
    { KernelID::Max,          "max",           1, 1, kernel_args  },
    { KernelID::Avg,          "avg",           1, 1, kernel_args  },

    QuerySpec::FunctionSignatureTerminator
};

const struct KernelInfo {
    const char* name;
    AggregateKernelConfig* (*create)(const std::vector<std::string>& cfg);
} kernel_list[] = {
    { "count",         CountKernel::Config::create        },
    { "sum",           SumKernel::Config::create          },
    { "percentage",    PercentageKernel::Config::create   },
    { "percent_total", PercentTotalKernel::Config::create },
    { "inclusive_sum", InclusiveSumKernel::Config::create },
    { "min",           MinKernel::Config::create          },
    { "max",           MaxKernel::Config::create          },
    { "avg",           AvgKernel::Config::create          },
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

    void parse_key(const string& key) {
        util::split(key, ':', back_inserter(m_key_strings));
        m_select_all = m_key_strings.empty();
    }

    void parse_aggr_config(const string& configstr) {
        vector<string> aggregators;

        util::split(configstr, ':', back_inserter(aggregators));

        for (const string& s : aggregators) {
            string::size_type oparen = s.find_first_of('(');
            string::size_type cparen = s.find_last_of(')');

            string kernelname = s.substr(0, oparen);
            vector<string> kernelattrs;

            if (cparen != string::npos && cparen > oparen+1) {
                string kernelattrs_str = s.substr(oparen+1, cparen-oparen-1);
                util::split(kernelattrs_str, ',', back_inserter(kernelattrs));
            }

            const ::KernelInfo* ki = ::kernel_list;

            for ( ; ki->name && ki->create && kernelname != ki->name; ++ki)
                ;

            if (ki->create)
                m_kernel_configs.push_back((*ki->create)(kernelattrs));
            else
                Log(0).stream() << "aggregator: unknown aggregation kernel \"" << kernelname << "\"" << std::endl;
        }
    }

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

    bool is_key(const CaliperMetadataAccessInterface& db, const std::vector<Attribute>& key_attrs, cali_id_t attr_id) {
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

    size_t pack_key(const Node* key_node, const vector<Entry>& immediates, CaliperMetadataAccessInterface& db, unsigned char* key) {
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
        size_t        pos  = pack_key(key_node, immediates, db, key);

        return find_trienode(pos, key);
    }

    void process(CaliperMetadataAccessInterface& db, const EntryList& list) {
        std::vector<Attribute>   key_attrs = update_key_attributes(db);

        // --- Unravel nodes, filter for key attributes

        std::vector<const Node*> nodes;
        std::vector<const Node*> rv_nodes;
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

                    p_trie->kernels[k]->aggregate(db, list);
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

Aggregator::Aggregator(const string& aggr_config, const string& key)
    : mP { new AggregatorImpl() }
{
    mP->parse_aggr_config(aggr_config);
    mP->parse_key(key);
}

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
        return op.args[0];
    case KernelID::Percentage:
        return op.args[0] + std::string("/") + op.args[1];
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
    }

    return std::string();
}
