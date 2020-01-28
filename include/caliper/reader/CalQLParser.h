// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

/// \file CalQLParser.h
/// \brief CalQLParser definition

#pragma once

#include "QuerySpec.h"

#include <iostream>
#include <memory>
#include <string>

namespace cali
{

/// \brief Create a QuerySpec specification from a given %CalQL expression.
/// \ingroup ReaderAPI

class CalQLParser
{
    struct CalQLParserImpl;
    std::unique_ptr<CalQLParserImpl> mP;
    
public:

    /// \brief Read %CalQL expression from the given input stream \a is.
    CalQLParser(std::istream& is);
    /// \brief Read %CalQL expression from \a str.
    CalQLParser(const char* str);
    
    ~CalQLParser();

    /// \brief Indicate if there was an error parsing the %CalQL expression.
    bool
    error() const;

    /// \brief Approximate position of a parser error in the given string or stream.
    std::istream::pos_type
    error_pos();

    /// \brief A descriptive error message in case of a parse error.
    std::string
    error_msg() const;

    /// \brief Returns the query specification object for the given %CalQL expression.
    QuerySpec
    spec() const;
}; 

}
