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
    int         m_refcount;

    Impl(const std::string& name, int opt)
        : m_cI(Caliper::instance()), 
          m_attr(Attribute::invalid), 
          m_name(name), 
          m_opt(opt),
          m_refcount(1)
        { 
            m_attr = m_cI->get_attribute(name);
        }

    void begin(const Variant& data) {
        if (m_attr == Attribute::invalid)
            create_attribute(data.type());

        if ((m_attr.type() == data.type()) && m_attr.type() != CALI_TYPE_INV)
            m_cI->begin(m_attr, data);
    }

    void set(const Variant& data) {
        if (m_attr == Attribute::invalid)
            create_attribute(data.type());

        if ((m_attr.type() == data.type()) && m_attr.type() != CALI_TYPE_INV)
            m_cI->set(m_attr, data);
    }

    void end() {
        m_cI->end(m_attr);
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
    pI->end();
    pI->detach();
}


/// \class Annotation
///
/// \brief Instrumentation interface to add and manipulate context attributes
///
/// The Annotation class is the primary source-code instrumentation interface
/// for Caliper. Annotation objects provide access to named Caliper context 
/// attributes. If the referenced attribute does not exist yet, it will be 
/// created automatically.
///
/// Example:
/// \code
/// cali::Annotation phase_ann("myprogram.phase");
///
/// phase_ann.begin("Initialization");
///   // ...
/// phase_ann.end();
/// \endcode
/// This example creates an annotation object for the \c myprogram.phase 
/// attribute, and uses the \c begin()/end() methods to mark a section 
/// of code where that attribute is set to "Initialization".
///
/// Note that the access to a named context attribute through Annotation 
/// objects is not exclusive: two different Annotation objects can reference and
/// update the same context attribute.
///
/// The Annotation class is \em not threadsafe; however, threads can safely
/// access the same context attribute simultaneously through different 
/// Annotation objects.

// --- Constructors / destructor

/// Construct an annotation object for context attribute \c name. 
/// 

Annotation::Annotation(const string& name, int opt)
    : pI(new Impl(name, opt))
{ }

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
