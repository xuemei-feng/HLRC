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

void require_task(const RepairTask &task, const std::vector<int> &failed,
                  const std::vector<int> &sources, const std::string &name)
{
    require(task.failed_block_ids == failed, name + " failed block order mismatch");
    require(task.source_block_ids == sources, name + " source block order mismatch");
}

void verify_registered_cases(const std::vector<TripleRepairCase> &cases)
{
    require(cases.size() == 4, "unexpected AzureLRC k128 triple case count");
    const std::vector<std::string> names = {
        "L0+L1+L2", "L1+L2+L3", "L2+L3+L4"};
    const std::vector<std::vector<int>> failed = {
        {131,132,133}, {132,133,134}, {133,134,135}};
    const std::vector<std::vector<std::vector<int>>> sources = {
        {range(0,24), range(25,49), range(50,75)},
        {range(25,49), range(50,75), range(76,101)},
        {range(50,75), range(76,101), range(102,127)}};
    for (size_t c = 0; c < names.size(); ++c) {
        require(cases[c].failed_name == names[c] && cases[c].tasks.size() == 3,
                "local triple case changed: " + names[c]);
        for (size_t t = 0; t < 3; ++t)
            require_task(cases[c].tasks[t], {failed[c][t]}, sources[c][t], names[c]);
    }
    require(cases[3].failed_name == "G0+G1+G2" && cases[3].tasks.size() == 1,
            "missing G0+G1+G2 shared case");
    require_task(cases[3].tasks[0], {128,129,130}, range(0,127), "G0+G1+G2");
    require(cases[3].tasks[0].failed_block_ids.front() == 128,
            "G0 must be the shared task owner proxy");
}
} // namespace

int main()
{
    try {
        constexpr int k=128, r=3, z=5, block_size=4096;
        const auto cases = ECProject::RepairBenchmark::make_azure_triple_block_cases(k);
        verify_registered_cases(cases);

        std::vector<std::vector<unsigned char>> blocks(k+r+z,
            std::vector<unsigned char>(block_size));
        std::mt19937 generator(0x415a3854u);
        std::uniform_int_distribution<int> distribution(0,255);
        for (int id=0; id<k; ++id)
            for (auto &value: blocks[id]) value=static_cast<unsigned char>(distribution(generator));
        std::vector<unsigned char*> data(k), parity(r+z);
        for (int id=0; id<k; ++id) data[id]=blocks[id].data();
        for (int id=0; id<r+z; ++id) parity[id]=blocks[k+id].data();
        ECProject::encode_azure_lrc(k,r,z,data.data(),parity.data(),block_size);

        for (const auto &test_case: cases) {
            std::unordered_set<int> all_failed;
            for (const auto &task: test_case.tasks)
                all_failed.insert(task.failed_block_ids.begin(),task.failed_block_ids.end());
            require(all_failed.size()==3,test_case.failed_name+" must contain three failures");
            for (const auto &task: test_case.tasks) {
                for (int source: task.source_block_ids)
                    require(all_failed.count(source)==0,test_case.failed_name+" reads a failed block");
                std::vector<unsigned char> coefficients;
                require(ECProject::get_local_fill_plan(k,r,z,"AzureLRC",task.failed_block_ids,
                                                       task.source_block_ids,coefficients),
                        test_case.failed_name+" coefficient solve failed");
                const size_t rows=task.failed_block_ids.size(), columns=task.source_block_ids.size();
                require(coefficients.size()==rows*columns,"coefficient matrix size mismatch");
                std::vector<std::vector<unsigned char>> recovered(rows,
                    std::vector<unsigned char>(block_size,0));
                for (size_t row=0; row<rows; ++row) {
                    for (size_t col=0; col<columns; ++col)
                        for (int byte=0; byte<block_size; ++byte)
                            recovered[row][byte]^=ECProject::gf_mul(
                                coefficients[row*columns+col],blocks[task.source_block_ids[col]][byte]);
                    require(recovered[row]==blocks[task.failed_block_ids[row]],
                            test_case.failed_name+" recovered bytes mismatch");
                }
                if (test_case.failed_name=="G0+G1+G2") {
                    require(rows==3,"G0 owner must produce three output rows");
                    require(recovered[0]==blocks[128],"G0 local write value mismatch");
                    require(recovered[1]==blocks[129],"G1 forwarded value mismatch");
                    require(recovered[2]==blocks[130],"G2 forwarded value mismatch");
                }
            }
        }
        std::cout << "AzureLRC k128 triple repair verification passed" << std::endl;
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "AzureLRC k128 triple repair verification failed: " << error.what() << std::endl;
        return 1;
    }
}
