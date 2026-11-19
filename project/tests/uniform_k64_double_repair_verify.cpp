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
constexpr int k = 64, r = 3, z = 3;

void require(bool condition, const std::string &message)
{
    if (!condition) throw std::runtime_error(message);
}

void verify(int block_size)
{
    using ECProject::RepairBenchmark::DoubleRepairCase;
    using ECProject::RepairBenchmark::inclusive_range;
    const auto cases = ECProject::RepairBenchmark::make_uniform_k64_double_block_cases();
    const std::vector<DoubleRepairCase> expected = {
        {"L0+L1", {{{67}, inclusive_range(0, 21)},
                     {{68}, inclusive_range(22, 43)}}, false, true},
        {"L0+L2", {{{67}, inclusive_range(0, 21)},
                     {{69}, inclusive_range(44, 66)}}, false, true},
        {"G0+G1", {{{64, 65}, inclusive_range(0, 63)}}},
    };
    require(cases.size() == expected.size(), "wrong double repair case count");

    std::vector<std::vector<unsigned char>> blocks(k + r + z,
        std::vector<unsigned char>(static_cast<size_t>(block_size)));
    std::mt19937 generator(0x554e6432u + static_cast<unsigned>(block_size));
    for (int id = 0; id < k; ++id)
        for (auto &byte : blocks[id]) byte = static_cast<unsigned char>(generator());
    std::vector<unsigned char *> data(k), parity(r + z);
    for (int id = 0; id < k; ++id) data[id] = blocks[id].data();
    for (int id = 0; id < r + z; ++id) parity[id] = blocks[k + id].data();
    ECProject::encode_uniform_lrc(k, r, z, data.data(), parity.data(), block_size);
    const auto original = blocks;

    for (size_t case_index = 0; case_index < cases.size(); ++case_index) {
        const auto &test = cases[case_index];
        const auto &wanted = expected[case_index];
        require(test.failed_name == wanted.failed_name && test.tasks.size() == wanted.tasks.size(),
                "wrong double repair case/order");
        require(test.sum_task_times == wanted.sum_task_times && !test.optimal_g0_d0_chain,
                test.failed_name + ": wrong timing/execution policy");

        std::unordered_set<int> all_failed;
        for (const auto &task : test.tasks)
            all_failed.insert(task.failed_block_ids.begin(), task.failed_block_ids.end());
        require(all_failed.size() == 2, test.failed_name + ": must contain two failures");
        for (int id : all_failed) std::fill(blocks[id].begin(), blocks[id].end(), 0);

        for (size_t task_index = 0; task_index < test.tasks.size(); ++task_index) {
            const auto &task = test.tasks[task_index];
            const auto &wanted_task = wanted.tasks[task_index];
            require(task.failed_block_ids == wanted_task.failed_block_ids &&
                        task.source_block_ids == wanted_task.source_block_ids,
                    test.failed_name + ": wrong sources, outputs or owner order");
            for (int source : task.source_block_ids)
                require(!all_failed.count(source), test.failed_name + ": reads a failed block");

            std::vector<unsigned char> coefficients;
            require(ECProject::get_local_fill_plan(k, r, z, "UniformLRC",
                                                  task.failed_block_ids,
                                                  task.source_block_ids, coefficients),
                    test.failed_name + ": coefficient solve failed");
            const size_t columns = task.source_block_ids.size();
            require(coefficients.size() == columns * task.failed_block_ids.size(),
                    test.failed_name + ": coefficient matrix dimensions mismatch");
            for (size_t row = 0; row < task.failed_block_ids.size(); ++row) {
                std::vector<unsigned char> recovered(static_cast<size_t>(block_size), 0);
                for (size_t column = 0; column < columns; ++column)
                    for (int byte = 0; byte < block_size; ++byte)
                        recovered[static_cast<size_t>(byte)] ^= ECProject::gf_mul(
                            coefficients[row * columns + column],
                            blocks[task.source_block_ids[column]][static_cast<size_t>(byte)]);
                const int id = task.failed_block_ids[row];
                require(recovered == original[id],
                        test.failed_name + ": reconstructed bytes mismatch");
                blocks[id] = std::move(recovered);
            }
        }
        require(blocks == original, test.failed_name + ": stripe differs after repair");
        std::cout << test.failed_name << " block_size=" << block_size << ": passed\n";
    }
}
} // namespace

int main()
{
    try {
        verify(4096);
        verify(1048576);
        std::cout << "UniformLRC k64 double repair verification passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "UniformLRC k64 double repair verification failed: "
                  << error.what() << '\n';
        return 1;
    }
}
