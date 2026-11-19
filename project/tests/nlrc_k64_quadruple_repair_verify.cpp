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
void require(bool condition, const std::string &message)
{
    if (!condition) throw std::runtime_error(message);
}

void verify(int block_size)
{
    constexpr int k = 64, r = 3, z = 5;
    using ECProject::RepairBenchmark::inclusive_range;
    const auto cases = ECProject::RepairBenchmark::make_nlrc_k64_quadruple_block_cases();
    require(cases.size() == 2, "expected exactly two NLRC k64 quadruple cases");
    require(cases[0].failed_name == "L0+L1+L2+L3" && cases[0].tasks.size() == 2,
            "local quadruple must use two shared tasks");
    require(cases[1].failed_name == "G0+G1+G2+L0" && cases[1].tasks.size() == 1,
            "global quadruple must use one shared task");
    const std::vector<std::vector<std::vector<int>>> wanted_failed = {
        {{67, 68}, {69, 70}}, {{64, 65, 66, 67}}};
    const std::vector<std::vector<std::vector<int>>> wanted_sources = {
        {inclusive_range(0, 21), inclusive_range(22, 43)}, {inclusive_range(0, 63)}};

    std::vector<std::vector<unsigned char>> blocks(k + r + z,
        std::vector<unsigned char>(block_size));
    std::mt19937 generator(0x4e4c6434u);
    for (int id = 0; id < k; ++id)
        for (auto &byte : blocks[id]) byte = static_cast<unsigned char>(generator());
    std::vector<unsigned char *> data(k), parity(r + z);
    for (int id = 0; id < k; ++id) data[id] = blocks[id].data();
    for (int id = 0; id < r + z; ++id) parity[id] = blocks[k + id].data();
    ECProject::encode_nlrc(k, r, z, data.data(), parity.data(), block_size);
    const auto original = blocks;
    // No parity buffer remains available to either reconstruction path.
    for (int id = k; id < k + r + z; ++id)
        std::fill(blocks[id].begin(), blocks[id].end(), 0);

    for (size_t case_index = 0; case_index < cases.size(); ++case_index) {
        const auto &test_case = cases[case_index];
        std::unordered_set<int> failed, reads;
        for (size_t index = 0; index < test_case.tasks.size(); ++index) {
            const auto &task = test_case.tasks[index];
            require(task.failed_block_ids == wanted_failed[case_index][index],
                    "wrong output order/owner proxy");
            require(task.source_block_ids == wanted_sources[case_index][index],
                    "wrong source order or range");
            for (int id : task.failed_block_ids)
                require(failed.insert(id).second, "duplicate failed output");
            for (int id : task.source_block_ids)
                require(id < k && reads.insert(id).second, "duplicate or non-data read");

            for (bool reversed : {false, true}) {
                auto sources = task.source_block_ids;
                if (reversed) std::reverse(sources.begin(), sources.end());
                const int columns = static_cast<int>(sources.size());
                const int rows = static_cast<int>(task.failed_block_ids.size());
                std::vector<unsigned char> coefficients;
                require(ECProject::get_local_fill_plan(k, r, z, "NLRC",
                            task.failed_block_ids, sources, coefficients), "coefficient solve failed");
                require(coefficients.size() == static_cast<size_t>(rows * columns),
                        "wrong coefficient matrix size");
                std::vector<std::vector<unsigned char>> recovered(rows,
                    std::vector<unsigned char>(block_size, 0xa5));
                std::vector<unsigned char *> inputs, outputs;
                for (int id : sources) inputs.push_back(blocks[id].data());
                for (auto &output : recovered) outputs.push_back(output.data());
                std::vector<unsigned char> tables(32U * columns * rows);
                ECProject::ec_init_tables(columns, rows, coefficients.data(), tables.data());
                ECProject::ec_encode_data_avx2(block_size, columns, rows, tables.data(),
                                              inputs.data(), outputs.data());
                for (int row = 0; row < rows; ++row) {
                    const auto &expected = original[task.failed_block_ids[row]];
                    require(recovered[row] == expected, "AVX2 repair differs from original parity");
                    std::vector<unsigned char> direct(block_size, 0);
                    for (int col = 0; col < columns; ++col)
                        for (int byte = 0; byte < block_size; ++byte)
                            direct[byte] ^= ECProject::gf_mul(coefficients[row * columns + col],
                                                               blocks[sources[col]][byte]);
                    require(direct == expected, "scalar GF repair differs from original parity");
                }
            }
        }
        require(failed.size() == 4, "case must contain four distinct failures");
        require(reads.size() == (case_index == 0 ? 44U : 64U), "wrong total read count");
        std::cout << "NLRC k64 " << test_case.failed_name << " block_size=" << block_size
                  << ": passed (" << reads.size() << " source blocks)\n";
    }
}
} // namespace

int main()
{
    try {
        verify(4096);
        verify(1048576);
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "NLRC k64 quadruple repair failed: " << error.what() << '\n';
        return 1;
    }
}
