#ifndef CALI_TOPDOWN_TOPDOWN_CALCULATOR_H
#define CALI_TOPDOWN_TOPDOWN_CALCULATOR_H

#include "caliper/Caliper.h"

#include <map>
#include <vector>

// clang-format off
/* How to create a new topdown calculation plugin:
 * 
 * Step 1: Create a subclass of this class implementing the calculations for the new 
 *         architecture (see Haswell and SPR as examples)
 * Step 2: Edit IntelTopdown::intel_topdown_register in IntelTopdown.cpp with logic for
 *         creating an instance of your subclass (edits should be made around line 165)
 * Step 3: Edit CMakeLists.txt to include the source file for your new subclass
 * Step 4: Edit the 'get_builtin_option_specs' function in src/caliper/controllers/controllers.cpp
 *         to add the appropriate option spec for your architecture in the topdown service
 */
// clang-format on

namespace cali
{
namespace topdown
{

enum IntelTopdownLevel { All = 1, Top = 2 };

class TopdownCalculator
{
protected:

    IntelTopdownLevel m_level;

    const char* m_top_counters;
    const char* m_all_counters;

    std::vector<const char*> m_res_top;
    std::vector<const char*> m_res_all;

    std::map<std::string, Attribute> m_counter_attrs;
    std::map<std::string, Attribute> m_result_attrs;

    std::map<std::string, int> m_counters_not_found;

    Variant get_val_from_rec(const std::vector<Entry>& rec, const char* name);

    TopdownCalculator(
        IntelTopdownLevel          level,
        const char*                top_counters,
        const char*                all_counters,
        std::vector<const char*>&& res_top,
        std::vector<const char*>&& res_all
    );

public:

    TopdownCalculator(IntelTopdownLevel level);

    virtual ~TopdownCalculator() = default;

    // Returns true if PAPI multiplexing cannot be used for the
    // counters and/or architecture needed for the subclass
    virtual bool check_for_disabled_multiplex() const = 0;

    // Computes the L1 topdown metrics using the counters contained
    // in the Caliper Entries.
    virtual std::vector<Entry> compute_toplevel(const std::vector<Entry>& rec) = 0;

    // Returns the expected size of the vectoor returned from
    // compute_toplevel
    virtual std::size_t get_num_expected_toplevel() const = 0;

    // Computes the topdown metrics beneath "Retiring" in the
    // topdown hierarchy for the given architecture
    virtual std::vector<Entry> compute_retiring(const std::vector<Entry>& rec) = 0;

    // Returns the expected size of the vector returned from
    // compute_retiring
    virtual std::size_t get_num_expected_retiring() const = 0;

    // Computes the topdown metrics beneath "Backend bound" in the
    // topdown hierarchy for the given architecture
    virtual std::vector<Entry> compute_backend_bound(const std::vector<Entry>& rec) = 0;

    // Returns the expected size of the vector returned from
    // compute_backend_bounnd
    virtual std::size_t get_num_expected_backend_bound() const = 0;

    // Computes the topdown metrics beneath "Frontend bound" in the
    // topdown hierarchy for the given architecture
    virtual std::vector<Entry> compute_frontend_bound(const std::vector<Entry>& rec) = 0;

    // Returns the expected size of the vector returned from
    // compute_frontend_bounnd
    virtual std::size_t get_num_expected_frontend_bound() const = 0;

    // Computes the topdown metrics beneath "Bad speculation" in the
    // topdown hierarchy for the given architecture
    virtual std::vector<Entry> compute_bad_speculation(const std::vector<Entry>& rec) = 0;

    // Returns the expected size of the vector returned from
    // compute_bad_speculation
    virtual std::size_t get_num_expected_bad_speculation() const = 0;

    bool find_counter_attrs(CaliperMetadataAccessInterface& db);

    void make_result_attrs(CaliperMetadataAccessInterface& db);

    const std::map<std::string, int>& get_counters_not_found() const;

    const char* get_counters() const;

    IntelTopdownLevel get_level() const;
};

} // namespace topdown
} // namespace cali

#endif /* CALI_TOPDOWN_TOPDOWN_CALCULATOR_H */