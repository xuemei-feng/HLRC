#include "encoder.h"
#include "repair_benchmark_plan.h"

#include <algorithm>
#include <iostream>
#include <numeric>
#include <random>
#include <set>
#include <string>
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
    std::vector<int> result(std::max(0, end - begin));
    std::iota(result.begin(), result.end(), begin);
    return result;
}

void verify_placement(int k, int r, int z)
{
    const auto groups = ECProject::get_uniform_lrc_group_id_to_block_ids(k, r, z);
    const auto reverse = ECProject::get_uniform_lrc_block_id_to_group_id(k, r, z);
    std::vector<int> seen(k + r + z, 0);
    for (const auto &entry : groups) {
        for (int block : entry.second) {
            check(block >= 0 && block < k + r + z, "placement block out of range");
            if (block >= 0 && block < k + r + z)
                ++seen[block];
            check(reverse.at(block) == entry.first, "placement reverse mapping mismatch");
        }
    }
    check(std::all_of(seen.begin(), seen.end(), [](int count) { return count == 1; }),
          "placement must be a complete bijection");
}

void verify_encoding(int k, int r, int z, const std::vector<unsigned char> &matrix)
{
    constexpr int block_size = 257;
    std::mt19937 generator(0x554c5243U + k);
    std::vector<std::vector<unsigned char>> data(k, std::vector<unsigned char>(block_size));
    std::vector<std::vector<unsigned char>> parity(r + z, std::vector<unsigned char>(block_size));
    std::vector<unsigned char *> data_ptrs(k), parity_ptrs(r + z);
    for (int i = 0; i < k; ++i) {
        std::generate(data[i].begin(), data[i].end(), [&] { return generator() & 0xff; });
        data_ptrs[i] = data[i].data();
    }
    for (int i = 0; i < r + z; ++i)
        parity_ptrs[i] = parity[i].data();
    ECProject::encode_uniform_lrc(k, r, z, data_ptrs.data(), parity_ptrs.data(), block_size);
    for (int row = 0; row < r + z; ++row) {
        for (int byte = 0; byte < block_size; ++byte) {
            unsigned char expected = 0;
            for (int d = 0; d < k; ++d)
                expected ^= ECProject::gf_mul(matrix[static_cast<size_t>(k + row) * k + d], data[d][byte]);
            check(parity[row][byte] == expected, "encoded bytes differ from generator matrix");
        }
    }
}

void verify_case(int k, int r, int z, const std::vector<std::pair<int, int>> &expected_bounds,
                 bool verify_generic_single_plan = true)
{
    const int rows = k + r + z;
    const auto bounds = ECProject::get_uniform_lrc_data_like_group_bounds(k, r, z);
    check(bounds == expected_bounds, "unexpected data-like bounds for k=" + std::to_string(k));
    const auto sizes = ECProject::get_uniform_lrc_local_group_sizes(k, r, z);
    for (int group = 0; group < z; ++group)
        check(sizes[group] == bounds[group].second - bounds[group].first + 1,
              "local group size must include its local parity");

    std::vector<unsigned char> matrix(static_cast<size_t>(rows) * k, 0);
    std::vector<unsigned char> cauchy(static_cast<size_t>(k + r) * k, 0);
    ECProject::gen_uniform_lrc_matrix(matrix.data(), k, r, z);
    ECProject::gf_gen_cauchy_matrix1(cauchy.data(), k + r, k);
    for (int global = 0; global < r; ++global)
        check(std::equal(matrix.begin() + static_cast<size_t>(k + global) * k,
                         matrix.begin() + static_cast<size_t>(k + global + 1) * k,
                         cauchy.begin() + static_cast<size_t>(k + global) * k),
              "global row differs from Uniform Cauchy row");

    std::vector<unsigned char> local_vector(k);
    ECProject::gf_gen_local_vector(local_vector.data(), k, r);
    for (int group = 0; group < z; ++group) {
        std::vector<unsigned char> expected(k, 0);
        for (int block = bounds[group].first; block < bounds[group].second; ++block) {
            check(ECProject::get_uniform_lrc_block_id_to_local_group_id(k, r, z, block) == group,
                  "data-like boundary mapping mismatch");
            if (block < k)
                expected[block] = local_vector[block];
            else
                for (int d = 0; d < k; ++d)
                    expected[d] ^= matrix[static_cast<size_t>(block) * k + d];
        }
        const int local_id = k + r + group;
        check(ECProject::get_uniform_lrc_block_id_to_local_group_id(k, r, z, local_id) == group,
              "local parity mapping mismatch");
        check(std::equal(expected.begin(), expected.end(),
                         matrix.begin() + static_cast<size_t>(local_id) * k),
              "weighted local row mismatch");
    }
    for (int global = 0; global < r; ++global)
        check(ECProject::get_uniform_lrc_block_id_to_local_group_id(k, r, z, k + global) == z - 1,
              "all globals must be in the last local group");

    verify_placement(k, r, z);
    if (verify_generic_single_plan) {
        const auto singles = ECProject::RepairBenchmark::make_uniform_uneven_single_block_cases(k, r, z);
        check(singles.size() == static_cast<size_t>(z + 1),
              "generic single plan must contain every local and G0");
        for (const auto &test_case : singles) {
            std::vector<unsigned char> coefficients;
            check(ECProject::get_local_fill_plan(k, r, z, "UniformLRC", {test_case.failed_block_id},
                                                 test_case.source_block_ids, coefficients),
                  "registered single case is not recoverable: " + test_case.failed_name);
        }
    }
    const auto doubles = ECProject::RepairBenchmark::make_uniform_uneven_double_block_cases(k);
    check(doubles.size() == 1 && doubles[0].tasks.size() == 1, "G0+G1 must use one shared task");
    if (doubles.size() == 1 && doubles[0].tasks.size() == 1) {
        const auto &task = doubles[0].tasks[0];
        check(task.failed_block_ids == std::vector<int>({k, k + 1}), "G0+G1 IDs changed");
        check(task.source_block_ids == range(0, k), "G0+G1 sources must be all data");
        std::vector<unsigned char> coefficients;
        check(ECProject::get_local_fill_plan(k, r, z, "UniformLRC", task.failed_block_ids,
                                             task.source_block_ids, coefficients),
              "G0+G1 shared task is not recoverable");
    }

    const int tail = bounds.back().first;
    std::vector<int> sources;
    int decode_rows = 0, decode_cols = 0;
    check(ECProject::get_global_decode_plan(k, r, z, "UniformLRC", {tail}, sources,
                                            nullptr, nullptr, decode_rows, decode_cols),
          "tail-boundary automatic recovery is not solvable");
    verify_encoding(k, r, z, matrix);
}

void verify_k64_single_plan()
{
    const auto cases = ECProject::RepairBenchmark::make_uniform_k64_single_block_cases();
    const std::vector<ECProject::RepairBenchmark::RepairCase> expected = {
        {"L0", 67, range(0, 22)},
        {"L2", 69, range(44, 67)},
    };
    check(cases.size() == expected.size(), "k64 single plan must contain exactly L0 and L2");
    if (cases.size() != expected.size())
        return;
    for (size_t i = 0; i < cases.size(); ++i) {
        check(cases[i].failed_name == expected[i].failed_name, "k64 single case name mismatch");
        check(cases[i].failed_block_id == expected[i].failed_block_id, "k64 single failed ID mismatch");
        check(cases[i].source_block_ids == expected[i].source_block_ids, "k64 single sources mismatch");
        std::vector<unsigned char> coefficients;
        check(ECProject::get_local_fill_plan(64, 3, 3, "UniformLRC", {cases[i].failed_block_id},
                                             cases[i].source_block_ids, coefficients),
              "k64 single case is not recoverable: " + cases[i].failed_name);
    }
}

void verify_k96_single_plan()
{
    const auto cases = ECProject::RepairBenchmark::make_uniform_k96_single_block_cases();
    const std::vector<ECProject::RepairBenchmark::RepairCase> expected = {
        {"L0", 99, range(0, 24)},
        {"L1", 100, range(24, 49)},
    };
    check(cases.size() == expected.size(), "k96 single plan must contain exactly L0 and L1");
    for (size_t i = 0; i < std::min(cases.size(), expected.size()); ++i) {
        check(cases[i].failed_name == expected[i].failed_name, "k96 single case name mismatch");
        check(cases[i].failed_block_id == expected[i].failed_block_id, "k96 single failed ID mismatch");
        check(cases[i].source_block_ids == expected[i].source_block_ids, "k96 single sources mismatch");
        std::vector<unsigned char> coefficients;
        check(ECProject::get_local_fill_plan(96, 3, 4, "UniformLRC", {cases[i].failed_block_id},
                                             cases[i].source_block_ids, coefficients),
              "k96 single case is not recoverable: " + cases[i].failed_name);
    }
}

void verify_k128_single_plan()
{
    auto l4_sources = range(104, 131);
    const auto cases = ECProject::RepairBenchmark::make_uniform_k128_single_block_cases();
    const std::vector<ECProject::RepairBenchmark::RepairCase> expected = {
        {"L0", 131, range(0,26)}, {"L4", 135, l4_sources},
    };
    check(cases.size() == expected.size(), "k128 single plan must contain exactly L0 and L4");
    for (size_t i = 0; i < std::min(cases.size(), expected.size()); ++i) {
        check(cases[i].failed_name == expected[i].failed_name, "k128 single case name mismatch");
        check(cases[i].failed_block_id == expected[i].failed_block_id, "k128 single failed ID mismatch");
        check(cases[i].source_block_ids == expected[i].source_block_ids, "k128 single sources mismatch");
        std::vector<unsigned char> coefficients;
        check(ECProject::get_local_fill_plan(128,3,5,"UniformLRC",{cases[i].failed_block_id},cases[i].source_block_ids,coefficients), "k128 single case is not recoverable");
    }
}

void verify_k64_double_plan()
{
    using ECProject::RepairBenchmark::DoubleRepairCase;
    using ECProject::RepairBenchmark::RepairTask;
    const auto cases = ECProject::RepairBenchmark::make_uniform_k64_double_block_cases();
    const std::vector<DoubleRepairCase> expected = {
        {"L0+L1", {RepairTask{{67}, range(0, 22)}, RepairTask{{68}, range(22, 44)}}},
        {"L0+L2", {RepairTask{{67}, range(0, 22)}, RepairTask{{69}, range(44, 67)}}},
        {"G0+G1", {RepairTask{{64, 65}, range(0, 64)}}},
    };
    check(cases.size() == expected.size(), "k64 double plan must contain exactly three cases");
    if (cases.size() != expected.size())
        return;
    for (size_t case_index = 0; case_index < cases.size(); ++case_index) {
        check(cases[case_index].failed_name == expected[case_index].failed_name,
              "k64 double case name mismatch");
        check(cases[case_index].tasks.size() == expected[case_index].tasks.size(),
              "k64 double task count mismatch");
        check(!cases[case_index].optimal_g0_d0_chain,
              "Uniform k64 double cases must use the generic custom multi-recovery path");
        if (cases[case_index].tasks.size() != expected[case_index].tasks.size())
            continue;
        for (size_t task_index = 0; task_index < cases[case_index].tasks.size(); ++task_index) {
            const auto &actual = cases[case_index].tasks[task_index];
            const auto &wanted = expected[case_index].tasks[task_index];
            check(actual.failed_block_ids == wanted.failed_block_ids, "k64 double failed IDs mismatch");
            check(actual.source_block_ids == wanted.source_block_ids, "k64 double sources mismatch");
            std::vector<unsigned char> coefficients;
            check(ECProject::get_local_fill_plan(64, 3, 3, "UniformLRC", actual.failed_block_ids,
                                                 actual.source_block_ids, coefficients),
                  "k64 double task is not recoverable: " + cases[case_index].failed_name);
        }
    }
    check(cases[2].tasks.size() == 1 &&
              cases[2].tasks[0].failed_block_ids == std::vector<int>({64, 65}),
          "k64 G0+G1 must remain one shared multi-output task with G0 first");
}

void verify_k96_double_plan()
{
    using ECProject::RepairBenchmark::DoubleRepairCase;
    using ECProject::RepairBenchmark::RepairTask;
    const auto cases = ECProject::RepairBenchmark::make_uniform_k96_double_block_cases();
    const std::vector<DoubleRepairCase> expected = {
        {"L0+L1", {RepairTask{{99}, range(0,24)}, RepairTask{{100}, range(24,49)}}},
        {"L1+L2", {RepairTask{{100}, range(24,49)}, RepairTask{{101}, range(49,74)}}},
        {"G0+G1", {RepairTask{{96,97}, range(0,96)}}},
    };
    check(cases.size() == expected.size(), "k96 double plan must contain three cases");
    for (size_t c = 0; c < std::min(cases.size(), expected.size()); ++c) {
        check(cases[c].failed_name == expected[c].failed_name, "k96 double case name mismatch");
        check(cases[c].tasks.size() == expected[c].tasks.size(), "k96 double task count mismatch");
        for (size_t t = 0; t < std::min(cases[c].tasks.size(), expected[c].tasks.size()); ++t) {
            check(cases[c].tasks[t].failed_block_ids == expected[c].tasks[t].failed_block_ids, "k96 double failed IDs mismatch");
            check(cases[c].tasks[t].source_block_ids == expected[c].tasks[t].source_block_ids, "k96 double sources mismatch");
            std::vector<unsigned char> coefficients;
            check(ECProject::get_local_fill_plan(96,3,4,"UniformLRC",cases[c].tasks[t].failed_block_ids,cases[c].tasks[t].source_block_ids,coefficients), "k96 double task is not recoverable");
        }
    }
}

void verify_k128_double_plan()
{
    using ECProject::RepairBenchmark::DoubleRepairCase;
    using ECProject::RepairBenchmark::RepairTask;
    const auto cases = ECProject::RepairBenchmark::make_uniform_k128_double_block_cases();
    const std::vector<DoubleRepairCase> expected = {
        {"L0+L1", {RepairTask{{131},range(0,26)},RepairTask{{132},range(26,52)}}},
        {"L0+L4", {RepairTask{{131},range(0,26)},RepairTask{{135},range(104,131)}}},
        {"G0+G1", {RepairTask{{128,129},range(0,128)}}},
    };
    check(cases.size() == expected.size(), "k128 double plan must contain three cases");
    for (size_t c=0;c<std::min(cases.size(),expected.size());++c) {
        check(cases[c].failed_name==expected[c].failed_name,"k128 double case name mismatch");
        check(cases[c].tasks.size()==expected[c].tasks.size(),"k128 double task count mismatch");
        for(size_t t=0;t<std::min(cases[c].tasks.size(),expected[c].tasks.size());++t){
            check(cases[c].tasks[t].failed_block_ids==expected[c].tasks[t].failed_block_ids,"k128 double failed IDs mismatch");
            check(cases[c].tasks[t].source_block_ids==expected[c].tasks[t].source_block_ids,"k128 double sources mismatch");
            std::vector<unsigned char> coefficients;
            check(ECProject::get_local_fill_plan(128,3,5,"UniformLRC",cases[c].tasks[t].failed_block_ids,cases[c].tasks[t].source_block_ids,coefficients),"k128 double task is not recoverable");
        }
    }
}

void verify_k64_triple_plan()
{
    const auto cases = ECProject::RepairBenchmark::make_uniform_k64_triple_block_cases();
    check(cases.size() == 1, "k64 triple plan must contain exactly G0+G1+G2");
    if (cases.size() != 1)
        return;
    const auto &test_case = cases.front();
    check(test_case.failed_name == "G0+G1+G2", "k64 triple case name mismatch");
    check(test_case.tasks.size() == 1, "k64 triple case must use one shared task");
    check(!test_case.optimal_g0_d0_chain,
          "Uniform k64 triple case must use the generic custom multi-recovery path");
    if (test_case.tasks.size() != 1)
        return;
    const auto &task = test_case.tasks.front();
    check(task.failed_block_ids == std::vector<int>({64, 65, 66}),
          "k64 triple failed IDs must keep G0 first");
    check(task.source_block_ids == range(0, 64),
          "k64 triple sources must be all data blocks");
    std::vector<unsigned char> coefficients;
    check(ECProject::get_local_fill_plan(64, 3, 3, "UniformLRC", task.failed_block_ids,
                                         task.source_block_ids, coefficients),
          "k64 triple shared task is not recoverable");
    check(coefficients.size() == 3U * 64U,
          "k64 triple recovery matrix must contain three output rows");
}

void verify_k96_triple_plan()
{
    using ECProject::RepairBenchmark::RepairTask;
    using ECProject::RepairBenchmark::TripleRepairCase;
    auto l3_sources = range(74, 99);
    const auto cases = ECProject::RepairBenchmark::make_uniform_k96_triple_block_cases();
    const std::vector<TripleRepairCase> expected = {
        {"L0+L1+L2", {RepairTask{{99},range(0,24)},RepairTask{{100},range(24,49)},RepairTask{{101},range(49,74)}}},
        {"L1+L2+L3", {RepairTask{{100},range(24,49)},RepairTask{{101},range(49,74)},RepairTask{{102},l3_sources}}},
        {"G0+G1+G2", {RepairTask{{96,97,98},range(0,96)}}},
    };
    check(cases.size() == expected.size(), "k96 triple plan must contain three cases");
    for (size_t c = 0; c < std::min(cases.size(), expected.size()); ++c) {
        check(cases[c].failed_name == expected[c].failed_name, "k96 triple case name mismatch");
        check(cases[c].tasks.size() == expected[c].tasks.size(), "k96 triple task count mismatch");
        for (size_t t = 0; t < std::min(cases[c].tasks.size(), expected[c].tasks.size()); ++t) {
            check(cases[c].tasks[t].failed_block_ids == expected[c].tasks[t].failed_block_ids, "k96 triple failed IDs mismatch");
            check(cases[c].tasks[t].source_block_ids == expected[c].tasks[t].source_block_ids, "k96 triple sources mismatch");
            std::vector<unsigned char> coefficients;
            check(ECProject::get_local_fill_plan(96,3,4,"UniformLRC",cases[c].tasks[t].failed_block_ids,cases[c].tasks[t].source_block_ids,coefficients), "k96 triple task is not recoverable");
        }
    }
}

void verify_k128_triple_plan()
{
    using ECProject::RepairBenchmark::RepairTask;
    using ECProject::RepairBenchmark::TripleRepairCase;
    const auto cases=ECProject::RepairBenchmark::make_uniform_k128_triple_block_cases();
    const std::vector<TripleRepairCase> expected={
        {"L0+L1+L2",{RepairTask{{131},range(0,26)},RepairTask{{132},range(26,52)},RepairTask{{133},range(52,78)}}},
        {"L0+L1+L4",{RepairTask{{131},range(0,26)},RepairTask{{132},range(26,52)},RepairTask{{135},range(104,131)}}},
        {"G0+G1+G2",{RepairTask{{128,129,130},range(0,128)}}},
    };
    check(cases.size()==expected.size(),"k128 triple plan must contain three cases");
    for(size_t c=0;c<std::min(cases.size(),expected.size());++c){
        check(cases[c].failed_name==expected[c].failed_name,"k128 triple case name mismatch");
        check(cases[c].tasks.size()==expected[c].tasks.size(),"k128 triple task count mismatch");
        for(size_t t=0;t<std::min(cases[c].tasks.size(),expected[c].tasks.size());++t){
            check(cases[c].tasks[t].failed_block_ids==expected[c].tasks[t].failed_block_ids,"k128 triple failed IDs mismatch");
            check(cases[c].tasks[t].source_block_ids==expected[c].tasks[t].source_block_ids,"k128 triple sources mismatch");
            std::vector<unsigned char> coefficients;check(ECProject::get_local_fill_plan(128,3,5,"UniformLRC",cases[c].tasks[t].failed_block_ids,cases[c].tasks[t].source_block_ids,coefficients),"k128 triple task is not recoverable");
        }
    }
}

void verify_k64_quadruple_plan()
{
    const auto cases = ECProject::RepairBenchmark::make_uniform_k64_quadruple_block_cases();
    check(cases.size() == 1, "k64 quadruple plan must contain exactly G0+G1+G2+L0");
    if (cases.size() != 1)
        return;
    const auto &test_case = cases.front();
    check(test_case.failed_name == "G0+G1+G2+L0", "k64 quadruple case name mismatch");
    check(test_case.tasks.size() == 1, "k64 quadruple case must use one shared task");
    check(!test_case.optimal_g0_d0_chain,
          "Uniform k64 quadruple case must use the generic custom multi-recovery path");
    if (test_case.tasks.size() != 1)
        return;
    const auto &task = test_case.tasks.front();
    check(task.failed_block_ids == std::vector<int>({64, 65, 66, 67}),
          "k64 quadruple failed IDs must keep G0 first");
    check(task.source_block_ids == range(0, 64),
          "k64 quadruple sources must be all data blocks");
    std::vector<unsigned char> coefficients;
    check(ECProject::get_local_fill_plan(64, 3, 3, "UniformLRC", task.failed_block_ids,
                                         task.source_block_ids, coefficients),
          "k64 quadruple shared task is not recoverable");
    check(coefficients.size() == 4U * 64U,
          "k64 quadruple recovery matrix must contain four output rows");
}

void verify_k96_quadruple_plan()
{
    const auto cases = ECProject::RepairBenchmark::make_uniform_k96_quadruple_block_cases();
    check(cases.size() == 1, "k96 quadruple plan must contain exactly one case");
    if (cases.size() != 1 || cases[0].tasks.size() != 1)
        return;
    check(cases[0].failed_name == "G0+G1+G2+L0", "k96 quadruple case name mismatch");
    const auto &task = cases[0].tasks[0];
    check(task.failed_block_ids == std::vector<int>({96,97,98,99}), "k96 quadruple failed order mismatch");
    check(task.source_block_ids == range(0,96), "k96 quadruple sources mismatch");
    std::vector<unsigned char> coefficients;
    check(ECProject::get_local_fill_plan(96,3,4,"UniformLRC",task.failed_block_ids,task.source_block_ids,coefficients), "k96 quadruple task is not recoverable");
    check(coefficients.size() == 4U * 96U, "k96 quadruple matrix size mismatch");
}

void verify_k128_quadruple_plan()
{
    using ECProject::RepairBenchmark::QuadrupleRepairCase;using ECProject::RepairBenchmark::RepairTask;
    const auto cases=ECProject::RepairBenchmark::make_uniform_k128_quadruple_block_cases();
    const std::vector<QuadrupleRepairCase> expected={
        {"L0+L1+L2+L3",{RepairTask{{131},range(0,26)},RepairTask{{132},range(26,52)},RepairTask{{133},range(52,78)},RepairTask{{134},range(78,104)}}},
        {"L0+L1+L2+L4",{RepairTask{{131},range(0,26)},RepairTask{{132},range(26,52)},RepairTask{{133},range(52,78)},RepairTask{{135},range(104,131)}}},
        {"G0+G1+G2+L4",{RepairTask{{128,129,130,135},range(0,128)}}},
    };
    check(cases.size()==expected.size(),"k128 quadruple plan must contain three cases");
    for(size_t c=0;c<std::min(cases.size(),expected.size());++c){
        check(cases[c].failed_name==expected[c].failed_name,"k128 quadruple case name mismatch");check(cases[c].tasks.size()==expected[c].tasks.size(),"k128 quadruple task count mismatch");
        for(size_t t=0;t<std::min(cases[c].tasks.size(),expected[c].tasks.size());++t){check(cases[c].tasks[t].failed_block_ids==expected[c].tasks[t].failed_block_ids,"k128 quadruple failed IDs mismatch");check(cases[c].tasks[t].source_block_ids==expected[c].tasks[t].source_block_ids,"k128 quadruple sources mismatch");std::vector<unsigned char> coefficients;check(ECProject::get_local_fill_plan(128,3,5,"UniformLRC",cases[c].tasks[t].failed_block_ids,cases[c].tasks[t].source_block_ids,coefficients),"k128 quadruple task is not recoverable");}
    }
}

} // namespace

int main()
{
    verify_case(64, 3, 3, {{0, 22}, {22, 44}, {44, 67}}, false);
    verify_k64_single_plan();
    verify_k64_double_plan();
    verify_k64_triple_plan();
    verify_k64_quadruple_plan();
    verify_case(96, 3, 4, {{0, 24}, {24, 49}, {49, 74}, {74, 99}});
    verify_k96_single_plan();
    verify_k96_double_plan();
    verify_k96_triple_plan();
    verify_k96_quadruple_plan();
    verify_case(128, 3, 5, {{0, 26}, {26, 52}, {52, 78}, {78, 104}, {104, 131}});
    verify_k128_single_plan();
    verify_k128_double_plan();
    verify_k128_triple_plan();
    verify_k128_quadruple_plan();
    for (const auto &legacy : std::vector<std::tuple<int, int, int>>{{24, 2, 2}, {48, 3, 4}, {72, 4, 4}})
        verify_placement(std::get<0>(legacy), std::get<1>(legacy), std::get<2>(legacy));
    if (failures == 0)
        std::cout << "UniformLRC uneven verification passed\n";
    return failures == 0 ? 0 : 1;
}
