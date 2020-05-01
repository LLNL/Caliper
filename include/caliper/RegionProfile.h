// Copyright (c) 2019, Lawrence Livermore National Security, LLC.  
// See top-level LICENSE file for details.

/// \file RuntimeProfile.h
/// RuntimeProfile class

#pragma once

#include "ChannelController.h"

#include <map>
#include <memory>
#include <tuple>

namespace cali
{

/// \class RegionProfile
/// \ingroup ControlChannelAPI
/// \brief Collect and return time profiles for annotated %Caliper regions
///   in a C++ map
///
/// The RegionProfile class is a %Caliper controller that allows one to 
/// collect and examine time spent in %Caliper-annotated code regions within
/// an instrumented program.
/// It can compute inclusive or exclusive profiles. Start/stop profiling with
/// the start() and stop() methods. Once started, time profiles can 
/// be retrieved at any time with exclusive_region_times() or 
/// inclusive_region_times().
///
/// \example cali-regionprofile.cpp
/// The example shows how to use the RegionProfile controller to examine 
/// inclusive and exclusive time profiles for annotated regions within a 
/// program.
class RegionProfile : public ChannelController
{
    struct RegionProfileImpl;
    std::shared_ptr<RegionProfileImpl> mP;
    
public:

    /// \brief Create a RegionProfile controller object. 
    ///
    /// Note that profiling must be started explicitly with the start() method.
    RegionProfile();

    virtual ~RegionProfile();

    /// \brief A tuple containing the computed time profiles.
    /// 
    /// The first member is a string -> double STL map that stores the times 
    /// per region. The map keys are the region names used in the annotations, 
    /// e.g., "work" for  \c CALI_MARK_BEGIN("work").
    /// Note that nested regions with the same name can't be distinguished.
    ///
    /// The second member stores the total time spent in the \em selected 
    /// region type, and the third member stores the total time spent
    /// profiling.
    typedef std::tuple< std::map<std::string, double>, double, double > region_profile_t;

    /// \brief Return an exclusive time profile for annotated regions.
    /// 
    /// Exclusive time is the time spent in within a begin/end region itself,
    /// excluding time spent in sub-regions that are nested within.
    ///
    /// Profiling must have been started with start().
    /// 
    /// By default, the result contains times for all region types with the 
    /// \c CALI_ATTR_NESTED flag. Specifically, this includes regions marked
    /// with the %Caliper annotations macros such as \c CALI_MARK_BEGIN.
    /// With the optional \a region_type argument, profiles can be calculated
    /// for a specific region type (%Caliper attribute) only. In this case,
    /// the second value in the result tuple contains the total time spent in
    /// regions with the selected type. 
    ///
    /// As an example, the following code returns a profile for "function" 
    /// regions:
    ///
    /// \code
    /// RegionProfile rp;
    /// rp.start();
    ///
    /// // ...
    ///
    /// std::map<std::string, double> function_times;
    /// double total_function_time;
    /// double total_profiling_time;
    ///
    /// std::tie(function_times, total_function_time, total_profiling_time) = 
    ///     rp.exclusive_region_times("function");
    /// \endcode
    ///
    /// \returns A region_profile_t tuple with exclusive time per region in 
    ///    a region name -> time STL map, the total time for the selected 
    ///    region time, and the total time spent profiling. 
    ///    All time values are in seconds.
    ///
    /// \sa region_profile_t
    region_profile_t
    exclusive_region_times(const std::string& region_type = "");

    /// \brief Return an inclusive time profile for annotated regions.
    /// 
    /// Inclusive time is time spent within a begin/end region, 
    /// including time spent in sub-regions that are nested within.
    ///
    /// Other than returning inclusive rather than exclusive times, this
    /// method works the same as exclusive_region_times().
    ///
    /// \returns A region_profile_t tuple with inclusive time per region in 
    ///    a region name -> time STL map, the total time for the selected 
    ///    region time, and the total time spent profiling. 
    ///    All time values are in seconds.
    ///
    /// \sa region_profile_t, exclusive_region_times()
    region_profile_t
    inclusive_region_times(const std::string& region_type = "");

    /// \brief Reset the profiling database.
    void
    clear();
};

}
