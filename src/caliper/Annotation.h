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

/// \brief Instrumentation interface to add and manipulate context attributes

class Annotation 
{
    struct Impl;
    Impl*  pI;

    Annotation(const Annotation&);
    Annotation& operator = (const Annotation&);

public:

    /// \brief Constructor. Creates an annotation object to manipulate 
    ///    the context attribute \c name. 

    Annotation(const std::string& name, int opt = 0);

    ~Annotation();


    /// \brief Scope guard to automatically \c end() an annotation at the end of
    ///   the C++ scope.

    class Guard {
        Impl* pI;

        Guard(const Guard&);
        Guard& operator = (const Guard&);

    public:

        Guard(Annotation& a);

        ~Guard();
    };

    // Keep AutoScope name for backward compatibility
    typedef Guard AutoScope;

    /// \name \c begin() overloads
    /// \{

    Annotation& begin(int data);
    Annotation& begin(double data);
    Annotation& begin(const std::string& data);
    Annotation& begin(const char* data);
    Annotation& begin(cali_attr_type type, void* data, uint64_t size);
    Annotation& begin(const Variant& data);

    /// \}
    /// \name \c set() overloads
    /// \{

    Annotation& set(int data);
    Annotation& set(double data);
    Annotation& set(const std::string& data);
    Annotation& set(const char* data);
    Annotation& set(cali_attr_type type, void* data, uint64_t size);
    Annotation& set(const Variant& data);

    /// \}
    /// \name \c end()
    /// \{

    void end();

    /// \}
};

} // namespace cali

#endif
