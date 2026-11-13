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

void require(bool condition, const char *message)
{
    if (!condition) throw std::runtime_error(message);
}

std::vector<int> range(int first, int last)
{
    return ECProject::RepairBenchmark::inclusive_range(first, last);
}


void require_task(const RepairTask &actual, std::vector<int> failed, std::vector<int> sources)
{
    require(actual.failed_block_ids == failed, "failed block order mismatch");
    require(actual.source_block_ids == sources, "source block order mismatch");
    require(actual.failed_block_ids.front() == failed.front(), "owner proxy mismatch");
}

void verify_registered_cases(const std::vector<TripleRepairCase> &cases)
{
    const std::vector<std::string> names={"L0+L1+L2","L2+L3+L4","L0+L2+L4","L2+L4+L6","G0+G1+G2"};
    const std::vector<size_t> counts={2,2,3,3,1};require(cases.size()==names.size(),"unexpected case count");for(size_t i=0;i<names.size();++i){require(cases[i].failed_name==names[i],"case order mismatch");require(cases[i].tasks.size()==counts[i],"task count mismatch");}
    require_task(cases[0].tasks[0],{99,100},range(0,23));require_task(cases[0].tasks[1],{101},range(24,48));
    require_task(cases[1].tasks[0],{101,102},range(24,48));require_task(cases[1].tasks[1],{103},range(49,73));
    require_task(cases[2].tasks[0],{99},range(0,23));require_task(cases[2].tasks[1],{101},range(24,48));require_task(cases[2].tasks[2],{103},range(49,73));
    auto l6=range(74,95);l6.insert(l6.end(),{96,97,98});require_task(cases[3].tasks[0],{101},range(24,48));require_task(cases[3].tasks[1],{103},range(49,73));require_task(cases[3].tasks[2],{105},l6);
    require_task(cases[4].tasks[0],{96,97,98},range(0,95));
}
}

int main()
{
    try {
        constexpr int k = 96, r = 3, z = 7, block_size = 4096;
        const auto cases = ECProject::RepairBenchmark::make_nlrc_k96_triple_block_cases();
        verify_registered_cases(cases);

        std::vector<std::vector<unsigned char>> blocks(k + r + z,
                                                       std::vector<unsigned char>(block_size));
        std::mt19937 generator(0x4e4c9633u);
        std::uniform_int_distribution<int> distribution(0, 255);
        for (int id = 0; id < k; ++id)
            for (auto &value : blocks[id]) value = static_cast<unsigned char>(distribution(generator));
        std::vector<unsigned char *> data_ptrs(k), parity_ptrs(r + z);
        for (int id = 0; id < k; ++id) data_ptrs[id] = blocks[id].data();
        for (int id = 0; id < r + z; ++id) parity_ptrs[id] = blocks[k + id].data();
        ECProject::encode_nlrc(k, r, z, data_ptrs.data(), parity_ptrs.data(), block_size);

        bool saw_two_outputs = false;
        bool saw_three_outputs = false;
        for (const auto &test_case : cases) {
            std::unordered_set<int> case_failed;
            for (const auto &task : test_case.tasks)
                case_failed.insert(task.failed_block_ids.begin(), task.failed_block_ids.end());
            require(case_failed.size() == 3, "case must contain exactly three failed blocks");

            for (const auto &task : test_case.tasks) {
                for (int source : task.source_block_ids)
                    require(case_failed.count(source) == 0,
                            "source contains a failed block from this case");

                std::vector<unsigned char> coefficients;
                require(ECProject::get_local_fill_plan(k, r, z, "NLRC",
                                                       task.failed_block_ids,
                                                       task.source_block_ids,
                                                       coefficients),
                        "coefficient solve failed");
                const size_t rows = task.failed_block_ids.size();
                const size_t columns = task.source_block_ids.size();
                require(coefficients.size() == rows * columns,
                        "coefficient matrix size mismatch");
                saw_two_outputs = saw_two_outputs || rows == 2;
                saw_three_outputs = saw_three_outputs || rows == 3;

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
        require(saw_two_outputs, "no shared two-output task was verified");
        require(saw_three_outputs, "no shared three-output task was verified");

        std::cout << "NLRC k96 triple repair verification passed" << std::endl;
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "NLRC k96 triple repair verification failed: "
                  << error.what() << std::endl;
        return 1;
    }
}
