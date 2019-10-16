// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

/** 
 * \file IdType.h 
 * \brief IdType class declaration
 */

#ifndef CALI_IDTYPE_H
#define CALI_IDTYPE_H


#include "cali_types.h"

namespace cali
{

/// \brief Base class for something with a unique numeric ID. 

class IdType {
    cali_id_t m_id;

public:

    IdType(cali_id_t id)
        : m_id(id) { }

    cali_id_t id() const { 
        return m_id;
    }

    bool operator == (const IdType& r) const {
        return m_id == r.m_id;
    }

    bool operator < (const IdType& r) const {
        return m_id < r.m_id;
    }
};

} // namespace cali

#endif // CALI_IDTYPE_H
