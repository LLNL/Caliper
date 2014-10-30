/// @file Annotation.h
/// Caliper C++ annotation interface

#include <Attribute.h>

#include <cali_types.h>

#include <string>
#include <utility>

namespace cali
{

class Annotation 
{
    Attribute   m_attr;
    std::string m_name;
    int         m_opt;
    int         m_depth;

    void create_attribute(cali_attr_type type);

public:

    enum Option { Default = 0, StoreAsValue = 1, NoMerge = 2, KeepAlive = 128 };

    Annotation(const std::string& name, int opt = Default);

    Annotation(const Annotation&) = delete;

    Annotation(Annotation&& a) 
        : m_attr { a.m_attr }, m_name { a.m_name }, m_opt { a.m_opt }, m_depth { a.m_depth }
        {
            a.m_attr  = Attribute::invalid;
            a.m_name.clear();
            a.m_opt   = Default;
            a.m_depth = 0;
        }

    ~Annotation();

    Annotation& operator = (const Annotation&) = delete;

    Annotation& operator = (Annotation&& a) { 
        m_attr    = a.m_attr;
        m_name    = std::move(a.m_name);
        m_opt     = a.m_opt;
        m_depth   = a.m_depth;
        a.m_attr  = Attribute::invalid;
        a.m_opt   = Default;
        a.m_depth = 0;

        return *this;
    }

    cali_err begin(int data);
    cali_err begin(double data);
    cali_err begin(const std::string& data);

    cali_err begin(cali_attr_type type, const void* data, std::size_t size);

    cali_err set(int data);
    cali_err set(double data);
    cali_err set(const std::string& data);

    cali_err set(cali_attr_type type, const void* data, std::size_t size);

    static std::pair<Annotation, cali_err> set(const std::string& name, int data, int opt = Default);
    static std::pair<Annotation, cali_err> set(const std::string& name, double data, int opt = Default);
    static std::pair<Annotation, cali_err> set(const std::string& name, const std::string& data, int opt = Default);

    cali_err end();
};

};
