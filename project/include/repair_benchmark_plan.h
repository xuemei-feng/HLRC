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

inline std::vector<RepairCase> make_nlrc_k96_single_block_cases()
{
    auto l6_sources = inclusive_range(74, 95);
    l6_sources.insert(l6_sources.end(), {96, 97, 98});
    return {{"L0", 99, inclusive_range(0, 23)},
            {"L6", 105, std::move(l6_sources)}};
}

inline std::vector<RepairCase> make_nlrc_k128_single_block_cases()
{
    auto l8_sources = inclusive_range(104, 127);
    l8_sources.insert(l8_sources.end(), {128, 129, 130});
    return {{"L0", 131, inclusive_range(0, 25)},
            {"L8", 139, std::move(l8_sources)}};
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

inline std::vector<DoubleRepairCase> make_nlrc_k96_double_block_cases()
{
    return {
        {"L0+L1", {{{99, 100}, inclusive_range(0, 23)}}},
        {"L2+L3", {{{101, 102}, inclusive_range(24, 48)}}},
        {"L0+L2", {{{99}, inclusive_range(0, 23)},
                      {{101}, inclusive_range(24, 48)}}},
        {"L2+L4", {{{101}, inclusive_range(24, 48)},
                      {{103}, inclusive_range(49, 73)}}},
        {"G0+G1", {{{96, 97}, inclusive_range(0, 95)}}},
    };
}

inline std::vector<DoubleRepairCase> make_nlrc_k128_double_block_cases()
{
    auto l8_sources = inclusive_range(104, 127);
    l8_sources.insert(l8_sources.end(), {128, 129, 130});
    return {
        {"L0+L1", {{{131, 132}, inclusive_range(0, 25)}}},
        {"L0+L2", {{{131}, inclusive_range(0, 25)},
                      {{133}, inclusive_range(26, 51)}}},
        {"L0+L8", {{{131}, inclusive_range(0, 25)},
                      {{139}, std::move(l8_sources)}}},
        {"G0+G1", {{{128, 129}, inclusive_range(0, 127)}}},
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

inline std::vector<TripleRepairCase> make_nlrc_k96_triple_block_cases()
{
    auto l6_sources = inclusive_range(74, 95);
    l6_sources.insert(l6_sources.end(), {96, 97, 98});
    return {
        {"L0+L1+L2", {{{99, 100}, inclusive_range(0, 23)},
                         {{101}, inclusive_range(24, 48)}}},
        {"L2+L3+L4", {{{101, 102}, inclusive_range(24, 48)},
                         {{103}, inclusive_range(49, 73)}}},
        {"L0+L2+L4", {{{99}, inclusive_range(0, 23)},
                         {{101}, inclusive_range(24, 48)},
                         {{103}, inclusive_range(49, 73)}}},
        {"L2+L4+L6", {{{101}, inclusive_range(24, 48)},
                         {{103}, inclusive_range(49, 73)},
                         {{105}, std::move(l6_sources)}}},
        {"G0+G1+G2", {{{96, 97, 98}, inclusive_range(0, 95)}}},
    };
}

inline std::vector<TripleRepairCase> make_nlrc_k128_triple_block_cases()
{
    auto l8_sources = inclusive_range(104, 127);
    l8_sources.insert(l8_sources.end(), {128, 129, 130});
    return {
        {"L0+L1+L2", {{{131, 132}, inclusive_range(0, 25)},
                         {{133}, inclusive_range(26, 51)}}},
        {"L0+L1+L8", {{{131, 132}, inclusive_range(0, 25)},
                         {{139}, l8_sources}}},
        {"L0+L2+L4", {{{131}, inclusive_range(0, 25)},
                         {{133}, inclusive_range(26, 51)},
                         {{135}, inclusive_range(52, 77)}}},
        {"L0+L2+L8", {{{131}, inclusive_range(0, 25)},
                         {{133}, inclusive_range(26, 51)},
                         {{139}, std::move(l8_sources)}}},
        {"G0+G1+G2", {{{128, 129, 130}, inclusive_range(0, 127)}}},
    };
}

inline std::vector<QuadrupleRepairCase> make_nlrc_k128_quadruple_block_cases()
{
    auto l8_sources = inclusive_range(104, 127);
    l8_sources.insert(l8_sources.end(), {128, 129, 130});
    return {
        {"L0+L1+L2+L3", {{{131, 132}, inclusive_range(0, 25)},
                            {{133, 134}, inclusive_range(26, 51)}}},
        {"L0+L1+L2+L4", {{{131, 132}, inclusive_range(0, 25)},
                            {{133}, inclusive_range(26, 51)},
                            {{135}, inclusive_range(52, 77)}}},
        {"L0+L1+L2+L8", {{{131, 132}, inclusive_range(0, 25)},
                            {{133}, inclusive_range(26, 51)},
                            {{139}, l8_sources}}},
        {"L0+L2+L4+L6", {{{131}, inclusive_range(0, 25)},
                            {{133}, inclusive_range(26, 51)},
                            {{135}, inclusive_range(52, 77)},
                            {{137}, inclusive_range(78, 103)}}},
        {"L0+L2+L4+L8", {{{131}, inclusive_range(0, 25)},
                            {{133}, inclusive_range(26, 51)},
                            {{135}, inclusive_range(52, 77)},
                            {{139}, std::move(l8_sources)}}},
        {"G0+G1+G2+L8", {{{128, 129, 130, 139}, inclusive_range(0, 127)}}},
    };
}

inline std::vector<QuadrupleRepairCase> make_nlrc_k96_quadruple_block_cases()
{
    auto l6_sources = inclusive_range(74, 95);
    l6_sources.insert(l6_sources.end(), {96, 97, 98});
    return {
        {"L0+L1+L2+L3", {{{99, 100}, inclusive_range(0, 23)},
                            {{101, 102}, inclusive_range(24, 48)}}},
        {"L2+L3+L4+L5", {{{101, 102}, inclusive_range(24, 48)},
                            {{103, 104}, inclusive_range(49, 73)}}},
        {"L0+L2+L4+L5", {{{99}, inclusive_range(0, 23)},
                            {{101}, inclusive_range(24, 48)},
                            {{103, 104}, inclusive_range(49, 73)}}},
        {"L2+L3+L4+L6", {{{101, 102}, inclusive_range(24, 48)},
                            {{103}, inclusive_range(49, 73)},
                            {{105}, l6_sources}}},
        {"G0+G1+G2+L6", {{{96, 97, 98, 105}, inclusive_range(0, 95)}}},
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

inline std::vector<RepairCase> make_hlrc_k96_single_block_cases()
{
    auto l0_sources = inclusive_range(0, 15);
    l0_sources.push_back(96);
    auto u0_sources = inclusive_range(1, 15);
    const auto middle_sources = inclusive_range(18, 31);
    const auto upper_sources = inclusive_range(34, 47);
    u0_sources.insert(u0_sources.end(), middle_sources.begin(), middle_sources.end());
    u0_sources.insert(u0_sources.end(), upper_sources.begin(), upper_sources.end());
    return {{"L1", 99, inclusive_range(16, 31)},
            {"L0", 98, std::move(l0_sources)},
            {"U0", 104, std::move(u0_sources)}};
}

inline std::vector<RepairCase> make_hlrc_k128_single_block_cases()
{
    auto l0_sources = inclusive_range(0, 15);
    l0_sources.push_back(128);
    auto u0_sources = inclusive_range(1, 15);
    const auto middle_sources = inclusive_range(18, 31);
    const auto upper_sources = inclusive_range(34, 47);
    const auto fourth_sources = inclusive_range(48, 63);
    u0_sources.insert(u0_sources.end(), middle_sources.begin(), middle_sources.end());
    u0_sources.insert(u0_sources.end(), upper_sources.begin(), upper_sources.end());
    u0_sources.insert(u0_sources.end(), fourth_sources.begin(), fourth_sources.end());
    return {{"L1", 131, inclusive_range(16, 31)},
            {"L0", 130, std::move(l0_sources)},
            {"U0", 138, std::move(u0_sources)}};
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

inline std::vector<DoubleRepairCase> make_hlrc_k96_double_block_cases()
{
    auto l0_sources = inclusive_range(0, 15);
    l0_sources.push_back(96);
    auto l3_sources = inclusive_range(48, 63);
    l3_sources.push_back(97);
    auto u0_sources = inclusive_range(1, 15);
    const auto u0_middle = inclusive_range(18, 31);
    const auto u0_upper = inclusive_range(34, 47);
    u0_sources.insert(u0_sources.end(), u0_middle.begin(), u0_middle.end());
    u0_sources.insert(u0_sources.end(), u0_upper.begin(), u0_upper.end());
    auto l1_u0_sources = inclusive_range(16, 31);
    const auto l1_u0_lower = inclusive_range(1, 15);
    const auto l1_u0_upper = inclusive_range(34, 47);
    l1_u0_sources.insert(l1_u0_sources.end(), l1_u0_lower.begin(), l1_u0_lower.end());
    l1_u0_sources.insert(l1_u0_sources.end(), l1_u0_upper.begin(), l1_u0_upper.end());
    auto u1_sources = inclusive_range(49, 63);
    const auto u1_middle = inclusive_range(66, 79);
    const auto u1_upper = inclusive_range(82, 95);
    u1_sources.insert(u1_sources.end(), u1_middle.begin(), u1_middle.end());
    u1_sources.insert(u1_sources.end(), u1_upper.begin(), u1_upper.end());
    return {
        {"L1+L2", {{{99}, inclusive_range(16, 31)},
                      {{100}, inclusive_range(32, 47)}}},
        {"L0+L1", {{{98}, l0_sources}, {{99}, inclusive_range(16, 31)}}},
        {"L0+L3", {{{98}, l0_sources}, {{101}, l3_sources}}},
        {"L1+U0", {{{104, 99}, std::move(l1_u0_sources)}}},
        {"L4+U0", {{{102}, inclusive_range(64, 79)}, {{104}, u0_sources}}},
        {"L3+U0", {{{101}, std::move(l3_sources)}, {{104}, u0_sources}}},
        {"U0+U1", {{{104}, std::move(u0_sources)}, {{105}, std::move(u1_sources)}}},
        {"G0+L0", {{{96, 98}, inclusive_range(0, 95)}}},
    };
}

inline std::vector<DoubleRepairCase> make_hlrc_k128_double_block_cases()
{
    auto l0_sources = inclusive_range(0, 15);
    l0_sources.push_back(128);
    auto l4_sources = inclusive_range(64, 79);
    l4_sources.push_back(129);

    auto u0_sources = inclusive_range(1, 15);
    const auto u0_middle = inclusive_range(18, 31);
    const auto u0_upper = inclusive_range(34, 47);
    const auto u0_fourth = inclusive_range(48, 63);
    u0_sources.insert(u0_sources.end(), u0_middle.begin(), u0_middle.end());
    u0_sources.insert(u0_sources.end(), u0_upper.begin(), u0_upper.end());
    u0_sources.insert(u0_sources.end(), u0_fourth.begin(), u0_fourth.end());

    auto l1_u0_sources = inclusive_range(16, 31);
    const auto l1_u0_lower = inclusive_range(1, 15);
    l1_u0_sources.insert(l1_u0_sources.end(), l1_u0_lower.begin(), l1_u0_lower.end());
    l1_u0_sources.insert(l1_u0_sources.end(), u0_upper.begin(), u0_upper.end());
    l1_u0_sources.insert(l1_u0_sources.end(), u0_fourth.begin(), u0_fourth.end());

    auto u1_sources = inclusive_range(65, 79);
    const auto u1_second = inclusive_range(82, 95);
    const auto u1_third = inclusive_range(98, 111);
    const auto u1_fourth = inclusive_range(114, 127);
    u1_sources.insert(u1_sources.end(), u1_second.begin(), u1_second.end());
    u1_sources.insert(u1_sources.end(), u1_third.begin(), u1_third.end());
    u1_sources.insert(u1_sources.end(), u1_fourth.begin(), u1_fourth.end());

    return {
        {"L1+L2", {{{131}, inclusive_range(16, 31)},
                      {{132}, inclusive_range(32, 47)}}},
        {"L0+L1", {{{130}, l0_sources}, {{131}, inclusive_range(16, 31)}}},
        {"L0+L4", {{{130}, l0_sources}, {{134}, l4_sources}}},
        {"L1+U0", {{{138, 131}, std::move(l1_u0_sources)}}},
        {"L5+U0", {{{135}, inclusive_range(80, 95)}, {{138}, u0_sources}}},
        {"L4+U0", {{{134}, std::move(l4_sources)}, {{138}, u0_sources}}},
        {"U0+U1", {{{138}, std::move(u0_sources)}, {{139}, std::move(u1_sources)}}},
        {"G0+L0", {{{128, 130}, inclusive_range(0, 127)}}},
    };
}

inline std::vector<TripleRepairCase> make_hlrc_k128_triple_block_cases()
{
    auto l0_sources = inclusive_range(0, 15);
    l0_sources.push_back(128);
    auto l4_sources = inclusive_range(64, 79);
    l4_sources.push_back(129);

    auto shared_u0_l1_l2_sources = inclusive_range(16, 31);
    const auto l2_data_sources = inclusive_range(32, 47);
    const auto u0_lower_sources = inclusive_range(1, 15);
    const auto shared_u0_fourth_sources = inclusive_range(50, 63);
    shared_u0_l1_l2_sources.insert(shared_u0_l1_l2_sources.end(),
                                   l2_data_sources.begin(), l2_data_sources.end());
    shared_u0_l1_l2_sources.insert(shared_u0_l1_l2_sources.end(),
                                   u0_lower_sources.begin(), u0_lower_sources.end());
    shared_u0_l1_l2_sources.insert(shared_u0_l1_l2_sources.end(),
                                   shared_u0_fourth_sources.begin(), shared_u0_fourth_sources.end());

    auto shared_u0_l1_sources = inclusive_range(16, 31);
    const auto u0_upper_sources = inclusive_range(34, 47);
    shared_u0_l1_sources.insert(shared_u0_l1_sources.end(),
                                u0_lower_sources.begin(), u0_lower_sources.end());
    shared_u0_l1_sources.insert(shared_u0_l1_sources.end(),
                                u0_upper_sources.begin(), u0_upper_sources.end());
    shared_u0_l1_sources.insert(shared_u0_l1_sources.end(),
                                shared_u0_fourth_sources.begin(), shared_u0_fourth_sources.end());

    auto u0_sources = inclusive_range(1, 15);
    const auto u0_middle_sources = inclusive_range(18, 31);
    const auto u0_fourth_sources = inclusive_range(48, 63);
    u0_sources.insert(u0_sources.end(), u0_middle_sources.begin(), u0_middle_sources.end());
    u0_sources.insert(u0_sources.end(), u0_upper_sources.begin(), u0_upper_sources.end());
    u0_sources.insert(u0_sources.end(), u0_fourth_sources.begin(), u0_fourth_sources.end());

    auto u1_sources = inclusive_range(65, 79);
    const auto u1_second_sources = inclusive_range(82, 95);
    const auto u1_third_sources = inclusive_range(98, 111);
    const auto u1_fourth_sources = inclusive_range(114, 127);
    u1_sources.insert(u1_sources.end(), u1_second_sources.begin(), u1_second_sources.end());
    u1_sources.insert(u1_sources.end(), u1_third_sources.begin(), u1_third_sources.end());
    u1_sources.insert(u1_sources.end(), u1_fourth_sources.begin(), u1_fourth_sources.end());

    return {
        {"L1+L2+L3", {{{131}, inclusive_range(16, 31)},
                         {{132}, inclusive_range(32, 47)},
                         {{133}, inclusive_range(48, 63)}}},
        {"L0+L1+L2", {{{130}, l0_sources},
                         {{131}, inclusive_range(16, 31)},
                         {{132}, inclusive_range(32, 47)}}},
        {"L0+L1+L4", {{{130}, l0_sources},
                         {{131}, inclusive_range(16, 31)},
                         {{134}, l4_sources}}},
        {"L1+L2+U0", {{{138, 131, 132}, std::move(shared_u0_l1_l2_sources)}}},
        {"L1+L5+U0", {{{138, 131}, shared_u0_l1_sources},
                         {{135}, inclusive_range(80, 95)}}},
        {"L1+L4+U0", {{{138, 131}, shared_u0_l1_sources},
                         {{134}, l4_sources}}},
        {"L5+L6+U0", {{{135}, inclusive_range(80, 95)},
                         {{136}, inclusive_range(96, 111)},
                         {{138}, u0_sources}}},
        {"L4+L5+U0", {{{134}, std::move(l4_sources)},
                         {{135}, inclusive_range(80, 95)},
                         {{138}, u0_sources}}},
        {"L1+U0+U1", {{{138, 131}, std::move(shared_u0_l1_sources)},
                         {{139}, std::move(u1_sources)}}},
        {"G0+G1+L1", {{{128, 129, 131}, inclusive_range(0, 127)}}},
    };
}

inline std::vector<TripleRepairCase> make_hlrc_k96_triple_block_cases()
{
    auto shared_u0_l1_l2_sources = inclusive_range(16, 31);
    const auto l2_data_sources = inclusive_range(32, 47);
    const auto u0_lower_sources = inclusive_range(1, 15);
    shared_u0_l1_l2_sources.insert(shared_u0_l1_l2_sources.end(),
                                   l2_data_sources.begin(), l2_data_sources.end());
    shared_u0_l1_l2_sources.insert(shared_u0_l1_l2_sources.end(),
                                   u0_lower_sources.begin(), u0_lower_sources.end());

    auto shared_u0_l1_sources = inclusive_range(16, 31);
    const auto u0_upper_sources = inclusive_range(34, 47);
    shared_u0_l1_sources.insert(shared_u0_l1_sources.end(),
                                u0_lower_sources.begin(), u0_lower_sources.end());
    shared_u0_l1_sources.insert(shared_u0_l1_sources.end(),
                                u0_upper_sources.begin(), u0_upper_sources.end());

    auto l0_sources = inclusive_range(0, 15);
    l0_sources.push_back(96);
    auto l3_sources = inclusive_range(48, 63);
    l3_sources.push_back(97);
    auto u1_sources = inclusive_range(49, 63);
    const auto u1_middle_sources = inclusive_range(66, 79);
    const auto u1_upper_sources = inclusive_range(82, 95);
    u1_sources.insert(u1_sources.end(), u1_middle_sources.begin(), u1_middle_sources.end());
    u1_sources.insert(u1_sources.end(), u1_upper_sources.begin(), u1_upper_sources.end());

    return {
        {"L1+L2+U0", {{{104, 99, 100}, std::move(shared_u0_l1_l2_sources)}}},
        {"L1+L2+L4", {{{99}, inclusive_range(16, 31)},
                         {{100}, inclusive_range(32, 47)},
                         {{102}, inclusive_range(64, 79)}}},
        {"L0+L1+L2", {{{98}, l0_sources},
                         {{99}, inclusive_range(16, 31)},
                         {{100}, inclusive_range(32, 47)}}},
        {"L0+L1+L3", {{{98}, l0_sources},
                         {{99}, inclusive_range(16, 31)},
                         {{101}, l3_sources}}},
        {"L1+L4+U0", {{{104, 99}, shared_u0_l1_sources},
                         {{102}, inclusive_range(64, 79)}}},
        {"L1+L3+U0", {{{104, 99}, shared_u0_l1_sources},
                         {{101}, std::move(l3_sources)}}},
        {"L1+L2+U1", {{{99}, inclusive_range(16, 31)},
                         {{100}, inclusive_range(32, 47)},
                         {{105}, u1_sources}}},
        {"L0+L1+U1", {{{98}, std::move(l0_sources)},
                         {{99}, inclusive_range(16, 31)},
                         {{105}, u1_sources}}},
        {"L1+U0+U1", {{{104, 99}, std::move(shared_u0_l1_sources)},
                         {{105}, std::move(u1_sources)}}},
        {"G0+G1+L1", {{{96, 97, 99}, inclusive_range(0, 95)}}},
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

inline std::vector<QuadrupleRepairCase> make_hlrc_k128_quadruple_block_cases()
{
    auto l0_sources = inclusive_range(0, 15);
    l0_sources.push_back(128);
    auto l4_sources = inclusive_range(64, 79);
    l4_sources.push_back(129);

    auto shared_u0_l1_l2_l3_sources = inclusive_range(1, 15);
    const auto l1_data_sources = inclusive_range(16, 31);
    const auto l2_data_sources = inclusive_range(32, 47);
    const auto l3_data_sources = inclusive_range(48, 63);
    shared_u0_l1_l2_l3_sources.insert(shared_u0_l1_l2_l3_sources.end(),
                                      l1_data_sources.begin(), l1_data_sources.end());
    shared_u0_l1_l2_l3_sources.insert(shared_u0_l1_l2_l3_sources.end(),
                                      l2_data_sources.begin(), l2_data_sources.end());
    shared_u0_l1_l2_l3_sources.insert(shared_u0_l1_l2_l3_sources.end(),
                                      l3_data_sources.begin(), l3_data_sources.end());

    auto shared_u0_l1_l2_sources = inclusive_range(16, 31);
    shared_u0_l1_l2_sources.insert(shared_u0_l1_l2_sources.end(),
                                   l2_data_sources.begin(), l2_data_sources.end());
    const auto u0_lower_sources = inclusive_range(1, 15);
    const auto shared_u0_fourth_sources = inclusive_range(50, 63);
    shared_u0_l1_l2_sources.insert(shared_u0_l1_l2_sources.end(),
                                   u0_lower_sources.begin(), u0_lower_sources.end());
    shared_u0_l1_l2_sources.insert(shared_u0_l1_l2_sources.end(),
                                   shared_u0_fourth_sources.begin(), shared_u0_fourth_sources.end());

    auto shared_u0_l1_sources = inclusive_range(16, 31);
    const auto u0_upper_sources = inclusive_range(34, 47);
    shared_u0_l1_sources.insert(shared_u0_l1_sources.end(),
                                u0_lower_sources.begin(), u0_lower_sources.end());
    shared_u0_l1_sources.insert(shared_u0_l1_sources.end(),
                                u0_upper_sources.begin(), u0_upper_sources.end());
    shared_u0_l1_sources.insert(shared_u0_l1_sources.end(),
                                shared_u0_fourth_sources.begin(), shared_u0_fourth_sources.end());

    auto u0_sources = inclusive_range(1, 15);
    const auto u0_middle_sources = inclusive_range(18, 31);
    u0_sources.insert(u0_sources.end(), u0_middle_sources.begin(), u0_middle_sources.end());
    u0_sources.insert(u0_sources.end(), u0_upper_sources.begin(), u0_upper_sources.end());
    u0_sources.insert(u0_sources.end(), l3_data_sources.begin(), l3_data_sources.end());

    auto shared_u1_l5_sources = inclusive_range(64, 79);
    const auto l5_data_sources = inclusive_range(80, 95);
    const auto u1_third_sources = inclusive_range(98, 111);
    const auto u1_fourth_sources = inclusive_range(114, 127);
    shared_u1_l5_sources.insert(shared_u1_l5_sources.end(),
                                l5_data_sources.begin(), l5_data_sources.end());
    shared_u1_l5_sources.insert(shared_u1_l5_sources.end(),
                                u1_third_sources.begin(), u1_third_sources.end());
    shared_u1_l5_sources.insert(shared_u1_l5_sources.end(),
                                u1_fourth_sources.begin(), u1_fourth_sources.end());

    return {
        {"L1+L2+L3+U0", {{{138, 131, 132, 133}, std::move(shared_u0_l1_l2_l3_sources)}}},
        {"L1+L2+L3+L5", {{{131}, inclusive_range(16, 31)},
                            {{132}, inclusive_range(32, 47)},
                            {{133}, inclusive_range(48, 63)},
                            {{135}, inclusive_range(80, 95)}}},
        {"L0+L1+L2+L3", {{{130}, l0_sources},
                            {{131}, inclusive_range(16, 31)},
                            {{132}, inclusive_range(32, 47)},
                            {{133}, inclusive_range(48, 63)}}},
        {"L0+L1+L2+L4", {{{130}, l0_sources},
                            {{131}, inclusive_range(16, 31)},
                            {{132}, inclusive_range(32, 47)},
                            {{134}, l4_sources}}},
        {"L1+L2+L5+U0", {{{138, 131, 132}, shared_u0_l1_l2_sources},
                            {{135}, inclusive_range(80, 95)}}},
        {"L1+L2+L4+U0", {{{138, 131, 132}, std::move(shared_u0_l1_l2_sources)},
                            {{134}, l4_sources}}},
        {"L1+L5+L6+U0", {{{138, 131}, shared_u0_l1_sources},
                            {{135}, inclusive_range(80, 95)},
                            {{136}, inclusive_range(96, 111)}}},
        {"L1+L4+L5+U0", {{{138, 131}, shared_u0_l1_sources},
                            {{134}, l4_sources},
                            {{135}, inclusive_range(80, 95)}}},
        {"L5+L6+L7+U0", {{{135}, inclusive_range(80, 95)},
                            {{136}, inclusive_range(96, 111)},
                            {{137}, inclusive_range(112, 127)},
                            {{138}, u0_sources}}},
        {"L4+L5+L6+U0", {{{134}, std::move(l4_sources)},
                            {{135}, inclusive_range(80, 95)},
                            {{136}, inclusive_range(96, 111)},
                            {{138}, u0_sources}}},
        {"L1+L5+U0+U1", {{{138, 131}, std::move(shared_u0_l1_sources)},
                            {{139, 135}, std::move(shared_u1_l5_sources)}}},
        {"G0+G1+L1+L2", {{{128, 129, 131, 132}, inclusive_range(0, 127)}}},
    };
}

inline std::vector<QuadrupleRepairCase> make_hlrc_k96_quadruple_block_cases()
{
    auto shared_u0_l0_l1_l2_sources = inclusive_range(0, 15);
    shared_u0_l0_l1_l2_sources.push_back(96);
    const auto l1_data_sources = inclusive_range(16, 31);
    const auto l2_data_sources = inclusive_range(32, 47);
    shared_u0_l0_l1_l2_sources.insert(shared_u0_l0_l1_l2_sources.end(),
                                      l1_data_sources.begin(), l1_data_sources.end());
    shared_u0_l0_l1_l2_sources.insert(shared_u0_l0_l1_l2_sources.end(),
                                      l2_data_sources.begin(), l2_data_sources.end());

    auto shared_u0_l1_l2_sources = inclusive_range(1, 15);
    shared_u0_l1_l2_sources.insert(shared_u0_l1_l2_sources.end(),
                                   l1_data_sources.begin(), l1_data_sources.end());
    shared_u0_l1_l2_sources.insert(shared_u0_l1_l2_sources.end(),
                                   l2_data_sources.begin(), l2_data_sources.end());

    auto shared_u0_l1_sources = inclusive_range(16, 31);
    const auto u0_lower_sources = inclusive_range(1, 15);
    const auto u0_upper_sources = inclusive_range(34, 47);
    shared_u0_l1_sources.insert(shared_u0_l1_sources.end(),
                                u0_lower_sources.begin(), u0_lower_sources.end());
    shared_u0_l1_sources.insert(shared_u0_l1_sources.end(),
                                u0_upper_sources.begin(), u0_upper_sources.end());

    auto shared_u1_l4_sources = inclusive_range(49, 63);
    const auto l4_data_sources = inclusive_range(64, 79);
    const auto u1_upper_sources = inclusive_range(82, 95);
    shared_u1_l4_sources.insert(shared_u1_l4_sources.end(),
                                l4_data_sources.begin(), l4_data_sources.end());
    shared_u1_l4_sources.insert(shared_u1_l4_sources.end(),
                                u1_upper_sources.begin(), u1_upper_sources.end());

    auto l0_sources = inclusive_range(0, 15);
    l0_sources.push_back(96);
    auto l3_sources = inclusive_range(48, 63);
    l3_sources.push_back(97);
    auto u1_sources = inclusive_range(49, 63);
    const auto u1_middle_sources = inclusive_range(66, 79);
    u1_sources.insert(u1_sources.end(), u1_middle_sources.begin(), u1_middle_sources.end());
    u1_sources.insert(u1_sources.end(), u1_upper_sources.begin(), u1_upper_sources.end());

    return {
        {"L0+L1+L2+U0", {{{104, 98, 99, 100}, std::move(shared_u0_l0_l1_l2_sources)}}},
        {"L1+L2+L4+U0", {{{104, 99, 100}, std::move(shared_u0_l1_l2_sources)},
                            {{102}, inclusive_range(64, 79)}}},
        {"L1+L2+L4+L5", {{{99}, inclusive_range(16, 31)},
                            {{100}, inclusive_range(32, 47)},
                            {{102}, inclusive_range(64, 79)},
                            {{103}, inclusive_range(80, 95)}}},
        {"L0+L1+L2+L4", {{{98}, l0_sources},
                            {{99}, inclusive_range(16, 31)},
                            {{100}, inclusive_range(32, 47)},
                            {{102}, inclusive_range(64, 79)}}},
        {"L0+L1+L2+L3", {{{98}, l0_sources},
                            {{99}, inclusive_range(16, 31)},
                            {{100}, inclusive_range(32, 47)},
                            {{101}, l3_sources}}},
        {"L1+L4+L5+U0", {{{104, 99}, shared_u0_l1_sources},
                            {{102}, inclusive_range(64, 79)},
                            {{103}, inclusive_range(80, 95)}}},
        {"L1+L3+L4+U0", {{{104, 99}, shared_u0_l1_sources},
                            {{101}, std::move(l3_sources)},
                            {{102}, inclusive_range(64, 79)}}},
        {"L1+L4+U0+U1", {{{104, 99}, std::move(shared_u0_l1_sources)},
                            {{105, 102}, std::move(shared_u1_l4_sources)}}},
        {"L0+L1+L2+U1", {{{98}, std::move(l0_sources)},
                            {{99}, inclusive_range(16, 31)},
                            {{100}, inclusive_range(32, 47)},
                            {{105}, std::move(u1_sources)}}},
        {"G0+G1+L1+L2", {{{96, 97, 99, 100}, inclusive_range(0, 95)}}},
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
    if (k == 96)
    {
        auto l0_sources = inclusive_range(0, 23);
        auto l1_sources = inclusive_range(24, 47);
        for (int global = 96; global <= 98; ++global)
        {
            l0_sources.push_back(global);
            l1_sources.push_back(global);
        }
        return {
            {"L0+L1", {{{99}, std::move(l0_sources)},
                         {{100}, std::move(l1_sources)}}},
            {"G0+G1", {{{96, 97}, inclusive_range(0, 95)}}},
        };
    }
    if (k == 128)
    {
        auto with_globals = [](std::vector<int> sources) {
            sources.insert(sources.end(), {128, 129, 130});
            return sources;
        };
        return {
            {"L0+L1", {{{131}, with_globals(inclusive_range(0, 24))},
                         {{132}, with_globals(inclusive_range(25, 49))}}},
            {"L0+L2", {{{131}, with_globals(inclusive_range(0, 24))},
                         {{133}, with_globals(inclusive_range(50, 75))}}},
            {"L2+L3", {{{133}, with_globals(inclusive_range(50, 75))},
                         {{134}, with_globals(inclusive_range(76, 101))}}},
            {"G0+G1", {{{128, 129}, inclusive_range(0, 127)}}},
        };
    }
    return {{"G0+G1", {{{k, k + 1}, inclusive_range(0, k - 1)}}}};
}

inline std::vector<RepairCase> make_uniform_k64_single_block_cases()
{
    return {{"L0", 67, inclusive_range(0, 21)},
            {"L2", 69, inclusive_range(44, 66)}};
}

inline std::vector<RepairCase> make_uniform_k96_single_block_cases()
{
    return {{"L0", 99, inclusive_range(0, 23)},
            {"L1", 100, inclusive_range(24, 48)}};
}

inline std::vector<RepairCase> make_uniform_k128_single_block_cases()
{
    auto l4_sources = inclusive_range(104, 127);
    l4_sources.insert(l4_sources.end(), {128, 129, 130});
    return {{"L0", 131, inclusive_range(0, 25)},
            {"L4", 135, std::move(l4_sources)}};
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

inline std::vector<DoubleRepairCase> make_uniform_k96_double_block_cases()
{
    return {
        {"L0+L1", {{{99}, inclusive_range(0, 23)},
                     {{100}, inclusive_range(24, 48)}}},
        {"L1+L2", {{{100}, inclusive_range(24, 48)},
                     {{101}, inclusive_range(49, 73)}}},
        {"G0+G1", {{{96, 97}, inclusive_range(0, 95)}}},
    };
}

inline std::vector<DoubleRepairCase> make_uniform_k128_double_block_cases()
{
    auto l4_sources = inclusive_range(104, 127);
    l4_sources.insert(l4_sources.end(), {128, 129, 130});
    return {
        {"L0+L1", {{{131}, inclusive_range(0, 25)},
                     {{132}, inclusive_range(26, 51)}}},
        {"L0+L4", {{{131}, inclusive_range(0, 25)},
                     {{135}, std::move(l4_sources)}}},
        {"G0+G1", {{{128, 129}, inclusive_range(0, 127)}}},
    };
}

inline std::vector<DoubleRepairCase> make_uniform_uneven_double_block_cases(int k)
{
    return {{"G0+G1", {{{k, k + 1}, inclusive_range(0, k - 1)}}}};
}

inline std::vector<TripleRepairCase> make_uniform_k96_triple_block_cases()
{
    auto l3_sources = inclusive_range(74, 95);
    l3_sources.insert(l3_sources.end(), {96, 97, 98});
    return {
        {"L0+L1+L2", {{{99}, inclusive_range(0, 23)},
                         {{100}, inclusive_range(24, 48)},
                         {{101}, inclusive_range(49, 73)}}},
        {"L1+L2+L3", {{{100}, inclusive_range(24, 48)},
                         {{101}, inclusive_range(49, 73)},
                         {{102}, std::move(l3_sources)}}},
        {"G0+G1+G2", {{{96, 97, 98}, inclusive_range(0, 95)}}},
    };
}

inline std::vector<TripleRepairCase> make_uniform_k128_triple_block_cases()
{
    auto l4_sources = inclusive_range(104, 127);
    l4_sources.insert(l4_sources.end(), {128, 129, 130});
    return {
        {"L0+L1+L2", {{{131}, inclusive_range(0, 25)},
                         {{132}, inclusive_range(26, 51)},
                         {{133}, inclusive_range(52, 77)}}},
        {"L0+L1+L4", {{{131}, inclusive_range(0, 25)},
                         {{132}, inclusive_range(26, 51)},
                         {{135}, std::move(l4_sources)}}},
        {"G0+G1+G2", {{{128, 129, 130}, inclusive_range(0, 127)}}},
    };
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

inline std::vector<TripleRepairCase> make_optimal_triple_block_cases(int k)
{
    if (k == 96)
    {
        auto l0_sources = inclusive_range(0, 23);
        auto l1_sources = inclusive_range(24, 47);
        auto l2_sources = inclusive_range(48, 71);
        for (int global = 96; global <= 98; ++global)
        {
            l0_sources.push_back(global);
            l1_sources.push_back(global);
            l2_sources.push_back(global);
        }
        return {
            {"L0+L1+L2", {{{99}, std::move(l0_sources)},
                            {{100}, std::move(l1_sources)},
                            {{101}, std::move(l2_sources)}}},
            {"G0+G1+G2", {{{96, 97, 98}, inclusive_range(0, 95)}}},
        };
    }
    if (k == 128)
    {
        auto with_globals = [](std::vector<int> sources) {
            sources.insert(sources.end(), {128, 129, 130});
            return sources;
        };
        return {
            {"L0+L1+L2", {{{131}, with_globals(inclusive_range(0, 24))},
                            {{132}, with_globals(inclusive_range(25, 49))},
                            {{133}, with_globals(inclusive_range(50, 75))}}},
            {"L1+L2+L3", {{{132}, with_globals(inclusive_range(25, 49))},
                            {{133}, with_globals(inclusive_range(50, 75))},
                            {{134}, with_globals(inclusive_range(76, 101))}}},
            {"L2+L3+L4", {{{133}, with_globals(inclusive_range(50, 75))},
                            {{134}, with_globals(inclusive_range(76, 101))},
                            {{135}, with_globals(inclusive_range(102, 127))}}},
            {"G0+G1+G2", {{{128, 129, 130}, inclusive_range(0, 127)}}},
        };
    }
    return {{"G0+G1+G2", {{{64, 65, 66}, inclusive_range(0, 63)}}}};
}

inline std::vector<TripleRepairCase> make_optimal_k64_triple_block_cases()
{
    return make_optimal_triple_block_cases(64);
}

inline std::vector<QuadrupleRepairCase> make_optimal_quadruple_block_cases(int k)
{
    if (k == 96)
        return {{"G0+G1+G2+L0",
                 {{{96, 97, 98, 99}, inclusive_range(0, 95)}}}};
    if (k == 128)
    {
        auto with_globals = [](std::vector<int> sources) {
            sources.insert(sources.end(), {128, 129, 130});
            return sources;
        };
        return {
            {"L0+L1+L2+L3", {{{131}, with_globals(inclusive_range(0, 24))},
                               {{132}, with_globals(inclusive_range(25, 49))},
                               {{133}, with_globals(inclusive_range(50, 75))},
                               {{134}, with_globals(inclusive_range(76, 101))}}},
            {"L1+L2+L3+L4", {{{132}, with_globals(inclusive_range(25, 49))},
                               {{133}, with_globals(inclusive_range(50, 75))},
                               {{134}, with_globals(inclusive_range(76, 101))},
                               {{135}, with_globals(inclusive_range(102, 127))}}},
            {"G0+G1+G2+L0",
             {{{128, 129, 130, 131}, inclusive_range(0, 127)}}},
        };
    }
    return {};
}

inline std::vector<QuadrupleRepairCase> make_uniform_k96_quadruple_block_cases()
{
    return {{"G0+G1+G2+L0",
             {{{96, 97, 98, 99}, inclusive_range(0, 95)}}}};
}

inline std::vector<QuadrupleRepairCase> make_uniform_k128_quadruple_block_cases()
{
    auto l4_sources = inclusive_range(104, 127);
    l4_sources.insert(l4_sources.end(), {128, 129, 130});
    return {
        {"L0+L1+L2+L3", {{{131}, inclusive_range(0, 25)},
                            {{132}, inclusive_range(26, 51)},
                            {{133}, inclusive_range(52, 77)},
                            {{134}, inclusive_range(78, 103)}}},
        {"L0+L1+L2+L4", {{{131}, inclusive_range(0, 25)},
                            {{132}, inclusive_range(26, 51)},
                            {{133}, inclusive_range(52, 77)},
                            {{135}, std::move(l4_sources)}}},
        {"G0+G1+G2+L4",
         {{{128, 129, 130, 135}, inclusive_range(0, 127)}}},
    };
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
