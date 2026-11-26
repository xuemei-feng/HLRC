#include "encoder.h"
#include "repair_benchmark_plan.h"

#include <algorithm>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace {
void require(bool condition, const std::string &message) { if (!condition) throw std::runtime_error(message); }

void verify(int block_size)
{
    constexpr int k = 96, r = 3, z = 7;
    using ECProject::RepairBenchmark::inclusive_range;
    const auto cases = ECProject::RepairBenchmark::make_alrc_k96_quadruple_block_cases();
    require(cases.size() == 3, "expected exactly three ALRC k96 quadruple cases");
    const std::vector<std::string> names = {"L0+L1+L2+L3", "L0+L1+L2+L4", "G0+G1+G2+L0"};
    const std::vector<size_t> task_counts = {2, 3, 1};
    for (size_t i = 0; i < cases.size(); ++i) {
        require(cases[i].failed_name == names[i], "case name/order mismatch");
        require(cases[i].tasks.size() == task_counts[i], "task count mismatch");
        require(cases[i].sum_task_times == (i < 2), "timing policy mismatch");
    }
    const std::vector<std::vector<std::vector<int>>> wanted_failed = {
        {{99, 100}, {101, 102}}, {{99, 100}, {101}, {103}}, {{96, 97, 98, 99}}};
    const std::vector<std::vector<std::vector<int>>> wanted_sources = {
        {inclusive_range(0, 23), inclusive_range(24, 47)},
        {inclusive_range(0, 23), inclusive_range(24, 47), inclusive_range(48, 71)},
        {inclusive_range(0, 95)}};

    std::vector<std::vector<unsigned char>> blocks(k + r + z, std::vector<unsigned char>(block_size));
    std::mt19937 generator(0x414c5136u);
    for (int id = 0; id < k; ++id) for (auto &byte : blocks[id]) byte = static_cast<unsigned char>(generator());
    std::vector<unsigned char *> data(k), parity(r + z);
    for (int id = 0; id < k; ++id) data[id] = blocks[id].data();
    for (int id = 0; id < r + z; ++id) parity[id] = blocks[k + id].data();
    ECProject::encode_alrc(k, r, z, data.data(), parity.data(), block_size);
    const auto original = blocks;
    for (int id = k; id < k + r + z; ++id) std::fill(blocks[id].begin(), blocks[id].end(), 0);

    for (size_t case_index = 0; case_index < cases.size(); ++case_index) {
        std::unordered_set<int> failed, reads;
        for (size_t task_index = 0; task_index < cases[case_index].tasks.size(); ++task_index) {
            const auto &task = cases[case_index].tasks[task_index];
            require(task.failed_block_ids == wanted_failed[case_index][task_index], "wrong output order/owner");
            require(task.source_block_ids == wanted_sources[case_index][task_index], "wrong source range");
            for (int id : task.failed_block_ids) require(failed.insert(id).second, "duplicate failed output");
            for (int id : task.source_block_ids) require(id < k && reads.insert(id).second, "duplicate/non-data read");

            for (bool reversed : {false, true}) {
                auto sources = task.source_block_ids;
                if (reversed) std::reverse(sources.begin(), sources.end());
                const int columns = static_cast<int>(sources.size());
                const int rows = static_cast<int>(task.failed_block_ids.size());
                std::vector<unsigned char> coefficients;
                require(ECProject::get_local_fill_plan(k, r, z, "ALRC", task.failed_block_ids, sources, coefficients), "coefficient solve failed");
                require(coefficients.size() == static_cast<size_t>(rows * columns), "coefficient size mismatch");
                std::vector<std::vector<unsigned char>> recovered(rows, std::vector<unsigned char>(block_size));
                std::vector<unsigned char *> inputs, outputs;
                for (int id : sources) inputs.push_back(blocks[id].data());
                for (auto &output : recovered) outputs.push_back(output.data());
                std::vector<unsigned char> tables(32U * columns * rows);
                ECProject::ec_init_tables(columns, rows, coefficients.data(), tables.data());
                ECProject::ec_encode_data_avx2(block_size, columns, rows, tables.data(), inputs.data(), outputs.data());
                for (int row = 0; row < rows; ++row) {
                    const auto &expected = original[task.failed_block_ids[row]];
                    require(recovered[row] == expected, "AVX2 recovery mismatch");
                    std::vector<unsigned char> direct(block_size, 0);
                    for (int col = 0; col < columns; ++col)
                        for (int byte = 0; byte < block_size; ++byte)
                            direct[byte] ^= ECProject::gf_mul(coefficients[row * columns + col], blocks[sources[col]][byte]);
                    require(direct == expected, "scalar GF recovery mismatch");
                }
            }
        }
        require(failed.size() == 4, "case must contain four failures");
        const size_t expected_reads = case_index == 0 ? 48U : case_index == 1 ? 72U : 96U;
        require(reads.size() == expected_reads, "wrong total read count");
    }
}
}

int main()
{
    try { verify(4096); verify(1048576); std::cout << "ALRC k96 quadruple repair verification passed\n"; return 0; }
    catch (const std::exception &error) { std::cerr << "ALRC k96 quadruple repair failed: " << error.what() << '\n'; return 1; }
}
