///@ file Annotation.cpp
///@ Annotation interface template specializations


#include "Annotation.h"
#include "Caliper.h"

#include <Log.h>

#include <cstring>

using namespace std;
using namespace cali;


// --- Scope subclass

Annotation::Scope::~Scope()
{
    if (m_destruct)
        Caliper::instance()->end(m_attr);
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
    // special case: allow assignment of int values to 'double' or 'uint' attributes
    if (m_attr.type() == CALI_TYPE_DOUBLE)
        return begin(Variant(static_cast<double>(data)));
    if (m_attr.type() == CALI_TYPE_UINT)
        return begin(Variant(static_cast<uint64_t>(data)));

    return begin(Variant(data));
}

Annotation::Scope Annotation::begin(double data)
{
    return begin(Variant(data));
}

Annotation::Scope Annotation::begin(const string& data)
{
    return begin(Variant(CALI_TYPE_STRING, data.data(), data.size()));
}

Annotation::Scope Annotation::begin(const char* data)
{
    return begin(Variant(CALI_TYPE_STRING, data, strlen(data)));
}

Annotation::Scope Annotation::begin(cali_attr_type type, const void* data, size_t size)
{
    return begin(Variant(type, data, size));
}

// --- begin() workhorse

Annotation::Scope Annotation::begin(const Variant& data)
{
    if (m_attr == Attribute::invalid)
        create_attribute(data.type());

    if (!(m_attr.type() == data.type()) || m_attr.type() == CALI_TYPE_INV)
        return Scope(Attribute::invalid);

    Caliper::instance()->begin(m_attr, data);

    return Scope(m_attr);
}

// --- set() overloads

Annotation::Scope Annotation::set(int data)
{
    // special case: allow assignment of int values to 'double' or 'uint' attributes
    if (m_attr.type() == CALI_TYPE_DOUBLE)
        return set(Variant(static_cast<double>(data)));
    if (m_attr.type() == CALI_TYPE_UINT)
        return set(Variant(static_cast<uint64_t>(data)));

    return set(Variant(data));
}

Annotation::Scope Annotation::set(double data)
{
    return set(Variant(data));
}

Annotation::Scope Annotation::set(const string& data)
{
    return set(Variant(CALI_TYPE_STRING, data.data(), data.size()));
}

Annotation::Scope Annotation::set(const char* data)
{
    return set(Variant(CALI_TYPE_STRING, data, strlen(data)));
}

Annotation::Scope Annotation::set(cali_attr_type type, const void* data, size_t size)
{
    return set(Variant(type, data, size));
}

// --- set() workhorse

Annotation::Scope Annotation::set(const Variant& data)
{
    if (m_attr == Attribute::invalid)
        create_attribute(data.type());

    if (!(m_attr.type() == data.type()) || m_attr.type() == CALI_TYPE_INV)
        return Scope(Attribute::invalid);

    Caliper::instance()->set(m_attr, data);

    return Scope(m_attr);
}

// --- end()

void Annotation::end()
{
    Caliper::instance()->end(m_attr);
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
