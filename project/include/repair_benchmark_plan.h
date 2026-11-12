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

inline std::vector<RepairCase> make_optimal_uneven_single_block_cases(int k, int r, int z)
{
    std::vector<RepairCase> cases;
    const auto bounds = get_optimal_lrc_local_group_bounds(k, z);
    for (int local = 0; local < z; ++local) {
        auto sources = inclusive_range(bounds[local].first, bounds[local].second - 1);
        for (int global = 0; global < r; ++global)
            sources.push_back(k + global);
        cases.push_back({"L" + std::to_string(local), k + r + local, std::move(sources)});
    }
    cases.push_back({"G0", k, inclusive_range(0, k - 1)});
    return cases;
}

inline std::vector<RepairCase> make_optimal_k64_single_block_cases()
{
    auto l0_sources = inclusive_range(0, 20);
    l0_sources.insert(l0_sources.end(), {64, 65, 66});
    auto l2_sources = inclusive_range(42, 63);
    l2_sources.insert(l2_sources.end(), {64, 65, 66});
    return {{"L0", 67, std::move(l0_sources)},
            {"L2", 69, std::move(l2_sources)}};
}

inline std::vector<RepairCase> make_nlrc_k64_single_block_cases()
{
    auto l4_sources = inclusive_range(44, 63);
    l4_sources.insert(l4_sources.end(), {64, 65, 66});
    return {{"L0", 67, inclusive_range(0, 21)},
            {"L4", 71, std::move(l4_sources)}};
}

inline std::vector<DoubleRepairCase> make_nlrc_k64_double_block_cases()
{
    auto l4_sources = inclusive_range(44, 63);
    l4_sources.insert(l4_sources.end(), {64, 65, 66});
    return {
        {"L0+L1", {{{67, 68}, inclusive_range(0, 21)}}},
        {"L0+L2", {{{67}, inclusive_range(0, 21)},
                      {{69}, inclusive_range(22, 43)}}},
        {"L0+L4", {{{67}, inclusive_range(0, 21)},
                      {{71}, std::move(l4_sources)}}},
        {"G0+G1", {{{64, 65}, inclusive_range(0, 63)}}},
    };
}

inline std::vector<TripleRepairCase> make_nlrc_k64_triple_block_cases()
{
    auto l4_sources = inclusive_range(44, 63);
    l4_sources.insert(l4_sources.end(), {64, 65, 66});
    return {
        {"L0+L1+L2", {{{67, 68}, inclusive_range(0, 21)},
                          {{69}, inclusive_range(22, 43)}}},
        {"L0+L1+L4", {{{67, 68}, inclusive_range(0, 21)},
                          {{71}, std::move(l4_sources)}}},
        {"G0+G1+G2", {{{64, 65, 66}, inclusive_range(0, 63)}}},
    };
}

inline std::vector<RepairCase> make_hlrc_k64_single_block_cases()
{
    auto l1_sources = inclusive_range(16, 31);
    auto l0_sources = inclusive_range(0, 15);
    l0_sources.push_back(64);
    auto u0_sources = inclusive_range(1, 15);
    const auto upper_sources = inclusive_range(18, 31);
    u0_sources.insert(u0_sources.end(), upper_sources.begin(), upper_sources.end());
    return {{"L1", 67, std::move(l1_sources)},
            {"L0", 66, std::move(l0_sources)},
            {"U0", 70, std::move(u0_sources)}};
}

inline std::vector<DoubleRepairCase> make_hlrc_k64_double_block_cases()
{
    auto l1_u0_sources = inclusive_range(16, 31);
    const auto u0_lower_sources = inclusive_range(1, 15);
    l1_u0_sources.insert(l1_u0_sources.end(), u0_lower_sources.begin(), u0_lower_sources.end());

    auto l0_sources = inclusive_range(0, 15);
    l0_sources.push_back(64);
    auto l2_sources = inclusive_range(32, 47);
    l2_sources.push_back(65);
    auto u0_sources = inclusive_range(1, 15);
    const auto u0_upper_sources = inclusive_range(18, 31);
    u0_sources.insert(u0_sources.end(), u0_upper_sources.begin(), u0_upper_sources.end());
    auto u1_sources = inclusive_range(33, 47);
    const auto u1_upper_sources = inclusive_range(50, 63);
    u1_sources.insert(u1_sources.end(), u1_upper_sources.begin(), u1_upper_sources.end());

    return {
        {"L1+U0", {{{70, 67}, std::move(l1_u0_sources)}}},
        {"L1+L3", {{{67}, inclusive_range(16, 31)},
                     {{69}, inclusive_range(48, 63)}}},
        {"L0+L1", {{{66}, l0_sources},
                     {{67}, inclusive_range(16, 31)}}},
        {"L0+L2", {{{66}, l0_sources},
                     {{68}, std::move(l2_sources)}}},
        {"L1+U1", {{{67}, inclusive_range(16, 31)},
                     {{71}, u1_sources}}},
        {"L0+U1", {{{66}, std::move(l0_sources)},
                     {{71}, u1_sources}}},
        {"U0+U1", {{{70}, std::move(u0_sources)},
                     {{71}, std::move(u1_sources)}}},
        {"G0+L0", {{{64, 66}, inclusive_range(0, 63)}}},
    };
}

inline std::vector<TripleRepairCase> make_hlrc_k64_triple_block_cases()
{
    auto shared_u0_l0_sources = inclusive_range(18, 31);
    const auto l0_data_sources = inclusive_range(0, 15);
    shared_u0_l0_sources.insert(shared_u0_l0_sources.end(),
                                l0_data_sources.begin(), l0_data_sources.end());
    shared_u0_l0_sources.push_back(64);

    auto shared_u0_l0_l1_sources = inclusive_range(16, 31);
    shared_u0_l0_l1_sources.insert(shared_u0_l0_l1_sources.end(),
                                   l0_data_sources.begin(), l0_data_sources.end());
    shared_u0_l0_l1_sources.push_back(64);

    auto l0_sources = l0_data_sources;
    l0_sources.push_back(64);
    auto l2_sources = inclusive_range(32, 47);
    l2_sources.push_back(65);
    auto u1_sources = inclusive_range(33, 47);
    const auto u1_upper_sources = inclusive_range(50, 63);
    u1_sources.insert(u1_sources.end(), u1_upper_sources.begin(), u1_upper_sources.end());

    return {
        {"L0+L1+U0", {{{70, 66, 67}, std::move(shared_u0_l0_l1_sources)}}},
        {"L0+L3+U0", {{{70, 66}, shared_u0_l0_sources},
                         {{69}, inclusive_range(48, 63)}}},
        {"L0+L2+U0", {{{70, 66}, shared_u0_l0_sources},
                         {{68}, l2_sources}}},
        {"L0+L1+L3", {{{66}, l0_sources},
                         {{67}, inclusive_range(16, 31)},
                         {{69}, inclusive_range(48, 63)}}},
        {"L0+L1+L2", {{{66}, l0_sources},
                         {{67}, inclusive_range(16, 31)},
                         {{68}, std::move(l2_sources)}}},
        {"L0+U0+U1", {{{70, 66}, std::move(shared_u0_l0_sources)},
                         {{71}, u1_sources}}},
        {"L0+L1+U1", {{{66}, std::move(l0_sources)},
                         {{67}, inclusive_range(16, 31)},
                         {{71}, std::move(u1_sources)}}},
        {"G0+G1+L1", {{{64, 65, 67}, inclusive_range(0, 63)}}},
    };
}

inline std::vector<QuadrupleRepairCase> make_hlrc_k64_quadruple_block_cases()
{
    auto shared_u0_l0_l1_sources = inclusive_range(16, 31);
    const auto l0_data_sources = inclusive_range(0, 15);
    shared_u0_l0_l1_sources.insert(shared_u0_l0_l1_sources.end(),
                                   l0_data_sources.begin(), l0_data_sources.end());
    shared_u0_l0_l1_sources.push_back(64);

    auto l2_sources = inclusive_range(32, 47);
    l2_sources.push_back(65);
    auto u1_sources = inclusive_range(33, 47);
    const auto u1_upper_sources = inclusive_range(50, 63);
    u1_sources.insert(u1_sources.end(), u1_upper_sources.begin(), u1_upper_sources.end());

    return {
        {"L0+L1+L3+U0", {{{70, 66, 67}, shared_u0_l0_l1_sources},
                            {{69}, inclusive_range(48, 63)}}},
        {"L0+L1+L2+U0", {{{70, 66, 67}, shared_u0_l0_l1_sources},
                            {{68}, std::move(l2_sources)}}},
        {"L0+L1+U0+U1", {{{70, 66, 67}, std::move(shared_u0_l0_l1_sources)},
                            {{71}, std::move(u1_sources)}}},
        {"G0+G1+L1+L3", {{{64, 65, 67, 69}, inclusive_range(0, 63)}}},
    };
}

inline std::vector<DoubleRepairCase> make_optimal_uneven_double_block_cases(int k)
{
    return {{"G0+G1", {{{k, k + 1}, inclusive_range(0, k - 1)}}}};
}

inline std::vector<RepairCase> make_uniform_k64_single_block_cases()
{
    return {{"L0", 67, inclusive_range(0, 21)},
            {"L2", 69, inclusive_range(44, 66)}};
}

inline std::vector<RepairCase> make_uniform_uneven_single_block_cases(int k, int r, int z)
{
    std::vector<RepairCase> cases;
    const auto bounds = get_uniform_lrc_data_like_group_bounds(k, r, z);
    for (int local = 0; local < z; ++local)
        cases.push_back({"L" + std::to_string(local), k + r + local,
                         inclusive_range(bounds[local].first, bounds[local].second - 1)});
    cases.push_back({"G0", k, inclusive_range(0, k - 1)});
    return cases;
}

inline std::vector<DoubleRepairCase> make_uniform_k64_double_block_cases()
{
    return {
        {"L0+L1", {{{67}, inclusive_range(0, 21)},
                     {{68}, inclusive_range(22, 43)}}},
        {"L0+L2", {{{67}, inclusive_range(0, 21)},
                     {{69}, inclusive_range(44, 66)}}},
        {"G0+G1", {{{64, 65}, inclusive_range(0, 63)}}},
    };
}

inline std::vector<DoubleRepairCase> make_uniform_uneven_double_block_cases(int k)
{
    return {{"G0+G1", {{{k, k + 1}, inclusive_range(0, k - 1)}}}};
}

inline std::vector<DoubleRepairCase> make_optimal_k64_double_block_cases()
{
    auto l0_sources = inclusive_range(0, 20);
    l0_sources.insert(l0_sources.end(), {64, 65, 66});
    auto l1_sources = inclusive_range(21, 41);
    l1_sources.insert(l1_sources.end(), {64, 65, 66});
    auto l2_sources = inclusive_range(42, 63);
    l2_sources.insert(l2_sources.end(), {64, 65, 66});
    return {
        {"L0+L1", {{{67}, l0_sources}, {{68}, std::move(l1_sources)}}},
        {"L0+L2", {{{67}, std::move(l0_sources)}, {{69}, std::move(l2_sources)}}},
        {"G0+G1", {{{64, 65}, inclusive_range(0, 63)}}},
    };
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

    if (k == 96)
    {
        return {
            {"L0+L1", {{{99}, inclusive_range(0, 23)},
                         {{100}, inclusive_range(24, 47)}}},
            {"G0+G1", {{{96, 97}, inclusive_range(0, 95)}}},
        };
    }

    if (k == 128)
    {
        return {
            {"L0+L1", {{{131}, inclusive_range(0, 24)},
                         {{132}, inclusive_range(25, 49)}}},
            {"L0+L2", {{{131}, inclusive_range(0, 24)},
                         {{133}, inclusive_range(50, 75)}}},
            {"L2+L3", {{{133}, inclusive_range(50, 75)},
                         {{134}, inclusive_range(76, 101)}}},
            {"G0+G1", {{{128, 129}, inclusive_range(0, 127)}}},
        };
    }

    return {{"G0+G1", {{{k, k + 1}, inclusive_range(0, k - 1)}}}};
}

inline std::vector<TripleRepairCase> make_uniform_k64_triple_block_cases()
{
    return {{"G0+G1+G2", {{{64, 65, 66}, inclusive_range(0, 63)}}}};
}

inline std::vector<TripleRepairCase> make_azure_triple_block_cases(int k)
{
    if (k == 96)
    {
        return {
            {"L0+L1+L2", {{{99}, inclusive_range(0, 23)},
                            {{100}, inclusive_range(24, 47)},
                            {{101}, inclusive_range(48, 71)}}},
            {"G0+G1+G2", {{{96, 97, 98}, inclusive_range(0, 95)}}},
        };
    }
    if (k == 128)
    {
        return {
            {"L0+L1+L2", {{{131}, inclusive_range(0, 24)},
                            {{132}, inclusive_range(25, 49)},
                            {{133}, inclusive_range(50, 75)}}},
            {"L1+L2+L3", {{{132}, inclusive_range(25, 49)},
                            {{133}, inclusive_range(50, 75)},
                            {{134}, inclusive_range(76, 101)}}},
            {"L2+L3+L4", {{{133}, inclusive_range(50, 75)},
                            {{134}, inclusive_range(76, 101)},
                            {{135}, inclusive_range(102, 127)}}},
            {"G0+G1+G2", {{{128, 129, 130}, inclusive_range(0, 127)}}},
        };
    }
    return {{"G0+G1+G2", {{{64, 65, 66}, inclusive_range(0, 63)}}}};
}

inline std::vector<TripleRepairCase> make_azure_k64_triple_block_cases()
{
    return make_azure_triple_block_cases(64);
}

inline std::vector<TripleRepairCase> make_optimal_k64_triple_block_cases()
{
    return {{"G0+G1+G2", {{{64, 65, 66}, inclusive_range(0, 63)}}}};
}

inline std::vector<QuadrupleRepairCase> make_uniform_k64_quadruple_block_cases()
{
    return {{"G0+G1+G2+L0", {{{64, 65, 66, 67}, inclusive_range(0, 63)}}}};
}

inline std::vector<QuadrupleRepairCase> make_azure_quadruple_block_cases(int k)
{
    if (k == 96)
        return {{"G0+G1+G2+L0",
                 {{{96, 97, 98, 99}, inclusive_range(0, 95)}}}};
    if (k == 128)
    {
        return {
            {"L0+L1+L2+L3", {{{131}, inclusive_range(0, 24)},
                               {{132}, inclusive_range(25, 49)},
                               {{133}, inclusive_range(50, 75)},
                               {{134}, inclusive_range(76, 101)}}},
            {"L1+L2+L3+L4", {{{132}, inclusive_range(25, 49)},
                               {{133}, inclusive_range(50, 75)},
                               {{134}, inclusive_range(76, 101)},
                               {{135}, inclusive_range(102, 127)}}},
            {"G0+G1+G2+L0",
             {{{128, 129, 130, 131}, inclusive_range(0, 127)}}},
        };
    }
    return {{"G0+G1+G2+L0", {{{64, 65, 66, 67}, inclusive_range(0, 63)}}}};
}

inline std::vector<QuadrupleRepairCase> make_azure_k64_quadruple_block_cases()
{
    return make_azure_quadruple_block_cases(64);
}

} // namespace RepairBenchmark
} // namespace ECProject
