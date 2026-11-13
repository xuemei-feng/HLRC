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

void require(bool condition, const char *message)
{
    if (!condition) throw std::runtime_error(message);
}

std::vector<int> range(int first, int last)
{
    return ECProject::RepairBenchmark::inclusive_range(first, last);
}

std::vector<int> joined(std::vector<int> first, const std::vector<int> &second)
{
    first.insert(first.end(), second.begin(), second.end());
    return first;
}

void require_task(const RepairTask &actual, std::vector<int> failed, std::vector<int> sources)
{
    require(actual.failed_block_ids == failed, "failed block order mismatch");
    require(actual.source_block_ids == sources, "source block order mismatch");
    require(actual.failed_block_ids.front() == failed.front(), "owner proxy mismatch");
}

void verify_registered_cases(const std::vector<QuadrupleRepairCase> &cases)
{
    const std::vector<std::string> names = {
        "L0+L1+L2+U0", "L1+L2+L4+U0", "L1+L2+L4+L5", "L0+L1+L2+L4",
        "L0+L1+L2+L3", "L1+L4+L5+U0", "L1+L3+L4+U0", "L1+L4+U0+U1",
        "L0+L1+L2+U1", "G0+G1+L1+L2"};
    const std::vector<size_t> counts = {1, 2, 4, 4, 4, 3, 3, 2, 4, 1};
    require(cases.size() == names.size(), "unexpected case count");
    for (size_t i = 0; i < names.size(); ++i) {
        require(cases[i].failed_name == names[i], "case name or order mismatch");
        require(cases[i].tasks.size() == counts[i], "task count mismatch");
    }
    const auto all_first_tier = joined(joined(joined(range(0, 15), {96}), range(16, 31)), range(32, 47));
    const auto shared_three = joined(joined(range(1, 15), range(16, 31)), range(32, 47));
    const auto shared_two = joined(joined(range(16, 31), range(1, 15)), range(34, 47));
    const auto shared_u1_l4 = joined(joined(range(49, 63), range(64, 79)), range(82, 95));
    const auto l0 = joined(range(0, 15), {96});
    const auto l3 = joined(range(48, 63), {97});
    const auto u1 = joined(joined(range(49, 63), range(66, 79)), range(82, 95));
    require_task(cases[0].tasks[0], {104,98,99,100}, all_first_tier);
    require_task(cases[1].tasks[0], {104,99,100}, shared_three); require_task(cases[1].tasks[1], {102}, range(64,79));
    require_task(cases[2].tasks[0], {99}, range(16,31)); require_task(cases[2].tasks[1], {100}, range(32,47)); require_task(cases[2].tasks[2], {102}, range(64,79)); require_task(cases[2].tasks[3], {103}, range(80,95));
    require_task(cases[3].tasks[0], {98}, l0); require_task(cases[3].tasks[1], {99}, range(16,31)); require_task(cases[3].tasks[2], {100}, range(32,47)); require_task(cases[3].tasks[3], {102}, range(64,79));
    require_task(cases[4].tasks[0], {98}, l0); require_task(cases[4].tasks[1], {99}, range(16,31)); require_task(cases[4].tasks[2], {100}, range(32,47)); require_task(cases[4].tasks[3], {101}, l3);
    require_task(cases[5].tasks[0], {104,99}, shared_two); require_task(cases[5].tasks[1], {102}, range(64,79)); require_task(cases[5].tasks[2], {103}, range(80,95));
    require_task(cases[6].tasks[0], {104,99}, shared_two); require_task(cases[6].tasks[1], {101}, l3); require_task(cases[6].tasks[2], {102}, range(64,79));
    require_task(cases[7].tasks[0], {104,99}, shared_two); require_task(cases[7].tasks[1], {105,102}, shared_u1_l4);
    require_task(cases[8].tasks[0], {98}, l0); require_task(cases[8].tasks[1], {99}, range(16,31)); require_task(cases[8].tasks[2], {100}, range(32,47)); require_task(cases[8].tasks[3], {105}, u1);
    require_task(cases[9].tasks[0], {96,97,99,100}, range(0,95));
}
}

int main()
{
    try {
        constexpr int k = 96, r = 2, z = 8, block_size = 4096;
        const auto cases = ECProject::RepairBenchmark::make_hlrc_k96_quadruple_block_cases();
        verify_registered_cases(cases);

        std::vector<std::vector<unsigned char>> blocks(k + r + z,
                                                       std::vector<unsigned char>(block_size));
        std::mt19937 generator(0x4b393651u);
        std::uniform_int_distribution<int> distribution(0, 255);
        for (int id = 0; id < k; ++id)
            for (auto &value : blocks[id]) value = static_cast<unsigned char>(distribution(generator));
        std::vector<unsigned char *> data_ptrs(k), parity_ptrs(r + z);
        for (int id = 0; id < k; ++id) data_ptrs[id] = blocks[id].data();
        for (int id = 0; id < r + z; ++id) parity_ptrs[id] = blocks[k + id].data();
        ECProject::encode_hlrc(k, r, z, data_ptrs.data(), parity_ptrs.data(), block_size);

        bool saw_three_outputs = false;
        bool saw_four_outputs = false;
        for (const auto &test_case : cases) {
            std::unordered_set<int> case_failed;
            for (const auto &task : test_case.tasks)
                case_failed.insert(task.failed_block_ids.begin(), task.failed_block_ids.end());
            require(case_failed.size() == 4, "case must contain exactly four failed blocks");

            for (const auto &task : test_case.tasks) {
                for (int source : task.source_block_ids)
                    require(case_failed.count(source) == 0,
                            "source contains a failed block from this case");

                std::vector<unsigned char> coefficients;
                require(ECProject::get_local_fill_plan(k, r, z, "HLRC",
                                                       task.failed_block_ids,
                                                       task.source_block_ids,
                                                       coefficients),
                        "coefficient solve failed");
                const size_t rows = task.failed_block_ids.size();
                const size_t columns = task.source_block_ids.size();
                require(coefficients.size() == rows * columns,
                        "coefficient matrix size mismatch");
                if (rows == 3) saw_three_outputs = true;
                if (rows == 4) saw_four_outputs = true;

                std::vector<unsigned char *> sources;
                for (int source : task.source_block_ids) sources.push_back(blocks[source].data());
                std::vector<std::vector<unsigned char>> recovered(
                    rows, std::vector<unsigned char>(block_size));
                std::vector<unsigned char *> outputs;
                for (auto &output : recovered) outputs.push_back(output.data());
                std::vector<unsigned char> tables(columns * rows * 32);
                ECProject::ec_init_tables(static_cast<int>(columns), static_cast<int>(rows),
                                          coefficients.data(), tables.data());
                ECProject::ec_encode_data_avx2(block_size, static_cast<int>(columns),
                                               static_cast<int>(rows), tables.data(),
                                               sources.data(), outputs.data());

                for (size_t row = 0; row < rows; ++row) {
                    require(recovered[row] == blocks[task.failed_block_ids[row]],
                            "AVX2 recovery mismatch");
                    std::vector<unsigned char> direct(block_size, 0);
                    for (size_t column = 0; column < columns; ++column)
                        for (int byte = 0; byte < block_size; ++byte)
                            direct[byte] ^= ECProject::gf_mul(
                                coefficients[row * columns + column],
                                blocks[task.source_block_ids[column]][byte]);
                    require(direct == blocks[task.failed_block_ids[row]],
                            "direct GF recovery mismatch");
                }
            }
        }
        require(saw_three_outputs, "missing shared three-output verification");
        require(saw_four_outputs, "missing shared four-output verification");

        std::cout << "HLRC k96 quadruple repair verification passed" << std::endl;
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "HLRC k96 quadruple repair verification failed: "
                  << error.what() << std::endl;
        return 1;
    }
}
