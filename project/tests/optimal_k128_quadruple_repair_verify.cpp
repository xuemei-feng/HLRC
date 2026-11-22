#include "encoder.h"
#include "repair_benchmark_plan.h"

#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace {
using ECProject::RepairBenchmark::QuadrupleRepairCase;
using ECProject::RepairBenchmark::RepairTask;

void require(bool condition, const std::string &message)
{
    if (!condition) throw std::runtime_error(message);
}

std::vector<int> range(int first, int last)
{
    return ECProject::RepairBenchmark::inclusive_range(first, last);
}

std::vector<int> with_globals(std::vector<int> sources)
{
    sources.insert(sources.end(), {128, 129, 130});
    return sources;
}

void check_task(const RepairTask &task, const std::vector<int> &failed,
                const std::vector<int> &sources, int owner, const std::string &name)
{
    require(task.failed_block_ids == failed, name + " failed IDs");
    require(task.source_block_ids == sources, name + " sources");
    require(task.failed_block_ids.front() == owner, name + " owner");
}

void verify_cases(const std::vector<QuadrupleRepairCase> &cases)
{
    require(cases.size() == 3, "case count");
    require(cases[0].failed_name == "L0+L1+L2+L3", "first case name");
    require(cases[0].tasks.size() == 1, "first case shared task");
    check_task(cases[0].tasks[0], {131, 132, 133, 134},
               with_globals(range(0, 101)), 131, cases[0].failed_name);

    require(cases[1].failed_name == "L1+L2+L3+L4", "second case name");
    require(cases[1].tasks.size() == 1, "second case shared task");
    check_task(cases[1].tasks[0], {132, 133, 134, 135},
               with_globals(range(25, 127)), 132, cases[1].failed_name);

    require(cases[2].failed_name == "G0+G1+G2+L0", "third case name");
    require(cases[2].tasks.size() == 1, "third case shared task");
    check_task(cases[2].tasks[0], {128, 129, 130, 131}, range(0, 127), 128,
               cases[2].failed_name);

    for (const auto &test_case : cases)
        require(!test_case.sum_task_times, test_case.failed_name + " shared wall time");
}
} // namespace

int main()
{
    try {
        constexpr int k = 128, r = 3, z = 5, block_size = 4096;
        const auto cases = ECProject::RepairBenchmark::make_optimal_quadruple_block_cases(k);
        verify_cases(cases);

        std::vector<std::vector<unsigned char>> blocks(k + r + z,
            std::vector<unsigned char>(block_size));
        std::mt19937 generator(0x4f503854u);
        std::uniform_int_distribution<int> distribution(0, 255);
        for (int id = 0; id < k; ++id)
            for (auto &value : blocks[id])
                value = static_cast<unsigned char>(distribution(generator));
        std::vector<unsigned char *> data(k), parity(r + z);
        for (int id = 0; id < k; ++id) data[id] = blocks[id].data();
        for (int id = 0; id < r + z; ++id) parity[id] = blocks[k + id].data();
        ECProject::encode_optimal_lrc(k, r, z, data.data(), parity.data(), block_size);

        for (const auto &test_case : cases) {
            const auto &task = test_case.tasks[0];
            std::unordered_set<int> failed(task.failed_block_ids.begin(),
                                           task.failed_block_ids.end());
            for (int source : task.source_block_ids)
                require(!failed.count(source), test_case.failed_name + " reads failure");

            std::vector<unsigned char> coefficients;
            require(ECProject::get_local_fill_plan(k, r, z, "OptimalLRC",
                        task.failed_block_ids, task.source_block_ids, coefficients),
                    test_case.failed_name + " solve");
            const size_t rows = task.failed_block_ids.size();
            const size_t columns = task.source_block_ids.size();
            require(rows == 4 && coefficients.size() == rows * columns,
                    test_case.failed_name + " matrix size");

            std::vector<std::vector<unsigned char>> recovered(rows,
                std::vector<unsigned char>(block_size));
            for (size_t output = 0; output < rows; ++output) {
                for (size_t column = 0; column < columns; ++column)
                    for (int byte = 0; byte < block_size; ++byte)
                        recovered[output][byte] ^= ECProject::gf_mul(
                            coefficients[output * columns + column],
                            blocks[task.source_block_ids[column]][byte]);
                require(recovered[output] == blocks[task.failed_block_ids[output]],
                        test_case.failed_name + " output bytes");
                if (output == 0)
                    require(task.failed_block_ids[output] == task.failed_block_ids.front(),
                            test_case.failed_name + " output0 owner-local write");
                else
                    require(task.failed_block_ids[output] != task.failed_block_ids.front(),
                            test_case.failed_name + " output1-3 remote forwarding");
            }
        }
        std::cout << "OptimalLRC k128 quadruple repair verification passed" << std::endl;
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "OptimalLRC k128 quadruple repair verification failed: "
                  << error.what() << std::endl;
        return 1;
    }
}
