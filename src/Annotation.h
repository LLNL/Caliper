/// @file Annotation.h
/// Caliper C++ annotation interface

#include "Caliper.h"

#include <string>

namespace cali
{

template<typename T>
class Annotation {
    Attribute m_attr;
    int       m_depth;

public:

    void begin(const T& data) {
        Caliper* c = Caliper::instance();

        if (c->begin(c->current_environment(), m_attr, &data, sizeof(T)) == CTX_SUCCESS)
            ++m_depth;
    }

    void end() {
        Caliper* c = Caliper::instance();

        c->end(c->current_environment(), m_attr);

        --m_depth;
    }

    Annotation(const std::string& name) 
        : m_attr(Attribute::invalid), m_depth(0)
        {
            m_attr = Caliper::instance()->get_attribute(name);
        }

    Annotation(const std::string& name, const T& data)
        : m_attr(Attribute::invalid), m_depth(0)
        {
            m_attr = Caliper::instance()->get_attribute(name);
            begin(data);
        }

    ~Annotation() {
        while (m_depth-- > 0)
            end();
    }
};

template<>
class Annotation<std::string>;

template<typename T>
Annotation<T> annotate(const std::string& name, const T& data)
{
    return Annotation<T>(name, data);
}

};
