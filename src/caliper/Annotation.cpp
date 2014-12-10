///@ file Annotation.cpp
///@ Annotation interface template specializations


#include "Annotation.h"
#include "Caliper.h"

#include <Log.h>

using namespace std;
using namespace cali;


// --- Scope subclass

Annotation::Scope::~Scope()
{
    if (m_destruct) {
        Caliper* c { Caliper::instance() };
        c->end(c->current_environment(), m_attr);
    }
}


// --- Constructors / destructor

Annotation::Annotation(const string& name, int opt)
    : m_attr { Attribute::invalid }, m_name { name }, m_opt { opt }
{
    m_attr = Caliper::instance()->get_attribute(name);
}


// --- begin() overloads

Annotation::Scope Annotation::begin(int data)
{
    // special case: allow assignment of int values to 'double' attributes
    if (m_attr.type() == CALI_TYPE_DOUBLE)
        return begin(static_cast<double>(data));

    int64_t buf = static_cast<int64_t>(data);
    return begin(CALI_TYPE_INT, &buf, sizeof(int64_t));
}

Annotation::Scope Annotation::begin(double data)
{
    return begin(CALI_TYPE_DOUBLE, &data, sizeof(double));
}

Annotation::Scope Annotation::begin(const string& data)
{
    return begin(CALI_TYPE_STRING, data.data(), data.size());
}

// --- begin() workhorse

Annotation::Scope Annotation::begin(cali_attr_type type, const void* data, size_t size)
{
    if (m_attr == Attribute::invalid)
        create_attribute(type);

    if (!(m_attr.type() == type))
        return Scope(Attribute::invalid);

    Caliper*  c = Caliper::instance();
    c->begin(c->current_environment(), m_attr, data, size);

    return Scope(m_attr);
}


// --- set() overloads

Annotation::Scope Annotation::set(int data)
{
    // special case: allow assignment of int values to 'double' attributes
    if (m_attr.type() == CALI_TYPE_DOUBLE)
        return set(static_cast<double>(data));

    int64_t buf = static_cast<int64_t>(data);
    return set(CALI_TYPE_INT, &buf, sizeof(int64_t));
}

Annotation::Scope Annotation::set(double data)
{
    return set(CALI_TYPE_DOUBLE, &data, sizeof(double));
}

Annotation::Scope Annotation::set(const string& data)
{
    return set(CALI_TYPE_STRING, data.c_str(), data.size());
}

// --- set() workhorse

Annotation::Scope Annotation::set(cali_attr_type type, const void* data, size_t size)
{
    if (m_attr == Attribute::invalid)
        create_attribute(type);

    if (!(m_attr.type() == type))
        return Scope(Attribute::invalid);

    Caliper* c  = Caliper::instance();
    cali_err ret = c->set(c->current_environment(), m_attr, data, size);

    if (ret != CALI_SUCCESS)
        return Scope(Attribute::invalid);

    return Scope(m_attr);
}

// --- end()

void Annotation::end()
{
    Caliper* c { Caliper::instance() };
    c->end(c->current_environment(), m_attr);
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

    if (m_attr == Attribute::invalid)
        Log(0).stream() << "Could not create attribute " << m_name << endl;
}
