/* *********************************************************************************************
 * Copyright (c) 2015, Lawrence Livermore National Security, LLC.  
 * Produced at the Lawrence Livermore National Laboratory.
 *
 * This file is part of Caliper.
 * Written by David Boehme, boehme3@llnl.gov.
 * LLNL-CODE-678900
 * All rights reserved.
 *
 * For details, see https://github.com/scalability-llnl/Caliper.
 * Please also see the LICENSE file for our additional BSD notice.
 *
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice, this list of
 *    conditions and the disclaimer below.
 *  * Redistributions in binary form must reproduce the above copyright notice, this list of
 *    conditions and the disclaimer (as noted below) in the documentation and/or other materials
 *    provided with the distribution.
 *  * Neither the name of the LLNS/LLNL nor the names of its contributors may be used to endorse
 *    or promote products derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * LAWRENCE LIVERMORE NATIONAL SECURITY, LLC, THE U.S. DEPARTMENT OF ENERGY OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * *********************************************************************************************/

#pragma once

/** 
 * \file cali_macros.h 
 * Convenience macros for Caliper annotations
 */

#ifdef __cplusplus

#include "Annotation.h"

/// \macro C++ macro to mark a function
#define CALI_CXX_MARK_FUNCTION \
    cali::Function __cali_ann##__func__(__func__)

/// \macro Mark a loop in C++ 
#define CALI_CXX_MARK_LOOP_BEGIN(loop_id, name) \
    cali::Loop __cali_loop_##loop_id(name)

/// \macro C++ macro for a loop iteration
#define CALI_CXX_MARK_LOOP_ITERATION(loop_id, iter) \
    cali::Loop::Iteration __cali_iter_##loop_id ( __cali_loop_##loop_id.iteration(static_cast<int>(iter)) )

#define CALI_CXX_MARK_LOOP_END(loop_id) \
    __cali_loop_##loop_id.end()

#endif // __cplusplus

extern cali_id_t cali_function_attr_id;
extern cali_id_t cali_loop_attr_id;
extern cali_id_t cali_statement_attr_id;

#define CALI_MARK_FUNCTION_BEGIN \
    cali_begin_string(cali_function_attr_id, __func__)

#define CALI_MARK_FUNCTION_END \
    cali_end(cali_function_attr_id)

#define CALI_MARK_LOOP_BEGIN(loop_id, name) \
    cali_begin_string(cali_loop_attr_id, name); \
    cali_id_t __cali_iter_##loop_id = \
        cali_make_loop_iteration_attribute(name);

#define CALI_MARK_LOOP_END(loop_id) \
    cali_end(cali_loop_attr_id)

#define CALI_MARK_ITERATION_BEGIN(loop_id, iter) \
    cali_begin_int( __cali_iter_##loop_id, ((int) (iter)))

#define CALI_MARK_ITERATION_END(loop_id) \
    cali_end( __cali_iter_##loop_id )

#define CALI_WRAP_STATEMENT(name, statement)     \
    cali_begin_string(cali_statement_attr_id, name); \
    statement; \
    cali_end(cali_statement_attr_id);
