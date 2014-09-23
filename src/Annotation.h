/// @file Annotation.h
/// Caliper C++ annotation interface

#include "Attribute.h"

#include "cali_types.h"

#include <string>


namespace cali
{

class Annotation {

    Attribute   m_attr;
    std::string m_name;
    int         m_opt;
    int         m_depth;

    void create_attribute(ctx_attr_type type);

public:

    enum Option { Default = 0, StoreAsValue = 1, NoMerge = 2, KeepAlive = 128 };

    Annotation(const std::string& name, Option opt = Default);

    ~Annotation();

    ctx_err begin(int data);
    ctx_err begin(double data);
    ctx_err begin(const std::string& data);

    ctx_err begin(ctx_attr_type type, const void* data, std::size_t size);

    ctx_err set(int data);
    ctx_err set(double data);
    ctx_err set(const std::string& data);

    ctx_err set(ctx_attr_type type, const void* data, std::size_t size);

    ctx_err end();
};

};
