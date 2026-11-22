#include "encoder.h"
#include "repair_benchmark_plan.h"

#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace {
using ECProject::RepairBenchmark::RepairTask;
using ECProject::RepairBenchmark::TripleRepairCase;

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

void verify_task(const RepairTask &task, const std::vector<int> &failed,
                 const std::vector<int> &sources, int owner,
                 const std::string &name)
{
    require(task.failed_block_ids == failed, name + " failed IDs");
    require(task.source_block_ids == sources, name + " sources");
    require(task.failed_block_ids.front() == owner, name + " request owner");
}

void verify_cases(const std::vector<TripleRepairCase> &cases)
{
    require(cases.size() == 4, "case count");
    const std::vector<std::string> names = {
        "L0+L1+L2", "L1+L2+L3", "L2+L3+L4", "G0+G1+G2"};
    const std::vector<std::vector<int>> failed = {
        {131, 132, 133}, {132, 133, 134}, {133, 134, 135}, {128, 129, 130}};
    const std::vector<std::vector<int>> sources = {
        with_globals(range(0, 75)), with_globals(range(25, 101)),
        with_globals(range(50, 127)), range(0, 127)};
    const std::vector<int> owners = {131, 132, 133, 128};
    for (size_t index = 0; index < cases.size(); ++index) {
        require(cases[index].failed_name == names[index], names[index] + " name");
        require(cases[index].tasks.size() == 1, names[index] + " shared task count");
        require(!cases[index].sum_task_times, names[index] + " wall timing");
        verify_task(cases[index].tasks[0], failed[index], sources[index], owners[index],
                    names[index]);
    }
}
} // namespace

int main()
{
    try {
        constexpr int k = 128, r = 3, z = 5, block_size = 4096;
        const auto cases = ECProject::RepairBenchmark::make_optimal_triple_block_cases(k);
        verify_cases(cases);

        std::vector<std::vector<unsigned char>> blocks(
            k + r + z, std::vector<unsigned char>(block_size));
        std::mt19937 generator(0x4f503854u);
        std::uniform_int_distribution<int> distribution(0, 255);
        for (int id = 0; id < k; ++id)
            for (auto &byte : blocks[id])
                byte = static_cast<unsigned char>(distribution(generator));
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
            require(rows == 3 && coefficients.size() == rows * columns,
                    test_case.failed_name + " matrix size");
            std::vector<std::vector<unsigned char>> outputs(
                rows, std::vector<unsigned char>(block_size));
            for (size_t output = 0; output < rows; ++output) {
                for (size_t source = 0; source < columns; ++source)
                    for (int byte = 0; byte < block_size; ++byte)
                        outputs[output][byte] ^= ECProject::gf_mul(
                            coefficients[output * columns + source],
                            blocks[task.source_block_ids[source]][byte]);
                require(outputs[output] == blocks[task.failed_block_ids[output]],
                        test_case.failed_name + " output bytes");
            }
            require(outputs[0] == blocks[task.failed_block_ids[0]],
                    test_case.failed_name + " outputs[0] local owner result");
            require(outputs[1] == blocks[task.failed_block_ids[1]],
                    test_case.failed_name + " outputs[1] first remote forward result");
            require(outputs[2] == blocks[task.failed_block_ids[2]],
                    test_case.failed_name + " outputs[2] second remote forward result");
        }
        std::cout << "OptimalLRC k128 shared triple repair verification passed" << std::endl;
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "OptimalLRC k128 triple repair verification failed: "
                  << error.what() << std::endl;
        return 1;
    }
}
