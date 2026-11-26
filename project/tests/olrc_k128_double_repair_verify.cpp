#include "encoder.h"
#include "repair_benchmark_plan.h"

#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace {
using ECProject::RepairBenchmark::DoubleRepairCase;
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

void require_task(const RepairTask &actual, std::vector<int> failed,
                  std::vector<int> sources)
{
    require(actual.failed_block_ids == failed, "failed block order mismatch");
    require(actual.source_block_ids == sources, "source block order mismatch");
    require(actual.failed_block_ids.front() == failed.front(), "owner proxy mismatch");
}

void verify_plan(const std::vector<DoubleRepairCase> &cases)
{
    const std::vector<std::string> names = {
        "L0+L1", "L4+L5", "L0+L2", "L0+L4", "L4+L6", "G0+L8"};
    require(cases.size() == names.size(), "case count mismatch");
    for (size_t index = 0; index < cases.size(); ++index) {
        require(cases[index].failed_name == names[index], "case order mismatch");
        require(cases[index].tasks.size() == 1, "case must use one shared task");
        require(!cases[index].sum_task_times, "shared task must use wall timing");
    }
    auto l0_l4 = range(0, 24);
    const auto l4_group = range(50, 75);
    l0_l4.insert(l0_l4.end(), l4_group.begin(), l4_group.end());
    require_task(cases[0].tasks[0], {131, 132}, with_globals(range(0, 24)));
    require_task(cases[1].tasks[0], {135, 136}, with_globals(range(50, 75)));
    require_task(cases[2].tasks[0], {131, 133}, with_globals(range(0, 49)));
    require_task(cases[3].tasks[0], {131, 135}, with_globals(std::move(l0_l4)));
    require_task(cases[4].tasks[0], {135, 137}, with_globals(range(50, 101)));
    require_task(cases[5].tasks[0], {128, 139}, range(0, 127));
}
}

int main()
{
    try {
        constexpr int k = 128, r = 3, z = 9, block_size = 4096;
        const auto cases = ECProject::RepairBenchmark::make_olrc_k128_double_block_cases();
        verify_plan(cases);
        std::vector<std::vector<unsigned char>> blocks(k + r + z,
                                                       std::vector<unsigned char>(block_size));
        std::mt19937 generator(0x4f4c4438u);
        for (int id = 0; id < k; ++id)
            for (auto &value : blocks[id]) value = static_cast<unsigned char>(generator());
        std::vector<unsigned char *> data(k), parity(r + z);
        for (int id = 0; id < k; ++id) data[id] = blocks[id].data();
        for (int id = 0; id < r + z; ++id) parity[id] = blocks[k + id].data();
        ECProject::encode_olrc(k, r, z, data.data(), parity.data(), block_size);

        for (const auto &test_case : cases) {
            const auto &task = test_case.tasks.front();
            std::unordered_set<int> failed(task.failed_block_ids.begin(), task.failed_block_ids.end());
            require(failed.size() == 2, "case must contain two failures");
            for (int source : task.source_block_ids)
                require(!failed.count(source), "source contains failed block");
            std::vector<unsigned char> coefficients;
            require(ECProject::get_local_fill_plan(k, r, z, "OLRC", task.failed_block_ids,
                                                   task.source_block_ids, coefficients),
                    test_case.failed_name + " coefficient solve failed");
            const int rows = static_cast<int>(task.failed_block_ids.size());
            const int columns = static_cast<int>(task.source_block_ids.size());
            require(rows == 2 && coefficients.size() == static_cast<size_t>(rows * columns),
                    "coefficient matrix size mismatch");
            std::vector<unsigned char *> inputs;
            for (int source : task.source_block_ids) inputs.push_back(blocks[source].data());
            std::vector<std::vector<unsigned char>> recovered(rows,
                                                               std::vector<unsigned char>(block_size));
            std::vector<unsigned char *> outputs;
            for (auto &output : recovered) outputs.push_back(output.data());
            std::vector<unsigned char> tables(static_cast<size_t>(rows) * columns * 32);
            ECProject::ec_init_tables(columns, rows, coefficients.data(), tables.data());
            ECProject::ec_encode_data_avx2(block_size, columns, rows, tables.data(),
                                           inputs.data(), outputs.data());
            for (int row = 0; row < rows; ++row) {
                require(recovered[row] == blocks[task.failed_block_ids[row]],
                        test_case.failed_name + " AVX2 recovery mismatch");
                std::vector<unsigned char> scalar(block_size, 0);
                for (int column = 0; column < columns; ++column)
                    for (int byte = 0; byte < block_size; ++byte)
                        scalar[byte] ^= ECProject::gf_mul(
                            coefficients[row * columns + column],
                            blocks[task.source_block_ids[column]][byte]);
                require(scalar == blocks[task.failed_block_ids[row]],
                        test_case.failed_name + " scalar recovery mismatch");
            }
        }
        std::cout << "OLRC k128 double repair verification passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "OLRC k128 double repair verification failed: " << error.what() << '\n';
        return 1;
    }
}
