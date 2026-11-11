#pragma once

#include "encoder.h"

#include <string>
#include <utility>
#include <vector>

namespace ECProject {
namespace RepairBenchmark {

struct RepairCase
{
    std::string failed_name;
    int failed_block_id;
    std::vector<int> source_block_ids;
};

struct RepairTask
{
    std::vector<int> failed_block_ids;
    std::vector<int> source_block_ids;
};

struct RepairGroupCase
{
    std::string failed_name;
    std::vector<RepairTask> tasks;
    bool optimal_g0_d0_chain = false;
};

using DoubleRepairCase = RepairGroupCase;
using TripleRepairCase = RepairGroupCase;
using QuadrupleRepairCase = RepairGroupCase;

inline bool is_supported_failure_type(const std::string &failure_type)
{
    return failure_type == "single" || failure_type == "double" ||
           failure_type == "triple" || failure_type == "quadruple";
}

inline std::vector<int> inclusive_range(int first, int last)
{
    std::vector<int> ids;
    for (int id = first; id <= last; ++id)
        ids.push_back(id);
    return ids;
}

inline std::vector<RepairCase> make_azure_uneven_single_block_cases(int k, int r, int z)
{
    std::vector<RepairCase> cases;
    const auto bounds = get_azure_lrc_local_group_bounds(k, z);
    for (int local = 0; local < z; ++local)
    {
        if (k == 64 && local == 1)
            continue;
        cases.push_back({"L" + std::to_string(local), k + r + local,
                         inclusive_range(bounds[local].first, bounds[local].second - 1)});
    }
    cases.push_back({"G0", k, inclusive_range(0, k - 1)});
    return cases;
}

inline std::vector<DoubleRepairCase> make_azure_double_block_cases(int k)
{
    if (k == 64)
    {
        return {
            {"L0+L1", {{{67}, inclusive_range(0, 20)},
                        {{68}, inclusive_range(21, 41)}}},
            {"L0+L2", {{{67}, inclusive_range(0, 20)},
                        {{69}, inclusive_range(42, 63)}}},
            {"G0+G1", {{{64, 65}, inclusive_range(0, 63)}}},
        };
    }

    return {{"G0+G1", {{{k, k + 1}, inclusive_range(0, k - 1)}}}};
}

inline std::vector<TripleRepairCase> make_azure_k64_triple_block_cases()
{
    return {{"G0+G1+G2", {{{64, 65, 66}, inclusive_range(0, 63)}}}};
}

inline std::vector<QuadrupleRepairCase> make_azure_k64_quadruple_block_cases()
{
    return {{"G0+G1+G2+L0", {{{64, 65, 66, 67}, inclusive_range(0, 63)}}}};
}

} // namespace RepairBenchmark
} // namespace ECProject
