/// \file Annotation.h
/// Caliper C++ annotation interface

//
// --- NOTE: This interface must be C++98 only!
//

#ifndef CALI_ANNOTATION_H
#define CALI_ANNOTATION_H

#include "cali_types.h"

#include <string>

namespace cali
{

class Variant;

class Annotation 
{
    struct Impl;
    Impl*  pI;

    Annotation(const Annotation&);
    Annotation& operator = (const Annotation&);

public:

    Annotation(const std::string& name, int opt = 0);

    ~Annotation();

    class Guard {
        Impl* pI;

        Guard(const Guard&);
        Guard& operator = (const Guard&);

    public:

        Guard(Annotation& a);

        ~Guard();
    };

    Annotation& begin(int data);
    Annotation& begin(double data);
    Annotation& begin(const std::string& data);
    Annotation& begin(const char* data);
    Annotation& begin(cali_attr_type type, void* data, uint64_t size);
    Annotation& begin(const Variant& data);

    Annotation& set(int data);
    Annotation& set(double data);
    Annotation& set(const std::string& data);
    Annotation& set(const char* data);
    Annotation& set(cali_attr_type type, void* data, uint64_t size);
    Annotation& set(const Variant& data);

    void end();
};

} // namespace cali

#endif
