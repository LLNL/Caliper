/// @file Annotation.h
/// Caliper C++ annotation interface

#include <Attribute.h>

#include <cali_types.h>

#include <string>
#include <utility>

namespace cali
{

class Variant;

class Annotation 
{
    Attribute   m_attr;
    std::string m_name;
    int         m_opt;

    void create_attribute(cali_attr_type type);

public:

    enum Option { Default = 0, StoreAsValue = 1, NoMerge = 2, KeepAlive = 128 };

    class Scope {
        Attribute m_attr;
        bool      m_destruct;

        Scope(const Attribute& a) 
            : m_attr { a }, m_destruct { false } { }

    public:

        Scope(Scope&& s) 
            : m_attr { s.m_attr }, m_destruct { true } 
            { 
                s.m_attr     = Attribute::invalid;
                s.m_destruct = false;
            }

        Scope(const Scope& s) = delete;

        Scope& operator = (const Scope&) = delete;

        Scope& operator = (Scope&& s) {
            m_attr       = s.m_attr;
            m_destruct   = true;

            s.m_attr     = Attribute::invalid;
            s.m_destruct = false;

            return *this;
        }

        operator bool() const {
            return !(m_attr == Attribute::invalid);
        }

        ~Scope();

        friend class Annotation;
    };

    Annotation(const std::string& name, int opt = Default);

    Annotation(const Annotation&) = default;

    Annotation& operator = (const Annotation&) = default;

    Scope begin(int data);
    Scope begin(double data);
    Scope begin(const std::string& data);
    Scope begin(const char* data);
    Scope begin(cali_attr_type type, const void* data, std::size_t size);
    Scope begin(const Variant& data);

    Scope set(int data);
    Scope set(double data);
    Scope set(const std::string& data);
    Scope set(const char* data);
    Scope set(cali_attr_type type, const void* data, std::size_t size);
    Scope set(const Variant& data);

    void  end();
};

};
