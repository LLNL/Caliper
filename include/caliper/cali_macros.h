/* Copyright (c) 2019, Lawrence Livermore National Security, LLC.
 * See top-level LICENSE file for details.
 */

#pragma once

/**
 * \file cali_macros.h
 * \brief Convenience macros for Caliper annotations
 *
 * \addtogroup AnnotationAPI
 * \{
 */

#ifdef __cplusplus

#include "Annotation.h"

/// \brief C++ macro to mark a function
///
/// Mark begin and end of a function. Should be placed at the top of the
/// function, and will automatically "close" the function at any return
/// point. Will export the annotated function by name in the pre-defined
/// `function` attribute. Only available in C++.
#define CALI_CXX_MARK_FUNCTION \
    cali::Function __cali_ann##__func__(__func__)

/// \brief C++ macro marking a scoped region
///
/// Mark begin and end of a C++ scope. Should be placed at the top of the
/// scope, and will automatically "close" the function at any return
/// point. Will export the annotated function by name in the pre-defined
/// `annotation` attribute. Only available in C++.
#define CALI_CXX_MARK_SCOPE(name) \
    cali::ScopeAnnotation __cali_ann_scope##__LINE__(name)

/// \brief Mark loop in C++
/// \copydetails CALI_MARK_LOOP_BEGIN
#define CALI_CXX_MARK_LOOP_BEGIN(loop_id, name) \
    cali::Loop __cali_loop_##loop_id(name)

/// \brief Mark loop end in C++
/// \copydetails CALI_MARK_LOOP_END
#define CALI_CXX_MARK_LOOP_END(loop_id) \
    __cali_loop_##loop_id.end()

/// \brief C++ macro for a loop iteration
///
/// Create a C++ annotation for a loop iteration. The loop must be marked
/// with \ref CALI_CXX_MARK_LOOP_BEGIN and \ref CALI_CXX_MARK_LOOP_END.
/// This will export the loop's iteration count given in \a iter in an
/// attribute named \c iteration\#name, where \a name is the loop name
/// given in \ref CALI_CXX_MARK_LOOP_BEGIN.
/// The macro should be placed at the beginning of the loop block. The
/// annotation will be closed automatically. Example:
///
/// \code
///   CALI_CXX_MARK_LOOP_BEGIN(mainloop_id, "mainloop");
///   for (int i = 0; i < ITER; ++i) {
///     CALI_CXX_MARK_LOOP_ITERATION(mainloop_id, i);
///     // ...
///   }
///   CALI_CXX_MARK_LOOP_END(mainloop_id);
/// \endcode
///
/// \param loop_id The loop identifier given to \ref CALI_CXX_MARK_LOOP_BEGIN
/// \param iter    The iteration number. Must be convertible to \c int.
#define CALI_CXX_MARK_LOOP_ITERATION(loop_id, iter) \
    cali::Loop::Iteration __cali_iter_##loop_id ( __cali_loop_##loop_id.iteration(static_cast<int>(iter)) )

#endif // __cplusplus

extern cali_id_t cali_function_attr_id;
extern cali_id_t cali_loop_attr_id;
extern cali_id_t cali_statement_attr_id;
extern cali_id_t cali_annotation_attr_id;

/// \brief Mark begin of a function.
///
/// Exports the annotated function's name in the pre=defined
/// `function` attribute. A \ref CALI_MARK_FUNCTION_END marker must be
/// placed at \e all function exit points. For C++, we recommend using
/// \ref CALI_CXX_MARK_FUNCTION instead.
/// \sa CALI_MARK_FUNCTION_END, CALI_CXX_MARK_FUNCTION
#define CALI_MARK_FUNCTION_BEGIN \
    if (cali_function_attr_id == CALI_INV_ID) \
        cali_init(); \
    cali_begin_string(cali_function_attr_id, __func__)

/// \brief Mark end of a function.
///
/// Must be placed at \e all exit points of a function marked with
/// \ref CALI_MARK_FUNCTION_BEGIN.
/// \sa CALI_MARK_FUNCTION_BEGIN
#define CALI_MARK_FUNCTION_END \
    cali_safe_end_string(cali_function_attr_id, __func__)

/// \brief Mark a loop
///
/// Mark begin of a loop. Will export the user-provided loop name
/// in the pre-defined `loop` attribute.
/// This macro should be placed before the loop of interest, and
/// \ref CALI_MARK_LOOP_END should be placed after the loop.
///
/// \param loop_id A loop identifier. Needed to refer to the loop
///   from the \a iteration and \a end annotations.
/// \param name    Name of the loop.
#define CALI_MARK_LOOP_BEGIN(loop_id, name) \
    if (cali_loop_attr_id == CALI_INV_ID) \
        cali_init(); \
    cali_begin_string(cali_loop_attr_id, (name));       \
    cali_id_t __cali_iter_##loop_id = \
        cali_make_loop_iteration_attribute(name);

/// \brief Mark a loop
///
/// Mark end of a loop. Will export the user-provided loop name
/// in the pre-defined `loop` attribute.
///
/// This macro should be placed after the loop of interest.
/// Users must ensure proper begin/end matching: If the surrounding
/// function can be exited from within the loop (e.g., from a \c return
/// statement within the loop), an exit marker must be placed there
/// as well.
///
/// \param loop_id The loop identifier given to \ref CALI_MARK_LOOP_BEGIN.
#define CALI_MARK_LOOP_END(loop_id) \
    cali_end(cali_loop_attr_id)

/// \brief Mark begin of a loop iteration.
///
/// This annotation should be placed at the top inside of the loop body.
/// The loop must be annotated with \ref CALI_MARK_LOOP_BEGIN.
/// The iteration number will be exported in the attribute `iteration#name`,
/// where \a name is the loop name given to \ref CALI_MARK_LOOP_BEGIN.
/// In C++, we recommend using \ref CALI_CXX_MARK_LOOP_ITERATION.
///
/// \param loop_id: Loop identifier, must match the identifier given to
///   \ref CALI_MARK_LOOP_BEGIN.
/// \param iter Current iteration number. This macro argument must be
///   convertible to \c int.
/// \sa CALI_MARK_ITERATION_END, CALI_MARK_LOOP_BEGIN,
///    CALI_CXX_MARK_LOOP_ITERATION
#define CALI_MARK_ITERATION_BEGIN(loop_id, iter) \
    cali_begin_int( __cali_iter_##loop_id, ((int) (iter)))

/// \brief Mark end of a loop iteration.
///
/// This annotation should be placed at the end inside of the loop body.
/// If an iteration can be left prematurely (e.g., from a \c continue,
/// \c break, or \c return statement), an end marker must be placed there
/// as well.
///
/// \param loop_id Loop identifier given in \ref CALI_MARK_LOOP_BEGIN.
/// \sa CALI_MARK_ITERATION_BEGIN
#define CALI_MARK_ITERATION_END(loop_id) \
    cali_end( __cali_iter_##loop_id )

/// \brief Wrap Caliper annotations around a C/C++ statement.
///
/// The wrapped statement will be annotated with the given \a name
/// in the `statement` attribute. Example
///
/// \code
///   double res;
///   /* Wrap the sqrt() call */
///   CALI_WRAP_STATEMENT( "sqrt", res = sqrt(49) );
/// \endcode
///
/// \param name The user-defined region name. Must be convertible into
///   a `const char*`.
/// \param statement C/C++ statement(s) that should be wrapped. The
///   statements must complete within the wrapped region, that is, they
///   cannot branch out of the macro (e.g.  with \c goto, \c continue,
///   \c break, or \c return).
///
#define CALI_WRAP_STATEMENT(name, statement)     \
    if (cali_statement_attr_id == CALI_INV_ID) \
        cali_init(); \
    cali_begin_string(cali_statement_attr_id, (name));  \
    statement; \
    cali_end(cali_statement_attr_id);

/// \brief Mark begin of a user-defined code region.
///
/// This annotation should be placed before a code region of interest.
/// The user-provided region name will be exported in the pre-defined
/// `annotation` attribute.
///
/// Users must ensure proper nesting: Each CALI_MARK_BEGIN must be
/// matched by a corresponding \ref CALI_MARK_END in the correct order.
/// Regions may  be nested within another, but they cannot overlap
/// partially.
///
/// \param name The region name. Must be convertible to `const char*`.
/// \sa CALI_MARK_END
#define CALI_MARK_BEGIN(name) \
    cali_begin_region(name)

/// \brief Mark end of a user-defined code region.
///
/// This annotation should be placed after a code region of interest
/// that has been annotated with \ref CALI_MARK_BEGIN.
///
/// \param name The region name given to \ref CALI_MARK_BEGIN.
///   The macro will check if the name matches, and report an error
///   if it doesn't.
/// \sa CALI_MARK_BEGIN
#define CALI_MARK_END(name) \
    cali_end_region(name)

/**
 * \} (group)
 */
