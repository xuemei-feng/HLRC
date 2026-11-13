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
        constexpr int k = 96, r = 2, z = 8, block_size = 4096;
        const auto cases = ECProject::RepairBenchmark::make_hlrc_k96_single_block_cases();
        require(cases.size() == 3, "unexpected case count");
        require(cases[0].failed_name == "L1" && cases[0].failed_block_id == 99, "bad L1 case");
        require(cases[1].failed_name == "L0" && cases[1].failed_block_id == 98, "bad L0 case");
        require(cases[2].failed_name == "U0" && cases[2].failed_block_id == 104, "bad U0 case");
        require(cases[0].source_block_ids == ECProject::RepairBenchmark::inclusive_range(16, 31), "bad L1 sources");
        auto expected_l0 = ECProject::RepairBenchmark::inclusive_range(0, 15); expected_l0.push_back(96);
        require(cases[1].source_block_ids == expected_l0, "bad L0 sources");
        auto expected_u0 = ECProject::RepairBenchmark::inclusive_range(1, 15);
        const auto u0_middle = ECProject::RepairBenchmark::inclusive_range(18, 31);
        const auto u0_upper = ECProject::RepairBenchmark::inclusive_range(34, 47);
        expected_u0.insert(expected_u0.end(), u0_middle.begin(), u0_middle.end());
        expected_u0.insert(expected_u0.end(), u0_upper.begin(), u0_upper.end());
        require(cases[2].source_block_ids == expected_u0, "bad U0 sources");

        std::vector<std::vector<unsigned char>> blocks(k + r + z,
                                                       std::vector<unsigned char>(block_size));
        std::mt19937 generator(0x484c9631u);
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

        require(ECProject::get_recovery_group_and_block_ids("HLRC", k, r, z, 97).empty(),
                "unsupported failure produced a plan");
        std::cout << "HLRC k96 single repair verification passed" << std::endl;
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "HLRC k96 single repair verification failed: " << error.what() << std::endl;
        return 1;
    }
}
