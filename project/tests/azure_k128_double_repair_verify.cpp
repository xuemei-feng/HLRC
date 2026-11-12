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
    if (!condition)
        throw std::runtime_error(message);
}

std::vector<int> range(int first, int last)
{
    return ECProject::RepairBenchmark::inclusive_range(first, last);
}

void require_task(const RepairTask &actual, const std::vector<int> &failed,
                  const std::vector<int> &sources, const std::string &name)
{
    require(actual.failed_block_ids == failed, name + " failed block order mismatch");
    require(actual.source_block_ids == sources, name + " source block order mismatch");
}

void verify_registered_cases(const std::vector<DoubleRepairCase> &cases)
{
    require(cases.size() == 4, "unexpected AzureLRC k128 double case count");

    require(cases[0].failed_name == "L0+L1" && cases[0].tasks.size() == 2,
            "missing L0+L1 case");
    require_task(cases[0].tasks[0], {131}, range(0, 24), "L0+L1/L0");
    require_task(cases[0].tasks[1], {132}, range(25, 49), "L0+L1/L1");

    require(cases[1].failed_name == "L0+L2" && cases[1].tasks.size() == 2,
            "missing L0+L2 case");
    require_task(cases[1].tasks[0], {131}, range(0, 24), "L0+L2/L0");
    require_task(cases[1].tasks[1], {133}, range(50, 75), "L0+L2/L2");

    require(cases[2].failed_name == "L2+L3" && cases[2].tasks.size() == 2,
            "missing L2+L3 case");
    require_task(cases[2].tasks[0], {133}, range(50, 75), "L2+L3/L2");
    require_task(cases[2].tasks[1], {134}, range(76, 101), "L2+L3/L3");

    require(cases[3].failed_name == "G0+G1" && cases[3].tasks.size() == 1,
            "missing G0+G1 shared case");
    require_task(cases[3].tasks[0], {128, 129}, range(0, 127), "G0+G1");
    require(cases[3].tasks[0].failed_block_ids.front() == 128,
            "G0 must be the shared task owner proxy");
}

} // namespace

int main()
{
    try {
        constexpr int k = 128;
        constexpr int r = 3;
        constexpr int z = 5;
        constexpr int block_size = 4096;

        const auto cases = ECProject::RepairBenchmark::make_azure_double_block_cases(k);
        verify_registered_cases(cases);

        std::vector<std::vector<unsigned char>> blocks(
            k + r + z, std::vector<unsigned char>(block_size));
        std::mt19937 generator(0x415a3832u);
        std::uniform_int_distribution<int> distribution(0, 255);
        for (int id = 0; id < k; ++id)
            for (auto &value : blocks[id])
                value = static_cast<unsigned char>(distribution(generator));

        std::vector<unsigned char *> data_ptrs(k);
        std::vector<unsigned char *> parity_ptrs(r + z);
        for (int id = 0; id < k; ++id)
            data_ptrs[id] = blocks[id].data();
        for (int id = 0; id < r + z; ++id)
            parity_ptrs[id] = blocks[k + id].data();
        ECProject::encode_azure_lrc(k, r, z, data_ptrs.data(), parity_ptrs.data(), block_size);

        for (const auto &test_case : cases) {
            std::unordered_set<int> all_failed;
            for (const auto &task : test_case.tasks)
                all_failed.insert(task.failed_block_ids.begin(), task.failed_block_ids.end());
            require(all_failed.size() == 2,
                    test_case.failed_name + " must contain two failures");

            for (const auto &task : test_case.tasks) {
                for (int source : task.source_block_ids)
                    require(all_failed.count(source) == 0,
                            test_case.failed_name + " reads a failed block");

                std::vector<unsigned char> coefficients;
                require(ECProject::get_local_fill_plan(k, r, z, "AzureLRC",
                                                       task.failed_block_ids,
                                                       task.source_block_ids,
                                                       coefficients),
                        test_case.failed_name + " coefficient solve failed");
                const size_t rows = task.failed_block_ids.size();
                const size_t columns = task.source_block_ids.size();
                require(coefficients.size() == rows * columns,
                        test_case.failed_name + " coefficient matrix size mismatch");

                // G0+G1 is one two-output task at G0's proxy. Row 0 is written
                // locally, while row 1 is forwarded to G1's owner proxy.
                std::vector<std::vector<unsigned char>> recovered(
                    rows, std::vector<unsigned char>(block_size, 0));
                for (size_t row = 0; row < rows; ++row) {
                    for (size_t column = 0; column < columns; ++column)
                        for (int byte = 0; byte < block_size; ++byte)
                            recovered[row][byte] ^= ECProject::gf_mul(
                                coefficients[row * columns + column],
                                blocks[task.source_block_ids[column]][byte]);
                    require(recovered[row] == blocks[task.failed_block_ids[row]],
                            test_case.failed_name + " recovered bytes mismatch");
                }

                if (test_case.failed_name == "G0+G1") {
                    require(rows == 2, "G0 owner must produce two output rows");
                    require(recovered[0] == blocks[128], "G0 local write value mismatch");
                    require(recovered[1] == blocks[129], "G1 forwarded value mismatch");
                }
            }
        }

        std::cout << "AzureLRC k128 double repair verification passed" << std::endl;
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "AzureLRC k128 double repair verification failed: "
                  << error.what() << std::endl;
        return 1;
    }
}
