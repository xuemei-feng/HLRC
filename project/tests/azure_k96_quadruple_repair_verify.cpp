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
    if (!condition)
        throw std::runtime_error(message);
}

std::vector<int> range(int first, int last)
{
    return ECProject::RepairBenchmark::inclusive_range(first, last);
}

void verify_registered_case(const std::vector<QuadrupleRepairCase> &cases)
{
    require(cases.size() == 1, "unexpected AzureLRC k96 quadruple case count");
    require(cases[0].failed_name == "G0+G1+G2+L0",
            "missing G0+G1+G2+L0 case");
    require(cases[0].tasks.size() == 1,
            "G0+G1+G2+L0 must use one shared task");

    const RepairTask &task = cases[0].tasks[0];
    require(task.failed_block_ids == std::vector<int>({96, 97, 98, 99}),
            "quadruple failed block order mismatch");
    require(task.source_block_ids == range(0, 95),
            "quadruple source block order mismatch");
    require(task.failed_block_ids.front() == 96,
            "G0 must be the shared task owner proxy");
}

} // namespace

int main()
{
    try {
        constexpr int k = 96;
        constexpr int r = 3;
        constexpr int z = 4;
        constexpr int block_size = 4096;

        const auto cases = ECProject::RepairBenchmark::make_azure_quadruple_block_cases(k);
        verify_registered_case(cases);
        const auto &task = cases[0].tasks[0];

        std::unordered_set<int> all_failed(task.failed_block_ids.begin(),
                                           task.failed_block_ids.end());
        require(all_failed.size() == 4, "case must contain four distinct failures");
        for (int source : task.source_block_ids)
            require(all_failed.count(source) == 0, "source list contains a failed block");

        std::vector<std::vector<unsigned char>> blocks(
            k + r + z, std::vector<unsigned char>(block_size));
        std::mt19937 generator(0x415a5134u);
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

        std::vector<unsigned char> coefficients;
        require(ECProject::get_local_fill_plan(k, r, z, "AzureLRC",
                                               task.failed_block_ids,
                                               task.source_block_ids,
                                               coefficients),
                "quadruple coefficient solve failed");
        const size_t rows = task.failed_block_ids.size();
        const size_t columns = task.source_block_ids.size();
        require(rows == 4, "G0 owner must produce four output rows");
        require(coefficients.size() == rows * columns,
                "quadruple coefficient matrix size mismatch");

        // One read at G0's proxy produces four output rows. The custom multi-output
        // path writes row 0 locally and forwards rows 1-3 to G1, G2, and L0 owners.
        std::vector<std::vector<unsigned char>> recovered(
            rows, std::vector<unsigned char>(block_size, 0));
        for (size_t row = 0; row < rows; ++row) {
            for (size_t column = 0; column < columns; ++column)
                for (int byte = 0; byte < block_size; ++byte)
                    recovered[row][byte] ^= ECProject::gf_mul(
                        coefficients[row * columns + column],
                        blocks[task.source_block_ids[column]][byte]);
            require(recovered[row] == blocks[task.failed_block_ids[row]],
                    "quadruple recovered bytes mismatch");
        }

        require(recovered[0] == blocks[96], "G0 local write value mismatch");
        require(recovered[1] == blocks[97], "G1 forwarded value mismatch");
        require(recovered[2] == blocks[98], "G2 forwarded value mismatch");
        require(recovered[3] == blocks[99], "L0 forwarded value mismatch");

        std::cout << "AzureLRC k96 G0+G1+G2+L0 quadruple repair verification passed"
                  << std::endl;
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "AzureLRC k96 quadruple repair verification failed: "
                  << error.what() << std::endl;
        return 1;
    }
}
