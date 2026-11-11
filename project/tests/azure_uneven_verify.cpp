#include "encoder.h"
#include "lrc.h"
#include "repair_benchmark_plan.h"

#include <algorithm>
#include <iostream>
#include <numeric>
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
    std::vector<int> result(end - begin);
    std::iota(result.begin(), result.end(), begin);
    return result;
}

void verify_new_case(int k, int r, int z, const std::vector<int> &expected_sizes)
{
    const int rows = k + r + z;
    std::vector<unsigned char> matrix(static_cast<size_t>(rows) * k, 0);
    ECProject::gen_azure_lrc_matrix(matrix.data(), k, r, z);

    check(ECProject::get_azure_lrc_local_group_sizes(k, z) == expected_sizes,
          "unexpected local group sizes for k=" + std::to_string(k));
    const auto bounds = ECProject::get_azure_lrc_local_group_bounds(k, z);
    int offset = 0;
    for (int group = 0; group < z; ++group) {
        check(bounds[group] == std::make_pair(offset, offset + expected_sizes[group]),
              "unexpected local group bounds for k=" + std::to_string(k));
        offset += expected_sizes[group];
    }

    for (int global = 0; global < 3; ++global)
        for (int data = 0; data < k; ++data)
            check(matrix[static_cast<size_t>(k + global) * k + data] != 0,
                  "global row has zero data coefficient");

    for (int group = 0; group < z; ++group) {
        const int local_id = k + r + group;
        for (int data = 0; data < k; ++data) {
            const bool covered = data >= bounds[group].first && data < bounds[group].second;
            check(matrix[static_cast<size_t>(local_id) * k + data] == (covered ? 1 : 0),
                  "local row coverage mismatch");
            check(ECProject::get_azurelrc_block_id_to_local_group_id(k, r, z, data) ==
                      std::find_if(bounds.begin(), bounds.end(), [data](const auto &bound) {
                          return data >= bound.first && data < bound.second;
                      }) - bounds.begin(),
                  "data-to-local mapping mismatch");
        }
        check(ECProject::get_azurelrc_block_id_to_local_group_id(k, r, z, local_id) == group,
              "local parity mapping mismatch");

        std::vector<unsigned char> coeffs;
        check(ECProject::get_local_fill_plan(k, r, z, "AzureLRC", {local_id},
                                             range(bounds[group].first, bounds[group].second), coeffs),
              "local parity repair plan is not solvable");
        check(std::all_of(coeffs.begin(), coeffs.end(), [](unsigned char value) { return value == 1; }),
              "local parity repair coefficients are not XOR");
    }

    check(ECProject::get_azurelrc_block_id_to_local_group_id(k, r, z, k) == -1,
          "global parity should not map to a local group");
    std::vector<unsigned char> coeffs;
    const auto all_data = range(0, k);
    check(ECProject::get_local_fill_plan(k, r, z, "AzureLRC", {k}, all_data, coeffs),
          "G0 repair plan is not solvable");
    check(ECProject::get_local_fill_plan(k, r, z, "AzureLRC", {k, k + 1}, all_data, coeffs),
          "G0+G1 repair plan is not solvable");

    const auto groups = ECProject::get_azurelrc_group_id_to_block_ids(k, r, z);
    const auto block_to_group = ECProject::get_azurelrc_block_id_to_group_id(k, r, z);
    check(static_cast<int>(block_to_group.size()) == rows, "placement omits or duplicates blocks");
    for (const auto &entry : groups)
        for (int block : entry.second)
            check(block_to_group.at(block) == entry.first, "placement maps disagree");
}

void verify_benchmark_failure_types()
{
    using ECProject::RepairBenchmark::is_supported_failure_type;

    check(is_supported_failure_type("single"),
          "benchmark must accept failure type single");
    check(is_supported_failure_type("double"),
          "benchmark must accept failure type double");
    check(is_supported_failure_type("triple"),
          "benchmark must accept failure type triple");
    check(!is_supported_failure_type("multi"),
          "benchmark must reject legacy failure type multi");
    check(is_supported_failure_type("quadruple"),
          "benchmark must accept failure type quadruple");
    check(!is_supported_failure_type("quintuple"),
          "benchmark must reject unimplemented failure type quintuple");
}

void verify_k64_benchmark_single_cases()
{
    const auto cases = ECProject::RepairBenchmark::make_azure_uneven_single_block_cases(64, 3, 3);
    const std::vector<std::string> expected_names = {"L0", "L2", "G0"};
    const std::vector<int> expected_failed_ids = {67, 69, 64};
    const std::vector<std::vector<int>> expected_sources = {
        range(0, 21), range(42, 64), range(0, 64)};

    check(cases.size() == expected_names.size(),
          "k=64 benchmark single plan must contain exactly three cases");
    if (cases.size() != expected_names.size())
        return;

    for (size_t i = 0; i < cases.size(); ++i) {
        check(cases[i].failed_name == expected_names[i],
              "k=64 benchmark case name or order mismatch");
        check(cases[i].failed_block_id == expected_failed_ids[i],
              "k=64 benchmark failed block ID mismatch");
        check(cases[i].source_block_ids == expected_sources[i],
              "k=64 benchmark source range mismatch");

        std::vector<unsigned char> coeffs;
        check(ECProject::get_local_fill_plan(64, 3, 3, "AzureLRC",
                                             {cases[i].failed_block_id},
                                             cases[i].source_block_ids, coeffs),
              "k=64 benchmark case is not recoverable: " + cases[i].failed_name);
    }
}

void verify_double_task_recoverable(int k, int r, int z,
                                    const ECProject::RepairBenchmark::RepairTask &task,
                                    const std::string &case_name)
{
    std::vector<unsigned char> coefficients;
    check(ECProject::get_local_fill_plan(k, r, z, "AzureLRC",
                                         task.failed_block_ids,
                                         task.source_block_ids, coefficients),
          "double repair task is not recoverable: " + case_name);
}

void verify_k64_benchmark_double_cases()
{
    using ECProject::RepairBenchmark::DoubleRepairCase;
    using ECProject::RepairBenchmark::RepairTask;

    const auto cases = ECProject::RepairBenchmark::make_azure_double_block_cases(64);
    const std::vector<DoubleRepairCase> expected = {
        {"L0+L1", {RepairTask{{67}, range(0, 21)}, RepairTask{{68}, range(21, 42)}}},
        {"L0+L2", {RepairTask{{67}, range(0, 21)}, RepairTask{{69}, range(42, 64)}}},
        {"G0+G1", {RepairTask{{64, 65}, range(0, 64)}}},
    };

    check(cases.size() == expected.size(),
          "k=64 benchmark double plan must contain exactly three cases");
    if (cases.size() != expected.size())
        return;

    for (size_t case_index = 0; case_index < cases.size(); ++case_index) {
        const auto &actual_case = cases[case_index];
        const auto &expected_case = expected[case_index];
        check(actual_case.failed_name == expected_case.failed_name,
              "k=64 double case name or order mismatch");
        check(actual_case.tasks.size() == expected_case.tasks.size(),
              "k=64 double case task count mismatch: " + expected_case.failed_name);
        if (actual_case.tasks.size() != expected_case.tasks.size())
            continue;
        for (size_t task_index = 0; task_index < actual_case.tasks.size(); ++task_index) {
            const auto &actual_task = actual_case.tasks[task_index];
            const auto &expected_task = expected_case.tasks[task_index];
            check(actual_task.failed_block_ids == expected_task.failed_block_ids,
                  "k=64 double failed IDs mismatch: " + expected_case.failed_name);
            check(actual_task.source_block_ids == expected_task.source_block_ids,
                  "k=64 double sources mismatch: " + expected_case.failed_name);
            verify_double_task_recoverable(64, 3, 3, actual_task,
                                           expected_case.failed_name);
        }
    }

    check(cases[2].tasks.size() == 1,
          "k=64 G0+G1 must use one shared multi-output task");
    if (cases[2].tasks.size() == 1)
        check(cases[2].tasks[0].failed_block_ids == std::vector<int>({64, 65}),
              "k=64 G0+G1 must put G0 first so its owner is the compute proxy");
}

void verify_k64_benchmark_triple_cases()
{
    const auto cases = ECProject::RepairBenchmark::make_azure_k64_triple_block_cases();
    check(cases.size() == 1,
          "k=64 benchmark triple plan must contain exactly one case");
    if (cases.size() != 1)
        return;

    const auto &test_case = cases.front();
    check(test_case.failed_name == "G0+G1+G2",
          "k=64 triple case name mismatch");
    check(test_case.tasks.size() == 1,
          "k=64 G0+G1+G2 must use one shared multi-output task");
    if (test_case.tasks.size() != 1)
        return;

    const auto &task = test_case.tasks.front();
    check(task.failed_block_ids == std::vector<int>({64, 65, 66}),
          "k=64 triple failed IDs must be G0,G1,G2 in compute-owner order");
    check(task.source_block_ids == range(0, 64),
          "k=64 triple sources must be D0-D63 in order");
    std::vector<unsigned char> coefficients;
    check(ECProject::get_local_fill_plan(64, 3, 3, "AzureLRC",
                                         task.failed_block_ids,
                                         task.source_block_ids, coefficients),
          "k=64 G0+G1+G2 shared task is not recoverable");
    check(coefficients.size() == 3U * 64U,
          "k=64 triple recovery matrix must contain three output rows");
}

void verify_k64_benchmark_quadruple_cases()
{
    const auto cases = ECProject::RepairBenchmark::make_azure_k64_quadruple_block_cases();
    check(cases.size() == 1,
          "k=64 benchmark quadruple plan must contain exactly one case");
    if (cases.size() != 1)
        return;

    const auto &test_case = cases.front();
    check(test_case.failed_name == "G0+G1+G2+L0",
          "k=64 quadruple case name mismatch");
    check(test_case.tasks.size() == 1,
          "k=64 G0+G1+G2+L0 must use one shared multi-output task");
    if (test_case.tasks.size() != 1)
        return;

    const auto &task = test_case.tasks.front();
    check(task.failed_block_ids == std::vector<int>({64, 65, 66, 67}),
          "k=64 quadruple failed IDs must be G0,G1,G2,L0 in compute-owner order");
    check(task.source_block_ids == range(0, 64),
          "k=64 quadruple sources must be D0-D63 in order");
    std::vector<unsigned char> coefficients;
    check(ECProject::get_local_fill_plan(64, 3, 3, "AzureLRC",
                                         task.failed_block_ids,
                                         task.source_block_ids, coefficients),
          "k=64 G0+G1+G2+L0 shared task is not recoverable");
    check(coefficients.size() == 4U * 64U,
          "k=64 quadruple recovery matrix must contain four output rows");
}

void verify_larger_azure_double_cases(int k, int r, int z)
{
    const auto cases = ECProject::RepairBenchmark::make_azure_double_block_cases(k);
    check(cases.size() == 1, "Azure k=" + std::to_string(k) +
                             " double plan must contain only G0+G1");
    if (cases.size() != 1)
        return;
    check(cases[0].failed_name == "G0+G1",
          "Azure k=" + std::to_string(k) + " double case name changed");
    check(cases[0].tasks.size() == 1,
          "Azure k=" + std::to_string(k) + " G0+G1 must remain one shared task");
    if (cases[0].tasks.size() != 1)
        return;
    check(cases[0].tasks[0].failed_block_ids == std::vector<int>({k, k + 1}),
          "Azure k=" + std::to_string(k) + " G0+G1 failed IDs changed");
    check(cases[0].tasks[0].source_block_ids == range(0, k),
          "Azure k=" + std::to_string(k) + " G0+G1 sources changed");
    verify_double_task_recoverable(k, r, z, cases[0].tasks[0], "G0+G1");
}

void verify_uneven_benchmark_coverage(int k, int r, int z)
{
    const auto cases = ECProject::RepairBenchmark::make_azure_uneven_single_block_cases(k, r, z);
    check(cases.size() == static_cast<size_t>(z + 1),
          "benchmark plan must retain every local case and G0 for k=" + std::to_string(k));
    if (cases.size() != static_cast<size_t>(z + 1))
        return;
    for (int local = 0; local < z; ++local)
        check(cases[local].failed_name == "L" + std::to_string(local),
              "benchmark local case order changed for k=" + std::to_string(k));
    check(cases.back().failed_name == "G0" && cases.back().failed_block_id == k,
          "benchmark G0 case changed for k=" + std::to_string(k));
}

void verify_legacy_make_matrix(int k, int r, int z, const std::vector<int> &expected_sizes)
{
    std::vector<int> matrix(static_cast<size_t>(r + z) * k, 0);
    check(ECProject::lrc_make_matrix(k, r, z, matrix.data(), ECProject::Azure_LRC),
          "legacy lrc_make_matrix failed for k=" + std::to_string(k));

    const auto bounds = ECProject::get_azure_lrc_local_group_bounds(k, z);
    for (int group = 0; group < z; ++group) {
        check(bounds[group].second - bounds[group].first == expected_sizes[group],
              "legacy matrix group size mismatch for k=" + std::to_string(k));
        for (int data = 0; data < k; ++data) {
            const bool covered = data >= bounds[group].first && data < bounds[group].second;
            check(matrix[static_cast<size_t>(r + group) * k + data] == (covered ? 1 : 0),
                  "legacy Azure local row coverage mismatch for k=" + std::to_string(k));
        }
    }

    std::fill(matrix.begin(), matrix.end(), 0);
    check(ECProject::lrc_make_matrix(k, r, z, matrix.data(), ECProject::Optimal_Cauchy_LRC),
          "Optimal Cauchy lrc_make_matrix failed for k=" + std::to_string(k));
    const int legacy_width = (k + z - 1) / z;
    for (int group = 0; group < z; ++group) {
        for (int data = 0; data < k; ++data) {
            const bool covered = group * legacy_width <= data && data < (group + 1) * legacy_width;
            check(matrix[static_cast<size_t>(r + group) * k + data] == (covered ? 1 : 0),
                  "Optimal Cauchy local row behavior changed for k=" + std::to_string(k));
        }
    }
}

std::unordered_map<int, std::vector<int>> legacy_layout(int k, int r, int z)
{
    std::unordered_map<int, std::vector<int>> result;
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
                result[group].push_back(data++);
            if (last)
                result[group].push_back(k + r + local);
            ++group;
        }
    }
    for (int global = 0; global < r; ++global)
        result[group].push_back(k + global);
    return result;
}

void verify_legacy_layouts()
{
    const std::vector<std::tuple<int, int, int>> cases = {
        {24, 2, 2}, {48, 3, 4}, {72, 4, 4}};
    for (const auto &[k, r, z] : cases)
        check(ECProject::get_azurelrc_group_id_to_block_ids(k, r, z) == legacy_layout(k, r, z),
              "legacy Azure placement changed for k=" + std::to_string(k));
}

} // namespace

int main()
{
    const auto k96_bounds = ECProject::get_azure_lrc_local_group_bounds(96, 4);
    check(ECProject::get_azure_lrc_local_group_sizes(96, 4) ==
              std::vector<int>({24, 24, 24, 24}),
          "k=96 local group sizes must be [24,24,24,24]");
    check(k96_bounds == std::vector<std::pair<int, int>>({
                            {0, 24}, {24, 48}, {48, 72}, {72, 96}}),
          "k=96 local group bounds must cover four consecutive 24-block ranges");
    verify_new_case(96, 3, 4, {24, 24, 24, 24});
    verify_new_case(64, 3, 3, {21, 21, 22});
    verify_new_case(128, 3, 5, {25, 25, 26, 26, 26});
    verify_benchmark_failure_types();
    verify_k64_benchmark_single_cases();
    verify_k64_benchmark_double_cases();
    verify_k64_benchmark_triple_cases();
    verify_k64_benchmark_quadruple_cases();
    verify_larger_azure_double_cases(96, 3, 4);
    verify_larger_azure_double_cases(128, 3, 5);
    verify_uneven_benchmark_coverage(96, 3, 4);
    verify_uneven_benchmark_coverage(128, 3, 5);
    verify_legacy_make_matrix(96, 3, 4, {24, 24, 24, 24});
    verify_legacy_make_matrix(64, 3, 3, {21, 21, 22});
    verify_legacy_make_matrix(128, 3, 5, {25, 25, 26, 26, 26});
    verify_legacy_layouts();
    if (failures == 0)
        std::cout << "Azure uneven-group verification passed\n";
    return failures == 0 ? 0 : 1;
}
