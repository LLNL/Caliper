/// \file Annotation.cpp
/// Annotation interface


#include "Annotation.h"
#include "Caliper.h"

#include <Log.h>
#include <Variant.h>

#include <cstring>

using namespace std;
using namespace cali;


// --- Annotation implementation object

struct Annotation::Impl {
    Caliper*    m_cI;   // We assume the Caliper instance object does not change
    Attribute   m_attr;
    std::string m_name;
    int         m_opt;
    int         m_count;
    int         m_refcount;

    Impl(const std::string& name, int opt)
        : m_cI(Caliper::instance()), 
          m_attr(Attribute::invalid), 
          m_name(name), 
          m_opt(opt),
          m_count(0),
          m_refcount(1)
        { 
            m_attr = m_cI->get_attribute(name);
        }

    void begin(const Variant& data) {
        if (m_attr == Attribute::invalid)
            create_attribute(data.type());

        if ((m_attr.type() == data.type()) && m_attr.type() != CALI_TYPE_INV)
            if (m_cI->begin(m_attr, data) == CALI_SUCCESS)
                ++m_count;
    }

    void set(const Variant& data) {
        if (m_attr == Attribute::invalid)
            create_attribute(data.type());

        if ((m_attr.type() == data.type()) && m_attr.type() != CALI_TYPE_INV)
            if (m_cI->set(m_attr, data) == CALI_SUCCESS)
                ++m_count;
    }

    void end() {
        if (m_count > 0)
            if (m_cI->end(m_attr) == CALI_SUCCESS)
                --m_count;
    }

    void create_attribute(cali_attr_type type) {
        m_attr = m_cI->create_attribute(m_name, type, m_opt);

        if (m_attr == Attribute::invalid)
            Log(0).stream() << "Could not create attribute " << m_name << endl;
    }

    Impl* attach() {
        ++m_refcount;
        return this;
    }

    void detach() {
        --m_refcount;

        if (m_refcount == 0)
            delete this;
    }
};


// --- Guard subclass

Annotation::Guard::Guard(Annotation& a)
    : pI(a.pI->attach())
{ }

Annotation::Guard::~Guard()
{
    if (pI->m_count > 0)
        pI->end();

    pI->detach();
}


// --- Constructors / destructor

Annotation::Annotation(const string& name, int opt)
    : pI(new Impl(name, opt))
{

}

Annotation::~Annotation()
{
    pI->detach();
}


// --- begin() overloads

Annotation& Annotation::begin(int data)
{
    // special case: allow assignment of int values to 'double' or 'uint' attributes
    if (pI->m_attr.type() == CALI_TYPE_DOUBLE)
        return begin(Variant(static_cast<double>(data)));
    if (pI->m_attr.type() == CALI_TYPE_UINT)
        return begin(Variant(static_cast<uint64_t>(data)));

    return begin(Variant(data));
}

Annotation& Annotation::begin(double data)
{
    return begin(Variant(data));
}

Annotation& Annotation::begin(const string& data)
{
    return begin(Variant(CALI_TYPE_STRING, data.data(), data.size()));
}

Annotation& Annotation::begin(const char* data)
{
    return begin(Variant(CALI_TYPE_STRING, data, strlen(data)));
}

Annotation& Annotation::begin(cali_attr_type type, void* data, uint64_t size)
{
    return begin(Variant(type, data, size));
}

Annotation& Annotation::begin(const Variant& data)
{
    pI->begin(data);
    return *this;
}

// --- set() overloads

Annotation& Annotation::set(int data)
{
    // special case: allow assignment of int values to 'double' or 'uint' attributes
    if (pI->m_attr.type() == CALI_TYPE_DOUBLE)
        return set(Variant(static_cast<double>(data)));
    if (pI->m_attr.type() == CALI_TYPE_UINT)
        return set(Variant(static_cast<uint64_t>(data)));

    return set(Variant(data));
}

Annotation& Annotation::set(double data)
{
    return set(Variant(data));
}

Annotation& Annotation::set(const string& data)
{
    return set(Variant(CALI_TYPE_STRING, data.data(), data.size()));
}

Annotation& Annotation::set(const char* data)
{
    return set(Variant(CALI_TYPE_STRING, data, strlen(data)));
}

Annotation& Annotation::set(cali_attr_type type, void* data, uint64_t size)
{
    return set(Variant(type, data, size));
}

Annotation& Annotation::set(const Variant& data)
{
    pI->set(data);
    return *this;
}

void Annotation::end()
{
    pI->end();
}
