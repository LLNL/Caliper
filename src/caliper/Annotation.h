/// @file Annotation.h
/// Caliper C++ annotation interface

//
// --- NOTE: This interface must be C++98 only!
//

#ifndef CALI_ANNOTATION_H
#define CALI_ANNOTATION_H

#include <Attribute.h>

#include <cali_types.h>

#include <string>

namespace cali
{

class Variant;

class Annotation 
{
    Attribute   m_attr;
    std::string m_name;
    int         m_opt;

    void create_attribute(cali_attr_type type);

    struct ScopeObj {
        cali::Attribute m_attr;
        bool            m_destruct;

        ScopeObj(const Attribute& attr, bool destruct = true)
            : m_attr(attr), m_destruct(destruct) { }
    };

public:

    class AutoScope {
        ScopeObj m_scope_info;

        AutoScope(const AutoScope& s);
        AutoScope& operator = (const AutoScope&);

    public:

        AutoScope(const ScopeObj& s) 
            : m_scope_info(s)
            { }

        ~AutoScope();
    };

    Annotation(const std::string& name, int opt = 0);

    ScopeObj begin(int data);
    ScopeObj begin(double data);
    ScopeObj begin(const std::string& data);
    ScopeObj begin(const char* data);
    ScopeObj begin(cali_attr_type type, const void* data, uint64_t size);
    ScopeObj begin(const Variant& data);

    ScopeObj set(int data);
    ScopeObj set(double data);
    ScopeObj set(const std::string& data);
    ScopeObj set(const char* data);
    ScopeObj set(cali_attr_type type, const void* data, uint64_t size);
    ScopeObj set(const Variant& data);

    void     end();
};

};

#endif
