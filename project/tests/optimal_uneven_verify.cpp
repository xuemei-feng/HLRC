#include "encoder.h"
#include "lrc.h"
#include "repair_benchmark_plan.h"

#include <algorithm>
#include <iostream>
#include <numeric>
#include <random>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace {
int failures = 0;

void check(bool condition, const std::string &message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

std::vector<int> range(int begin, int end)
{
    std::vector<int> ids(std::max(0, end - begin));
    std::iota(ids.begin(), ids.end(), begin);
    return ids;
}

std::unordered_map<int, std::vector<int>> legacy_placement(int k, int r, int z)
{
    std::unordered_map<int, std::vector<int>> groups;
    const int local_size = k / z;
    const int groups_per_local = local_size / (r + 1) + 1;
    const int slots = local_size + 1;
    const int base = slots / groups_per_local;
    const int extra = slots % groups_per_local;
    int data = 0;
    int group = 0;
    for (int local = 0; local < z; ++local) {
        for (int part = 0; part < groups_per_local; ++part) {
            const int part_slots = base + (part < extra ? 1 : 0);
            const bool last = part == groups_per_local - 1;
            for (int i = 0; i < part_slots - (last ? 1 : 0); ++i)
                groups[group].push_back(data++);
            if (last)
                groups[group].push_back(k + r + local);
            ++group;
        }
    }
    for (int global = 0; global < r; ++global)
        groups[group].push_back(k + global);
    return groups;
}

unsigned char evaluate_row(const unsigned char *row, const std::vector<unsigned char> &data)
{
    unsigned char value = 0;
    for (size_t i = 0; i < data.size(); ++i)
        value ^= ECProject::gf_mul(row[i], data[i]);
    return value;
}

void verify_case(int k, int r, int z, const std::vector<int> &expected_sizes,
                 const std::vector<std::pair<int, int>> &expected_bounds)
{
    const int rows = k + r + z;
    std::vector<unsigned char> matrix(static_cast<size_t>(rows) * k);
    ECProject::gen_optimal_lrc_matrix(matrix.data(), k, r, z);

    check(ECProject::get_optimal_lrc_local_group_sizes(k, z) == expected_sizes,
          "local sizes mismatch for k=" + std::to_string(k));
    const auto bounds = ECProject::get_optimal_lrc_local_group_bounds(k, z);
    check(bounds == expected_bounds, "local bounds mismatch for k=" + std::to_string(k));

    std::vector<unsigned char> expected_cauchy(static_cast<size_t>(k + r) * k);
    ECProject::gf_gen_cauchy_matrix1(expected_cauchy.data(), k + r, k);
    for (int global = 0; global < r; ++global) {
        for (int data = 0; data < k; ++data) {
            const unsigned char actual = matrix[static_cast<size_t>(k + global) * k + data];
            check(actual != 0, "global coefficient is zero");
            check(actual == expected_cauchy[static_cast<size_t>(k + global) * k + data],
                  "global Cauchy coefficient mismatch");
        }
    }

    std::vector<unsigned char> local_vector(k);
    ECProject::gf_gen_local_vector(local_vector.data(), k, r);
    for (int local = 0; local < z; ++local) {
        for (int data = 0; data < k; ++data) {
            unsigned char expected = 0;
            if (data >= bounds[local].first && data < bounds[local].second)
                expected = local_vector[data];
            for (int global = 0; global < r; ++global)
                expected ^= matrix[static_cast<size_t>(k + global) * k + data];
            check(matrix[static_cast<size_t>(k + r + local) * k + data] == expected,
                  "weighted local row mismatch");
        }
    }

    std::vector<int> legacy(static_cast<size_t>(r + z) * k);
    check(ECProject::lrc_make_matrix(k, r, z, legacy.data(), ECProject::Optimal_Cauchy_LRC),
          "legacy matrix generation failed");
    for (int row = 0; row < r + z; ++row)
        for (int data = 0; data < k; ++data)
            check(legacy[static_cast<size_t>(row) * k + data] ==
                      matrix[static_cast<size_t>(k + row) * k + data],
                  "legacy/main matrix mismatch");

    std::mt19937 generator(0x4f50544dU + static_cast<unsigned>(k));
    std::uniform_int_distribution<int> byte(0, 255);
    std::vector<unsigned char> random_data(k);
    for (auto &value : random_data)
        value = static_cast<unsigned char>(byte(generator));
    for (int row = 0; row < r + z; ++row) {
        std::vector<unsigned char> legacy_row(k);
        for (int data = 0; data < k; ++data)
            legacy_row[data] = static_cast<unsigned char>(legacy[static_cast<size_t>(row) * k + data]);
        check(evaluate_row(legacy_row.data(), random_data) ==
                  evaluate_row(matrix.data() + static_cast<size_t>(k + row) * k, random_data),
              "random encoded parity mismatch");
    }

    const auto groups = ECProject::get_optimal_lrc_group_id_to_block_ids(k, r, z);
    const auto block_to_group = ECProject::get_optimal_lrc_block_id_to_group_id(k, r, z);
    check(static_cast<int>(block_to_group.size()) == rows, "placement does not cover every block");
    std::vector<int> occurrences(rows, 0);
    for (const auto &entry : groups) {
        for (int block : entry.second) {
            check(block >= 0 && block < rows, "placement contains out-of-range block");
            if (block >= 0 && block < rows)
                ++occurrences[block];
            check(block_to_group.at(block) == entry.first, "placement maps are not inverse");
        }
    }
    check(std::all_of(occurrences.begin(), occurrences.end(), [](int count) { return count == 1; }),
          "placement is not a complete bijection");

    for (int data = 0; data < k; ++data) {
        const auto it = std::find_if(bounds.begin(), bounds.end(), [data](const auto &bound) {
            return data >= bound.first && data < bound.second;
        });
        check(ECProject::get_optimal_lrc_block_id_to_local_group_id(k, r, z, data) ==
                  static_cast<int>(it - bounds.begin()),
              "data/local boundary mapping mismatch");
    }
    for (int local = 0; local < z; ++local)
        check(ECProject::get_optimal_lrc_block_id_to_local_group_id(k, r, z, k + r + local) == local,
              "local parity/local group mapping mismatch");

    std::vector<unsigned char> coefficients;
    if (k == 64) {
        const auto singles = ECProject::RepairBenchmark::make_optimal_k64_single_block_cases();
        auto l0_sources = range(0, 21);
        const auto globals = range(64, 67);
        l0_sources.insert(l0_sources.end(), globals.begin(), globals.end());
        auto l2_sources = range(42, 64);
        l2_sources.insert(l2_sources.end(), globals.begin(), globals.end());
        const std::vector<ECProject::RepairBenchmark::RepairCase> expected = {
            {"L0", 67, l0_sources}, {"L2", 69, l2_sources}};
        check(singles.size() == expected.size(), "k64 registered single case count mismatch");
        for (size_t index = 0; index < std::min(singles.size(), expected.size()); ++index) {
            check(singles[index].failed_name == expected[index].failed_name,
                  "k64 single case name/order mismatch");
            check(singles[index].failed_block_id == expected[index].failed_block_id,
                  "k64 single failed ID/order mismatch");
            check(singles[index].source_block_ids == expected[index].source_block_ids,
                  "k64 single source order mismatch");
            check(ECProject::get_local_fill_plan(k, r, z, "OptimalLRC",
                                                 {singles[index].failed_block_id},
                                                 singles[index].source_block_ids, coefficients),
                  "k64 single case is not recoverable");
        }
    } else {
        const auto singles = ECProject::RepairBenchmark::make_optimal_uneven_single_block_cases(k, r, z);
        check(singles.size() == static_cast<size_t>(z + 1),
              "registered single case count mismatch");
        for (int local = 0; local < z; ++local) {
            const auto &test_case = singles[local];
            std::vector<int> expected_sources = range(bounds[local].first, bounds[local].second);
            const auto globals = range(k, k + r);
            expected_sources.insert(expected_sources.end(), globals.begin(), globals.end());
            check(test_case.failed_name == "L" + std::to_string(local),
                  "single local case name mismatch");
            check(test_case.failed_block_id == k + r + local, "single local failed ID mismatch");
            check(test_case.source_block_ids == expected_sources, "single local sources mismatch");
            check(ECProject::get_local_fill_plan(k, r, z, "OptimalLRC", {test_case.failed_block_id},
                                                 test_case.source_block_ids, coefficients),
                  "single local case is not recoverable");
        }
        check(singles.back().failed_name == "G0" && singles.back().failed_block_id == k &&
                  singles.back().source_block_ids == range(0, k),
              "G0 single case mismatch");
        check(ECProject::get_local_fill_plan(k, r, z, "OptimalLRC", {k}, range(0, k), coefficients),
              "G0 single case is not recoverable");
    }

    const auto doubles = k == 64
        ? ECProject::RepairBenchmark::make_optimal_k64_double_block_cases()
        : ECProject::RepairBenchmark::make_optimal_uneven_double_block_cases(k);
    std::vector<ECProject::RepairBenchmark::DoubleRepairCase> expected_doubles;
    if (k == 64) {
        auto l0_sources = range(0, 21);
        auto l1_sources = range(21, 42);
        auto l2_sources = range(42, 64);
        const auto globals = range(64, 67);
        l0_sources.insert(l0_sources.end(), globals.begin(), globals.end());
        l1_sources.insert(l1_sources.end(), globals.begin(), globals.end());
        l2_sources.insert(l2_sources.end(), globals.begin(), globals.end());
        expected_doubles = {
            {"L0+L1", {{{67}, l0_sources}, {{68}, l1_sources}}},
            {"L0+L2", {{{67}, l0_sources}, {{69}, l2_sources}}},
            {"G0+G1", {{{64, 65}, range(0, 64)}}},
        };
    } else if (k == 96) {
        auto l0_sources = range(0, 24);
        auto l1_sources = range(24, 48);
        const auto globals = range(96, 99);
        l0_sources.insert(l0_sources.end(), globals.begin(), globals.end());
        l1_sources.insert(l1_sources.end(), globals.begin(), globals.end());
        expected_doubles = {
            {"L0+L1", {{{99}, l0_sources}, {{100}, l1_sources}}},
            {"G0+G1", {{{96, 97}, range(0, 96)}}},
        };
    } else if (k == 128) {
        auto with_globals = [](std::vector<int> sources) {
            sources.insert(sources.end(), {128, 129, 130});
            return sources;
        };
        expected_doubles = {
            {"L0+L1", {{{131}, with_globals(range(0, 25))},
                         {{132}, with_globals(range(25, 50))}}},
            {"L0+L2", {{{131}, with_globals(range(0, 25))},
                         {{133}, with_globals(range(50, 76))}}},
            {"L2+L3", {{{133}, with_globals(range(50, 76))},
                         {{134}, with_globals(range(76, 102))}}},
            {"G0+G1", {{{128, 129}, range(0, 128)}}},
        };
    } else {
        expected_doubles = {{"G0+G1", {{{k, k + 1}, range(0, k)}}}};
    }
    check(doubles.size() == expected_doubles.size(), "double case count mismatch");
    for (size_t case_index = 0;
         case_index < std::min(doubles.size(), expected_doubles.size()); ++case_index) {
        const auto &actual_case = doubles[case_index];
        const auto &expected_case = expected_doubles[case_index];
        check(actual_case.failed_name == expected_case.failed_name,
              "double case name/order mismatch");
        check(actual_case.tasks.size() == expected_case.tasks.size(),
              "double task count mismatch");
        for (size_t task_index = 0;
             task_index < std::min(actual_case.tasks.size(), expected_case.tasks.size());
             ++task_index) {
            const auto &actual_task = actual_case.tasks[task_index];
            const auto &expected_task = expected_case.tasks[task_index];
            check(actual_task.failed_block_ids == expected_task.failed_block_ids,
                  "double failed ID/order mismatch");
            check(actual_task.source_block_ids == expected_task.source_block_ids,
                  "double source order mismatch");
            check(ECProject::get_local_fill_plan(k, r, z, "OptimalLRC",
                                                 actual_task.failed_block_ids,
                                                 actual_task.source_block_ids, coefficients),
                  "double task is not recoverable");
        }
    }
    if (!doubles.empty()) {
        const auto &global_case = doubles.back();
        check(global_case.failed_name == "G0+G1" && global_case.tasks.size() == 1,
              "G0+G1 must be one shared task");
        if (global_case.tasks.size() == 1)
            check(global_case.tasks[0].failed_block_ids == std::vector<int>({k, k + 1}),
                  "G0+G1 must put G0 first for compute-proxy selection");
    }

    if (k == 64 || k == 96 || k == 128) {
        const auto triples = ECProject::RepairBenchmark::make_optimal_triple_block_cases(k);
        const size_t expected_count = k == 96 ? 2U : (k == 128 ? 4U : 1U);
        check(triples.size() == expected_count, "triple case count mismatch");
        if (k == 96 && triples.size() == 2) {
            auto l0_sources = range(0, 24);
            auto l1_sources = range(24, 48);
            auto l2_sources = range(48, 72);
            const auto globals = range(96, 99);
            l0_sources.insert(l0_sources.end(), globals.begin(), globals.end());
            l1_sources.insert(l1_sources.end(), globals.begin(), globals.end());
            l2_sources.insert(l2_sources.end(), globals.begin(), globals.end());
            const auto &local = triples[0];
            check(local.failed_name == "L0+L1+L2" && local.tasks.size() == 3,
                  "k96 local triple case changed");
            const std::vector<int> failed = {99, 100, 101};
            const std::vector<std::vector<int>> sources = {l0_sources, l1_sources, l2_sources};
            for (size_t i = 0; i < std::min<size_t>(3, local.tasks.size()); ++i) {
                check(local.tasks[i].failed_block_ids == std::vector<int>({failed[i]}),
                      "k96 local triple failed ID changed");
                check(local.tasks[i].source_block_ids == sources[i],
                      "k96 local triple source order changed");
                coefficients.clear();
                check(ECProject::get_local_fill_plan(k, r, z, "OptimalLRC",
                                                     local.tasks[i].failed_block_ids,
                                                     local.tasks[i].source_block_ids, coefficients),
                      "k96 local triple task is not recoverable");
            }
        }
        if (k == 128 && triples.size() == 4) {
            auto with_globals = [](std::vector<int> sources) {
                sources.insert(sources.end(), {128, 129, 130});
                return sources;
            };
            const std::vector<std::string> names = {"L0+L1+L2", "L1+L2+L3", "L2+L3+L4"};
            const std::vector<std::vector<int>> failed = {{131,132,133},{132,133,134},{133,134,135}};
            const std::vector<std::vector<std::vector<int>>> sources = {
                {with_globals(range(0,25)),with_globals(range(25,50)),with_globals(range(50,76))},
                {with_globals(range(25,50)),with_globals(range(50,76)),with_globals(range(76,102))},
                {with_globals(range(50,76)),with_globals(range(76,102)),with_globals(range(102,128))}};
            for (size_t c = 0; c < 3; ++c) {
                check(triples[c].failed_name == names[c] && triples[c].tasks.size() == 3, "k128 local triple case changed");
                for (size_t t = 0; t < std::min<size_t>(3, triples[c].tasks.size()); ++t) {
                    check(triples[c].tasks[t].failed_block_ids == std::vector<int>({failed[c][t]}), "k128 triple failed ID changed");
                    check(triples[c].tasks[t].source_block_ids == sources[c][t], "k128 triple sources changed");
                    coefficients.clear();
                    check(ECProject::get_local_fill_plan(k,r,z,"OptimalLRC",triples[c].tasks[t].failed_block_ids,triples[c].tasks[t].source_block_ids,coefficients), "k128 local triple not recoverable");
                }
            }
        }
        if (!triples.empty()) {
            const auto &global = triples.back();
            check(global.failed_name == "G0+G1+G2" && global.tasks.size() == 1,
                  "global triple must use one shared task");
            if (global.tasks.size() == 1) {
                const auto &task = global.tasks[0];
                check(task.failed_block_ids == std::vector<int>({k, k + 1, k + 2}),
                      "global triple failed ID/order mismatch");
                check(task.source_block_ids == range(0, k),
                      "global triple source order mismatch");
                coefficients.clear();
                check(ECProject::get_local_fill_plan(k, r, z, "OptimalLRC",
                                                     task.failed_block_ids,
                                                     task.source_block_ids, coefficients),
                      "global triple task is not recoverable");
                check(coefficients.size() == static_cast<size_t>(3 * k),
                      "global triple coefficient count mismatch");
            }
        }
    }

    if (k == 96 || k == 128) {
        const auto quadruples = ECProject::RepairBenchmark::make_optimal_quadruple_block_cases(k);
        if (k == 128) {
            check(quadruples.size() == 3, "k128 quadruple case count mismatch");
            auto with_globals = [](std::vector<int> sources) {
                sources.insert(sources.end(), {128, 129, 130});
                return sources;
            };
            const std::vector<std::string> names = {"L0+L1+L2+L3", "L1+L2+L3+L4"};
            const std::vector<std::vector<int>> failed = {{131,132,133,134},{132,133,134,135}};
            const std::vector<std::vector<std::vector<int>>> sources = {
                {with_globals(range(0,25)),with_globals(range(25,50)),with_globals(range(50,76)),with_globals(range(76,102))},
                {with_globals(range(25,50)),with_globals(range(50,76)),with_globals(range(76,102)),with_globals(range(102,128))}};
            for (size_t c = 0; c < 2 && c < quadruples.size(); ++c) {
                check(quadruples[c].failed_name == names[c] && quadruples[c].tasks.size() == 4, "k128 local quadruple case changed");
                for (size_t t = 0; t < std::min<size_t>(4, quadruples[c].tasks.size()); ++t) {
                    check(quadruples[c].tasks[t].failed_block_ids == std::vector<int>({failed[c][t]}), "k128 quadruple failed ID changed");
                    check(quadruples[c].tasks[t].source_block_ids == sources[c][t], "k128 quadruple sources changed");
                    coefficients.clear();
                    check(ECProject::get_local_fill_plan(k,r,z,"OptimalLRC",quadruples[c].tasks[t].failed_block_ids,quadruples[c].tasks[t].source_block_ids,coefficients), "k128 local quadruple not recoverable");
                }
            }
            if (quadruples.size() == 3) {
                const auto &task = quadruples[2].tasks[0];
                check(quadruples[2].failed_name == "G0+G1+G2+L0" && quadruples[2].tasks.size() == 1, "k128 global quadruple must use one task");
                check(task.failed_block_ids == std::vector<int>({128,129,130,131}), "k128 global quadruple failed order changed");
                check(task.source_block_ids == range(0,128), "k128 global quadruple sources changed");
                coefficients.clear();
                check(ECProject::get_local_fill_plan(k,r,z,"OptimalLRC",task.failed_block_ids,task.source_block_ids,coefficients), "k128 global quadruple not recoverable");
                check(coefficients.size() == 4U * 128U, "k128 global quadruple coefficient count mismatch");
            }
        }
        if (k == 96) {
        check(quadruples.size() == 1, "k96 quadruple case count mismatch");
        if (quadruples.size() == 1) {
            const auto &test_case = quadruples[0];
            check(test_case.failed_name == "G0+G1+G2+L0" && test_case.tasks.size() == 1,
                  "k96 quadruple must use one shared task");
            if (test_case.tasks.size() == 1) {
                const auto &task = test_case.tasks[0];
                check(task.failed_block_ids == std::vector<int>({96, 97, 98, 99}),
                      "k96 quadruple failed ID/order mismatch");
                check(task.source_block_ids == range(0, 96),
                      "k96 quadruple source order mismatch");
                coefficients.clear();
                check(ECProject::get_local_fill_plan(k, r, z, "OptimalLRC",
                                                     task.failed_block_ids,
                                                     task.source_block_ids, coefficients),
                      "k96 quadruple task is not recoverable");
                check(coefficients.size() == 4U * 96U,
                      "k96 quadruple coefficient count mismatch");
            }
        }
        }
    }

    for (int failed : {bounds.back().first, bounds.back().second - 1, k + r + z - 1}) {
        const auto plan = ECProject::get_recovery_group_and_block_ids_optimal_lrc(k, r, z, failed);
        check(!plan.empty(), "boundary recovery plan is empty");
        std::vector<int> sources;
        for (const auto &entry : plan)
            sources.insert(sources.end(), entry.second.begin(), entry.second.end());
        check(ECProject::get_local_fill_plan(k, r, z, "OptimalLRC", {failed}, sources, coefficients),
              "boundary recovery plan is not solvable");
    }
}

void verify_legacy_placements()
{
    for (const auto &[k, r, z] : std::vector<std::tuple<int, int, int>>{
             {24, 2, 2}, {48, 3, 4}, {72, 4, 4}}) {
        check(ECProject::get_optimal_lrc_group_id_to_block_ids(k, r, z) == legacy_placement(k, r, z),
              "legacy Optimal placement changed for k=" + std::to_string(k));
    }
}
} // namespace

int main()
{
    verify_case(64, 3, 3, {21, 21, 22}, {{0, 21}, {21, 42}, {42, 64}});
    verify_case(128, 3, 5, {25, 25, 26, 26, 26},
                {{0, 25}, {25, 50}, {50, 76}, {76, 102}, {102, 128}});
    verify_case(96, 3, 4, {24, 24, 24, 24},
                {{0, 24}, {24, 48}, {48, 72}, {72, 96}});
    verify_legacy_placements();
    if (failures != 0) {
        std::cerr << failures << " Optimal uneven verification(s) failed\n";
        return 1;
    }
    std::cout << "Optimal uneven verification passed\n";
    return 0;
}
