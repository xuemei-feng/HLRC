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
        constexpr int k = 64, r = 3, z = 5, block_size = 4096;
        const auto cases = ECProject::RepairBenchmark::make_nlrc_k64_single_block_cases();
        require(cases.size() == 2, "unexpected case count");
        require(cases[0].failed_name == "L0" && cases[0].failed_block_id == 67,
                "bad L0 case");
        require(cases[0].source_block_ids == ECProject::RepairBenchmark::inclusive_range(0, 21),
                "bad L0 sources");
        auto expected_l4 = ECProject::RepairBenchmark::inclusive_range(44, 63);
        expected_l4.insert(expected_l4.end(), {64, 65, 66});
        require(cases[1].failed_name == "L4" && cases[1].failed_block_id == 71,
                "bad L4 case");
        require(cases[1].source_block_ids == expected_l4, "bad L4 sources");

        std::vector<std::vector<unsigned char>> blocks(k + r + z,
                                                       std::vector<unsigned char>(block_size));
        std::mt19937 generator(0x4e4c5243u);
        std::uniform_int_distribution<int> distribution(0, 255);
        for (int id = 0; id < k; ++id)
            for (auto &value : blocks[id])
                value = static_cast<unsigned char>(distribution(generator));
        std::vector<unsigned char *> data_ptrs(k), parity_ptrs(r + z);
        for (int id = 0; id < k; ++id) data_ptrs[id] = blocks[id].data();
        for (int id = 0; id < r + z; ++id) parity_ptrs[id] = blocks[k + id].data();
        ECProject::encode_nlrc(k, r, z, data_ptrs.data(), parity_ptrs.data(), block_size);

        const auto group_by_block = ECProject::get_nlrc_block_id_to_group_id(k, r, z);
        for (const auto &test_case : cases) {
            require(std::find(test_case.source_block_ids.begin(), test_case.source_block_ids.end(),
                              test_case.failed_block_id) == test_case.source_block_ids.end(),
                    "source set contains failed block");
            const auto plan = ECProject::get_recovery_group_and_block_ids(
                "NLRC", k, r, z, test_case.failed_block_id);
            const auto sources = flatten(plan);
            require(sources == test_case.source_block_ids, "recovery plan sources differ");
            for (const auto &entry : plan)
                for (int source : entry.second)
                    require(group_by_block.at(source) == entry.first, "incorrect physical group");

            std::vector<unsigned char> coefficients;
            require(ECProject::get_local_fill_plan(k, r, z, "NLRC",
                                                   {test_case.failed_block_id}, sources,
                                                   coefficients),
                    "coefficient solve failed");
            require(coefficients.size() == sources.size(), "bad coefficient count");

            std::vector<unsigned char> direct(block_size, 0);
            for (size_t i = 0; i < sources.size(); ++i)
                for (int byte = 0; byte < block_size; ++byte)
                    direct[byte] ^= ECProject::gf_mul(coefficients[i], blocks[sources[i]][byte]);
            require(direct == blocks[test_case.failed_block_id], "direct GF recovery mismatch");

            std::vector<unsigned char *> source_ptrs;
            for (int source : sources) source_ptrs.push_back(blocks[source].data());
            std::vector<unsigned char> decoded(block_size, 0);
            ECProject::decode_nlrc(k, r, z, static_cast<int>(sources.size()), &sources,
                                   source_ptrs.data(), decoded.data(), block_size,
                                   test_case.failed_block_id);
            require(decoded == blocks[test_case.failed_block_id], "decode_nlrc mismatch");
        }

        for (int failed = 0; failed < k + r + z; ++failed)
            if (failed != 67 && failed != 71)
                require(ECProject::get_recovery_group_and_block_ids(
                            "NLRC", k, r, z, failed).empty(),
                        "unsupported failure produced a plan");
        require(ECProject::get_recovery_group_and_block_ids("NLRC", k, 2, z, 67).empty(),
                "unsupported NLRC r produced a plan");
        std::cout << "NLRC k64 single repair verification passed" << std::endl;
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "NLRC k64 single repair verification failed: " << error.what() << std::endl;
        return 1;
    }
}
