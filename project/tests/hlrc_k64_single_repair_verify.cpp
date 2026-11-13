#include "encoder.h"
#include "repair_benchmark_plan.h"

#include <algorithm>
#include <iostream>
#include <random>
#include <stdexcept>
#include <vector>

namespace {
void require(bool condition, const char *message)
{
    if (!condition) throw std::runtime_error(message);
}

std::vector<int> flatten(const std::vector<std::pair<int, std::vector<int>>> &plan)
{
    std::vector<int> result;
    for (const auto &entry : plan)
        result.insert(result.end(), entry.second.begin(), entry.second.end());
    return result;
}
}

int main()
{
    try {
        constexpr int k = 64, r = 2, z = 6, block_size = 4096;
        const auto cases = ECProject::RepairBenchmark::make_hlrc_k64_single_block_cases();
        require(cases.size() == 3, "unexpected case count");
        require(cases[0].failed_name == "L1" && cases[0].failed_block_id == 67, "bad L1 case");
        require(cases[1].failed_name == "L0" && cases[1].failed_block_id == 66, "bad L0 case");
        require(cases[2].failed_name == "U0" && cases[2].failed_block_id == 70, "bad U0 case");

        std::vector<std::vector<unsigned char>> blocks(k + r + z,
                                                       std::vector<unsigned char>(block_size));
        std::mt19937 generator(0x484c5243u);
        std::uniform_int_distribution<int> distribution(0, 255);
        for (int id = 0; id < k; ++id)
            for (auto &value : blocks[id]) value = static_cast<unsigned char>(distribution(generator));
        std::vector<unsigned char *> data_ptrs(k), parity_ptrs(r + z);
        for (int id = 0; id < k; ++id) data_ptrs[id] = blocks[id].data();
        for (int id = 0; id < r + z; ++id) parity_ptrs[id] = blocks[k + id].data();
        ECProject::encode_hlrc(k, r, z, data_ptrs.data(), parity_ptrs.data(), block_size);

        for (const auto &test_case : cases) {
            const auto plan = ECProject::get_recovery_group_and_block_ids(
                "HLRC", k, r, z, test_case.failed_block_id);
            const auto sources = flatten(plan);
            require(sources == test_case.source_block_ids, "recovery plan sources differ");

            const auto group_by_block = ECProject::get_hlrc_block_id_to_group_id(k, r, z);
            for (const auto &entry : plan)
                for (int source : entry.second)
                    require(group_by_block.at(source) == entry.first, "incorrect physical group");

            std::vector<unsigned char> coefficients;
            require(ECProject::get_local_fill_plan(k, r, z, "HLRC",
                                                   {test_case.failed_block_id}, sources,
                                                   coefficients),
                    "coefficient solve failed");
            require(coefficients.size() == sources.size(), "bad coefficient count");
            require(std::any_of(coefficients.begin(), coefficients.end(),
                                [](unsigned char coefficient) { return coefficient != 0 && coefficient != 1; }),
                    "test did not exercise weighted GF coefficients");

            std::vector<unsigned char> direct(block_size, 0);
            for (size_t i = 0; i < sources.size(); ++i)
                for (int byte = 0; byte < block_size; ++byte)
                    direct[byte] ^= ECProject::gf_mul(coefficients[i], blocks[sources[i]][byte]);
            require(direct == blocks[test_case.failed_block_id], "direct GF recovery mismatch");

            std::vector<unsigned char *> source_ptrs;
            for (int source : sources) source_ptrs.push_back(blocks[source].data());
            std::vector<unsigned char> decoded(block_size, 0);
            ECProject::decode_hlrc(k, r, z, static_cast<int>(sources.size()), &sources,
                                   source_ptrs.data(), decoded.data(), block_size,
                                   test_case.failed_block_id);
            require(decoded == blocks[test_case.failed_block_id], "decode_hlrc mismatch");
        }

        require(ECProject::get_recovery_group_and_block_ids("HLRC", k, r, z, 65).empty(),
                "unsupported failure produced a plan");
        std::cout << "HLRC k64 single repair verification passed" << std::endl;
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "HLRC k64 single repair verification failed: " << error.what() << std::endl;
        return 1;
    }
}
