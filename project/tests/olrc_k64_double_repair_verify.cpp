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

void require_task(const RepairTask &actual, std::vector<int> failed,
                  std::vector<int> sources)
{
    require(actual.failed_block_ids == failed, "failed block order mismatch");
    require(actual.source_block_ids == sources, "source block order mismatch");
    require(actual.failed_block_ids.front() == failed.front(), "owner proxy mismatch");
}

void verify_plan(const std::vector<DoubleRepairCase> &cases)
{
    require(cases.size() == 3, "case count mismatch");
    require(cases[0].failed_name == "L0+L1", "L0+L1 case mismatch");
    require(cases[1].failed_name == "L0+L2", "L0+L2 case mismatch");
    require(cases[2].failed_name == "G0+L4", "G0+L4 case mismatch");

    auto first_sources = range(0, 20);
    first_sources.insert(first_sources.end(), {64, 65, 66});
    auto second_sources = range(0, 41);
    second_sources.insert(second_sources.end(), {64, 65, 66});
    require(cases[0].tasks.size() == 1, "L0+L1 must use one shared task");
    require(cases[1].tasks.size() == 1, "L0+L2 must use one shared task");
    require(cases[2].tasks.size() == 1, "G0+L4 must use one shared task");
    require_task(cases[0].tasks[0], {67, 68}, std::move(first_sources));
    require_task(cases[1].tasks[0], {67, 69}, std::move(second_sources));
    require_task(cases[2].tasks[0], {64, 71}, range(0, 63));
    for (const auto &test_case : cases)
        require(!test_case.sum_task_times, "shared task must use wall timing");
}
}

int main()
{
    try {
        constexpr int k = 64, r = 3, z = 5, block_size = 4096;
        const auto cases = ECProject::RepairBenchmark::make_olrc_k64_double_block_cases();
        verify_plan(cases);

        std::vector<std::vector<unsigned char>> blocks(k + r + z,
                                                       std::vector<unsigned char>(block_size));
        std::mt19937 generator(0x4f4c4436u);
        for (int id = 0; id < k; ++id)
            for (auto &value : blocks[id]) value = static_cast<unsigned char>(generator());
        std::vector<unsigned char *> data(k), parity(r + z);
        for (int id = 0; id < k; ++id) data[id] = blocks[id].data();
        for (int id = 0; id < r + z; ++id) parity[id] = blocks[k + id].data();
        ECProject::encode_olrc(k, r, z, data.data(), parity.data(), block_size);

        for (const auto &test_case : cases) {
            std::unordered_set<int> failed;
            for (const auto &task : test_case.tasks)
                failed.insert(task.failed_block_ids.begin(), task.failed_block_ids.end());
            require(failed.size() == 2, "case must contain two failures");

            const auto &task = test_case.tasks.front();
            for (int source : task.source_block_ids)
                require(failed.count(source) == 0, "source contains failed block");
            std::vector<unsigned char> coefficients;
            require(ECProject::get_local_fill_plan(k, r, z, "OLRC",
                    task.failed_block_ids, task.source_block_ids, coefficients),
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
        std::cout << "OLRC k64 double repair verification passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "OLRC k64 double repair verification failed: " << error.what() << '\n';
        return 1;
    }
}
