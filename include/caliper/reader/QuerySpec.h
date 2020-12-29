// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

/// \file QuerySpec.h
/// \brief %Caliper query specification definition

#pragma once

#include <map>
#include <string>
#include <vector>

namespace cali
{

/// \brief Describes a %Caliper data processing rule (filter, aggregation, formatting)
/// \ingroup ReaderAPI
struct QuerySpec
{
    //
    // --- Types
    //

    /// \brief Template type for list-style query options
    template<class T>
    struct SelectionList {
        enum SelectionOpt {
            Default, ///< Take the default
            None,    ///< Take none
            All,     ///< Take all available
            List     ///< User-defined list
        }              selection; ///< Selection specification

        /// \brief User-defined list
        std::vector<T> list;
    };

    /// \brief Describe function signatures in query specs
    struct FunctionSignature {
        int          id;
        const char*  name;     ///< Function name
        int          min_args; ///< Minimum required number of arguments
        int          max_args; ///< Maximum allowed number of function arguments
        const char** args;     ///< Names of the function arguments
    };

    static const FunctionSignature FunctionSignatureTerminator;

    /// \brief An aggregation function invocation in a query spec
    struct AggregationOp {
        FunctionSignature        op;    ///< The aggregation operator
        std::vector<std::string> args;  ///< Arguments for the aggregation operator (typically, the attribute name)

        AggregationOp()
            : op(FunctionSignatureTerminator)
        { }

        AggregationOp(const FunctionSignature& s)
            : op(s)
        { }

        AggregationOp(const FunctionSignature& s, const std::vector<std::string>& a)
            : op(s), args(a)
        { }
    };

    /// \brief Sort description.
    struct SortSpec {
        /// \brief The sort order.
        enum Order {
            None, Ascending, Descending
        }           order;
        /// \brief Name of the attribute to be sorted.
        std::string attribute;

        SortSpec(const std::string& s, Order o = Ascending)
            : order(o), attribute(s)
        { }
    };

    /// \brief Filter condition
    struct Condition {
        enum Op   {
            None,
            Exist,       NotExist,
            Equal,       NotEqual,
            LessThan,    GreaterThan,
            LessOrEqual, GreaterOrEqual
        }           op;
        std::string attr_name;
        std::string value;

        Condition()
            : op(Op::None)
        { }

        Condition(Condition::Op o, const std::string& name, const std::string& val)
            : op(o), attr_name(name), value(val)
        { }
    };

    /// \brief Output formatter specification.
    struct FormatSpec {
        enum Opt {
            Default, User
        }                        opt;       ///< Default or user-defined formatter
        FunctionSignature        formatter; ///< The formatter to use. Signatures provided by FormatProcessor.
        std::vector<std::string> args;      ///< Arguments to the formatter.
    };

    struct PreprocessSpec {
        std::string   target;
        AggregationOp op;
        Condition     cond;
    };

    //
    // --- Data members
    //

    typedef SelectionList<AggregationOp> AggregationSelection;
    typedef SelectionList<std::string>   AttributeSelection;
    typedef SelectionList<Condition>     FilterSelection;
    typedef SelectionList<SortSpec>      SortSelection;

    /// \brief List of aggregations to be performed.
    AggregationSelection         aggregation_ops;
    /// \brief List of attribute names that form the aggregation key (i.e., GROUP BY spec).
    AttributeSelection           aggregation_key;

    /// \brief List of attributes to print in output
    AttributeSelection           attribute_selection;

    /// \brief List of filter clauses (filters will be combined with AND)
    FilterSelection              filter;

    /// \brief List of sort specifications
    SortSelection                sort;

    /// \brief Output formatter specification
    FormatSpec                   format;

    /// \brief Output aliases for attributes (i.e., "select x AS y" )
    std::map<std::string, std::string> aliases;

    /// \brief Units for attributes (i.e. SELECT x AS y UNIT z)
    std::map<std::string, std::string> units;

    /// \brief List of preprocessing operations (i.e., "LET y=f(x)")
    std::vector<PreprocessSpec>  preprocess_ops;
};

} // namespace cali
