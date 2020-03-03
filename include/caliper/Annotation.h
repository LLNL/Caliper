// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

/// \file Annotation.h
/// %Caliper C++ annotation interface

#ifndef CALI_ANNOTATION_H
#define CALI_ANNOTATION_H

#include "caliper/common/Variant.h"

#include <map>

#if __cplusplus >= 201103L
#define CALI_FORWARDING_ENABLED
#endif

namespace cali
{

/// \addtogroup AnnotationAPI
/// \{

/// \brief Pre-defined function annotation class
class Function
{
private:

    // Do not copy Function objects: will double-end things
    Function(const Function&);
    Function& operator = (const Function&);

public:

    Function(const char* name);
    ~Function();
};

/// \brief Pre-defined region annotation class.
///   Region begins and ends with object construction and destruction.
class ScopeAnnotation
{
public:

    explicit ScopeAnnotation(const char* name);

    ScopeAnnotation(const ScopeAnnotation&) = delete;
    ScopeAnnotation& operator = (const ScopeAnnotation&) = delete;

    ~ScopeAnnotation();
};

/// \brief Pre-defined loop annotation class, with optional iteration attribute
class Loop
{
    struct Impl;
    Impl* pI;

public:

    class Iteration {
        const Impl* pI;

    public:

        Iteration(const Impl*, int);
        ~Iteration();
    };

    Loop(const char* name);
    Loop(const Loop& loop);

    ~Loop();

    Iteration iteration(int i) const;

    void end();
};


/// \brief Instrumentation interface to add and manipulate context attributes
///
/// The Annotation class is the primary source-code instrumentation interface
/// for %Caliper. %Annotation objects provide access to named %Caliper context
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
/// \note Access to the underlying named context attribute through
/// %Annotation objects is not exclusive: multiple %Annotation objects
/// can reference and update the same context attribute.

class Annotation
{
    struct Impl;
    Impl*  pI;


public:

    /// \brief Creates an annotation object to manipulate
    ///   the context attribute with the given \a name.
    ///
    /// \param name The attribute name
    /// \param opt  %Attribute flags. Bitwise OR combination
    ///   of \ref cali_attr_properties values.
    Annotation(const char* name, int opt = 0);

    typedef std::map<const char*, Variant> MetadataListType ;
    /// \brief Creates an annotation object to manipulate
    ///   the context attribute with the given \a name.
    ///
    /// \param name The attribute name
    /// \param opt  %Attribute flags. Bitwise OR combination
    ///   of \ref cali_attr_properties values.
    /// \param metadata: a map of
    Annotation(const char* name, const MetadataListType& metadata, int opt=0);

    Annotation(const Annotation&);

    ~Annotation();

    Annotation& operator = (const Annotation&);


    /// \brief Scope guard to automatically close an annotation at the end of
    ///   the C++ scope.
    ///
    /// Example:
    /// \code
    ///   int var = 42;
    ///   while (condition) {
    ///     cali::Annotation::Guard
    ///       g( cali::Annotation("myvar").set(var) );
    ///     // Sets "myvar=<var>" and automatically closes it at the end of the loop
    ///   }
    /// \endcode

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

    /// \name begin() overloads
    /// \{

    Annotation& begin();

    /// \brief Begin <em>name</em>=<em>data</em> region for the associated
    ///    context attribute.
    ///
    /// Marks begin of the <em>name</em>=<em>data</em> region, where
    /// \a name is the attribute name given in
    /// cali::Annotation::Annotation(). The new value will be nested
    /// under already open regions for the \a name context attribute.
    Annotation& begin(int data);
    /// \copydoc cali::Annotation::begin(int)
    Annotation& begin(double data);
    /// \copydoc cali::Annotation::begin(int)
    Annotation& begin(const char* data);
    Annotation& begin(cali_attr_type type, void* data, uint64_t size);
    /// \copydoc cali::Annotation::begin(int)
    Annotation& begin(const Variant& data);

#ifdef CALI_FORWARDING_ENABLED
    template<typename Arg, typename... Args>
    struct head{
        using type = Arg;
    };

    template<typename... Args>
    auto begin(Args&&... args) -> typename std::enable_if<!std::is_same<typename head<Args...>::type,cali::Variant>::value,Annotation&>::type {
        return begin(Variant(std::forward<Args>(args)...));
    }
    template<typename... Args>
    auto set(Args&&... args) -> typename std::enable_if<!std::is_same<typename head<Args...>::type,cali::Variant>::value,Annotation&>::type {
        return set(Variant(std::forward<Args>(args)...));
    }
#else
    template<typename Arg>
    Annotation& begin(const Arg& arg){
        return begin(Variant(arg));
    }
    template<typename Arg>
    Annotation& set(const Arg& arg){
        return set(Variant(arg));
    }
    template<typename Arg>
    Annotation& begin(Arg& arg){
        return begin(Variant(arg));
    }
    template<typename Arg>
    Annotation& set(Arg& arg){
        return set(Variant(arg));
    }
#endif

    /// \}
    /// \name set() overloads
    /// \{

    /// \brief Set <em>name</em>=<em>data</em> for the associated
    ///    context attribute.
    ///
    /// Exports <em>name</em>=<em>data</em>, where \a name is the
    /// attribute name given in cali::Annotation::Annotation(). The
    /// top-most prior open value for the \a name context attribute,
    /// if any, will be overwritten.
    Annotation& set(int data);
    /// \copydoc cali::Annotation::set(int)
    Annotation& set(double data);
    /// \copydoc cali::Annotation::set(int)
    Annotation& set(const char* data);
    Annotation& set(cali_attr_type type, void* data, uint64_t size);
    /// \copydoc cali::Annotation::set(int)
    Annotation& set(const Variant& data);

    /// \}

    /// \brief Close top-most open region for the associated
    ///   context attribute.
    void end();
};

/// \} // AnnotationAPI group

} // namespace cali

#undef CALI_FORWARDING_ENABLED


#endif
