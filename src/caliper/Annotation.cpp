///@ file Annotation.cpp
///@ Annotation interface template specializations


#include "Annotation.h"
#include "Caliper.h"

using namespace std;
using namespace cali;


// --- Constructors / destructor

Annotation::Annotation(const string& name, int opt)
    : m_attr { Attribute::invalid }, m_name { name }, m_opt { opt }, m_depth { 0 }
{
    m_attr = Caliper::instance()->get_attribute(name);
}

Annotation::~Annotation() {
    while (!(m_opt & KeepAlive) && m_depth-- > 0)
        end();
}


// --- begin() overloads

cali_err Annotation::begin(int data)
{
    // special case: allow assignment of int values to 'double' attributes
    if (m_attr.type() == CALI_TYPE_DOUBLE)
        return begin(static_cast<double>(data));

    int64_t buf = static_cast<int64_t>(data);
    return begin(CALI_TYPE_INT, &buf, sizeof(int64_t));
}

cali_err Annotation::begin(double data)
{
    return begin(CALI_TYPE_DOUBLE, &data, sizeof(double));
}

cali_err Annotation::begin(const string& data)
{
    return begin(CALI_TYPE_STRING, data.data(), data.size());
}

// --- begin() workhorse

cali_err Annotation::begin(cali_attr_type type, const void* data, size_t size)
{
    if (m_attr == Attribute::invalid)
        create_attribute(type);

    if (!(m_attr.type() == type))
        return CALI_EINV;

    Caliper*  c = Caliper::instance();
    cali_err ret = c->begin(c->current_environment(), m_attr, data, size);

    if (ret == CALI_SUCCESS)
        ++m_depth;

    return ret;
}


// --- set() overloads

cali_err Annotation::set(int data)
{
    // special case: allow assignment of int values to 'double' attributes
    if (m_attr.type() == CALI_TYPE_DOUBLE)
        return set(static_cast<double>(data));

    int64_t buf = static_cast<int64_t>(data);
    return set(CALI_TYPE_INT, &buf, sizeof(int64_t));
}

cali_err Annotation::set(double data)
{
    return set(CALI_TYPE_DOUBLE, &data, sizeof(double));
}

cali_err Annotation::set(const string& data)
{
    return set(CALI_TYPE_STRING, data.c_str(), data.size());
}

// --- set() workhorse

cali_err Annotation::set(cali_attr_type type, const void* data, size_t size)
{
    if (m_attr == Attribute::invalid)
        create_attribute(type);

    if (!(m_attr.type() == type))
        return CALI_EINV;

    Caliper* c  = Caliper::instance();
    cali_err ret = c->set(c->current_environment(), m_attr, data, size);

    if (ret == CALI_SUCCESS)
        ++m_depth; // FIXME: really?

    return ret;
}

// --- set() static overloads

pair<Annotation, cali_err> Annotation::set(const string& name, int data, int opt)
{
    Annotation a { name, opt };
    cali_err  err = a.set(data);

    return make_pair(std::move(a), err);
}

pair<Annotation, cali_err> Annotation::set(const string& name, double data, int opt)
{
    Annotation a { name, opt };
    cali_err  err = a.set(data);

    return make_pair(std::move(a), err);
}

pair<Annotation, cali_err> Annotation::set(const string& name, const string& data, int opt)
{
    Annotation a { name, opt };
    cali_err  err = a.set(data);

    return make_pair(std::move(a), err);
}


// --- end()

cali_err Annotation::end()
{
    Caliper*  c = Caliper::instance();
    cali_err ret = c->end(c->current_environment(), m_attr);

    if (m_depth > 0)
        --m_depth;

    return ret;
}


// --- init_attribute 

void Annotation::create_attribute(cali_attr_type type)
{
    // Option -> cali_attr_properties map
    const int prop[] = {
        CALI_ATTR_DEFAULT,                   // Default      = 0
        CALI_ATTR_ASVALUE,                   // StoreAsValue = 1
        CALI_ATTR_NOMERGE,                   // NoMerge      = 2
        CALI_ATTR_ASVALUE | CALI_ATTR_NOMERGE // StoreAsValue | NoMerge = 3
    };

    m_attr = Caliper::instance()->create_attribute(m_name, type, prop[m_opt & 0x03]);
}
