// Copyright (c) 2020, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// Preprocessor implementation

#include "caliper/reader/Preprocessor.h"

#include "caliper/reader/QuerySpec.h"
#include "caliper/reader/RecordSelector.h"

#include "caliper/common/Attribute.h"
#include "caliper/common/CaliperMetadataAccessInterface.h"
#include "caliper/common/Log.h"
#include "caliper/common/Node.h"

#include "caliper/common/cali_types.h"

#include <cmath>
#include <vector>
#include <utility>

using namespace cali;

namespace
{

Variant get_value(const CaliperMetadataAccessInterface& db, const std::string& attr_name, Attribute& attr, const EntryList& rec)
{
    if (attr == Attribute::invalid)
        attr = db.get_attribute(attr_name);
    if (attr == Attribute::invalid)
        return Variant();

    for (const Entry& e : rec) {
        auto val = e.value(attr.id());
        if (!val.empty())
            return val;
    }

    return Variant();
}


class Kernel {
public:

    virtual void process(CaliperMetadataAccessInterface& db, EntryList& rec) = 0;

    virtual ~Kernel() {}
};

class ScaledRatioKernel : public Kernel
{
    std::string m_res_attr_name;
    std::string m_nom_attr_name;
    std::string m_dnm_attr_name;

    Attribute   m_res_attr;
    Attribute   m_nom_attr;
    Attribute   m_dnm_attr;

    double      m_scale;

    ScaledRatioKernel(const std::string& def, const std::vector<std::string>& args)
        : m_res_attr_name(def),
          m_nom_attr_name(args[0]),
          m_dnm_attr_name(args[1]),
          m_res_attr(Attribute::invalid),
          m_nom_attr(Attribute::invalid),
          m_dnm_attr(Attribute::invalid),
          m_scale(1.0)
        {
            if (args.size() > 2)
                m_scale = std::stod(args[2]);
        }

public:

    void process(CaliperMetadataAccessInterface& db, EntryList& rec) {
        Variant v_nom = get_value(db, m_nom_attr_name, m_nom_attr, rec);
        Variant v_dnm = get_value(db, m_dnm_attr_name, m_dnm_attr, rec);

        if (v_nom.empty() || v_dnm.empty())
            return;

        double dnm = v_dnm.to_double();

        if (!(std::fabs(dnm) > 0.0))
            return;

        double val = m_scale * (v_nom.to_double() / dnm);

        if (m_res_attr == Attribute::invalid)
            m_res_attr = db.create_attribute(m_res_attr_name, CALI_TYPE_DOUBLE,
                            CALI_ATTR_SKIP_EVENTS |
                            CALI_ATTR_ASVALUE);

        rec.push_back(Entry(m_res_attr, Variant(val)));
    }

    static Kernel* create(const std::string& def, const std::vector<std::string>& args) {
        return new ScaledRatioKernel(def, args);
    }
};


class ScaleKernel : public Kernel
{
    std::string m_res_attr_name;
    std::string m_tgt_attr_name;

    Attribute   m_res_attr;
    Attribute   m_tgt_attr;

    double      m_scale;

    ScaleKernel(const std::string& def, const std::vector<std::string>& args)
        : m_res_attr_name(def),
          m_tgt_attr_name(args[0]),
          m_res_attr(Attribute::invalid),
          m_tgt_attr(Attribute::invalid),
          m_scale(1.0)
        {
            m_scale = std::stod(args[1]);
        }

public:

    void process(CaliperMetadataAccessInterface& db, EntryList& rec) {
        Variant v_tgt = get_value(db, m_tgt_attr_name, m_tgt_attr, rec);

        if (v_tgt.empty())
            return;

        if (m_res_attr == Attribute::invalid)
            m_res_attr = db.create_attribute(m_res_attr_name, CALI_TYPE_DOUBLE,
                            CALI_ATTR_SKIP_EVENTS |
                            CALI_ATTR_ASVALUE);

        rec.push_back(Entry(m_res_attr, Variant(m_scale * v_tgt.to_double())));
    }

    static Kernel* create(const std::string& def, const std::vector<std::string>& args) {
        return new ScaleKernel(def, args);
    }
};


class TruncateKernel : public Kernel
{
    std::string m_res_attr_name;
    std::string m_tgt_attr_name;

    Attribute   m_res_attr;
    Attribute   m_tgt_attr;

    double      m_factor;

    TruncateKernel(const std::string& def, const std::vector<std::string>& args)
        : m_res_attr_name(def),
          m_tgt_attr_name(args[0]),
          m_res_attr(Attribute::invalid),
          m_tgt_attr(Attribute::invalid),
          m_factor(1.0)
        {
            if (args.size() > 1)
                m_factor = std::stod(args[1]);
        }

public:

    void process(CaliperMetadataAccessInterface& db, EntryList& rec) {
        Variant v_tgt = get_value(db, m_tgt_attr_name, m_tgt_attr, rec);

        if (v_tgt.empty())
            return;

        cali_attr_type type = m_tgt_attr.type();

        if (!(type == CALI_TYPE_INT || type == CALI_TYPE_UINT))
            type = CALI_TYPE_DOUBLE;

        if (m_res_attr == Attribute::invalid)
            m_res_attr = db.create_attribute(m_res_attr_name, type,
                            CALI_ATTR_SKIP_EVENTS |
                            CALI_ATTR_ASVALUE);

        double val = v_tgt.to_double();
        double res = val - fmod(val, m_factor);

        Variant v_res(res);

        if (type == CALI_TYPE_INT)
            v_res = cali_make_variant_from_int(static_cast<int>(res));
        else if (type == CALI_TYPE_UINT)
            v_res = cali_make_variant_from_uint(static_cast<uint64_t>(fabs(res)));

        rec.push_back(Entry(m_res_attr, Variant(v_res)));
    }

    static Kernel* create(const std::string& def, const std::vector<std::string>& args) {
        return new TruncateKernel(def, args);
    }
};

class FirstKernel : public Kernel
{
    std::string m_res_attr_name;
    Attribute   m_res_attr;

    std::vector<std::string> m_tgt_attr_names;
    std::vector<Attribute>   m_tgt_attrs;

public:

    FirstKernel(const std::string& def, const std::vector<std::string>& args)
        : m_res_attr_name(def),
          m_res_attr(Attribute::invalid),
          m_tgt_attr_names(args)
        {
            m_tgt_attrs.assign(args.size(), Attribute::invalid);
        }

    void process(CaliperMetadataAccessInterface& db, EntryList& rec) {
        for (size_t i = 0; i < m_tgt_attrs.size(); ++i) {
            Variant v_tgt = get_value(db, m_tgt_attr_names[i], m_tgt_attrs[i], rec);

            if (v_tgt.empty())
                continue;

            cali_attr_type type = m_tgt_attrs[i].type();

            if (m_res_attr == Attribute::invalid)
                m_res_attr = db.create_attribute(m_res_attr_name, type,
                                CALI_ATTR_SKIP_EVENTS |
                                CALI_ATTR_ASVALUE);

            rec.push_back(Entry(m_res_attr, v_tgt));

            break;
        }
    }

    static Kernel* create(const std::string& def, const std::vector<std::string>& args) {
        return new FirstKernel(def, args);
    }

};

enum KernelID {
    ScaledRatio,
    Scale,
    Truncate,
    First,
};

const char* sratio_args[]  = { "numerator", "denominator", "scale" };
const char* scale_args[]   = { "attribute", "scale" };
const char* first_args[]   = {
    "attribute0", "attribute1", "attribute2",
    "attribute3", "attribute4", "attribute5",
    "attribute6", "attribute7", "attribute8"
};

const QuerySpec::FunctionSignature kernel_signatures[] = {
    { KernelID::ScaledRatio,   "ratio",         2, 3, sratio_args  },
    { KernelID::Scale,         "scale",         2, 2, scale_args   },
    { KernelID::Truncate,      "truncate",      1, 2, scale_args   },
    { KernelID::First,         "first",         1, 8, first_args   },

    QuerySpec::FunctionSignatureTerminator
};

typedef Kernel* (*KernelCreateFn)(const std::string& def, const std::vector<std::string>& args);

const KernelCreateFn kernel_create_fn[] = {
    ScaledRatioKernel::create,
    ScaleKernel::create,
    TruncateKernel::create,
    FirstKernel::create
};

constexpr int MAX_KERNEL_ID = 3;

}

struct Preprocessor::PreprocessorImpl
{
    std::vector< std::pair<RecordSelector, Kernel*> > kernels;

    void configure(const QuerySpec& spec) {
        for (const auto &pspec : spec.preprocess_ops) {
            int index = pspec.op.op.id;

            if (index >= 0 && index <= MAX_KERNEL_ID) {
                kernels.push_back(
                        std::make_pair(
                            RecordSelector(pspec.cond),
                            (*::kernel_create_fn[index])(pspec.target, pspec.op.args)
                        )
                    );
            }
        }
    }

    EntryList process(CaliperMetadataAccessInterface& db, const EntryList& rec) {
        EntryList ret = rec;

        for (auto &k : kernels)
            if (k.first.pass(db, ret))
                k.second->process(db, ret);

        return ret;
    }

    PreprocessorImpl(const QuerySpec& spec) {
        configure(spec);
    }

    ~PreprocessorImpl() {
        for (auto &k : kernels)
            delete k.second;
    }
};


Preprocessor::Preprocessor(const QuerySpec& spec)
    : mP( new PreprocessorImpl(spec) )
{ }

Preprocessor::~Preprocessor()
{ }

EntryList
Preprocessor::process(CaliperMetadataAccessInterface& db, const EntryList& rec)
{
    return mP->process(db, rec);
}

const QuerySpec::FunctionSignature*
Preprocessor::preprocess_defs()
{
    return ::kernel_signatures;
}
