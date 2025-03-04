// wrapAnnotation.h
// This file is generated by Shroud 0.13.0. Do not edit.
/**
 * \file wrapAnnotation.h
 * \brief Shroud generated wrapper for Annotation class
 */
// For C users and C++ implementation

#ifndef WRAPANNOTATION_H
#define WRAPANNOTATION_H

#include "typesCaliper.h"

// splicer begin class.Annotation.CXX_declarations
// splicer end class.Annotation.CXX_declarations

#ifdef __cplusplus
extern "C"
{
#endif

// splicer begin class.Annotation.C_declarations
// splicer end class.Annotation.C_declarations

cali_Annotation* cali_Annotation_new(const char* key, cali_Annotation* SHC_rv);

cali_Annotation* cali_Annotation_new_with_properties(const char* key, int properties, cali_Annotation* SHC_rv);

void cali_Annotation_delete(cali_Annotation* self);

void cali_Annotation_begin_int(cali_Annotation* self, int val);

void cali_Annotation_begin_string(cali_Annotation* self, const char* val);

void cali_Annotation_set_int(cali_Annotation* self, int val);

void cali_Annotation_set_string(cali_Annotation* self, const char* val);

void cali_Annotation_end(cali_Annotation* self);

#ifdef __cplusplus
}
#endif

#endif // WRAPANNOTATION_H
