#pragma once

namespace cali
{

class Attribute;
class Variant;

class Filter {
public:

    virtual ~Filter()
        { }
    
    virtual bool filter(const cali::Attribute& attr, const cali::Variant& value) = 0;
};

}

