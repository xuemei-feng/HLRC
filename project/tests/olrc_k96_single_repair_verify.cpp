#include "encoder.h"
#include "repair_benchmark_plan.h"

#include <algorithm>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void require(bool condition, const std::string &message)
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
        constexpr int k = 96, r = 3, z = 7, block_size = 4096;
        const auto cases = ECProject::RepairBenchmark::make_olrc_k96_single_block_cases();
        require(cases.size() == 1, "case count mismatch");
        auto expected_sources = ECProject::RepairBenchmark::inclusive_range(0, 23);
        expected_sources.insert(expected_sources.end(), {96, 97, 98});
        require(cases[0].failed_name == "L0" && cases[0].failed_block_id == 99,
                "L0 case mismatch");
        require(cases[0].source_block_ids == expected_sources, "L0 source set mismatch");

        std::vector<std::vector<unsigned char>> blocks(k + r + z,
                                                       std::vector<unsigned char>(block_size));
        std::mt19937 generator(0x4f4c5339u);
        for (int id = 0; id < k; ++id)
            for (auto &value : blocks[id]) value = static_cast<unsigned char>(generator());
        std::vector<unsigned char *> data(k), parity(r + z);
        for (int id = 0; id < k; ++id) data[id] = blocks[id].data();
        for (int id = 0; id < r + z; ++id) parity[id] = blocks[k + id].data();
        ECProject::encode_olrc(k, r, z, data.data(), parity.data(), block_size);

        const auto sources = flatten(ECProject::get_recovery_group_and_block_ids(
            "OLRC", k, r, z, cases[0].failed_block_id));
        require(sources == expected_sources, "recovery read set mismatch");
        require(std::find(sources.begin(), sources.end(), cases[0].failed_block_id) == sources.end(),
                "source contains failure");
        std::vector<unsigned char> coefficients;
        require(ECProject::get_local_fill_plan(k, r, z, "OLRC", {cases[0].failed_block_id},
                                               sources, coefficients),
                "coefficient solve failed");
        require(coefficients.size() == sources.size(), "coefficient count mismatch");

        std::vector<unsigned char *> inputs;
        for (int source : sources) inputs.push_back(blocks[source].data());
        std::vector<unsigned char> recovered(block_size, 0);
        unsigned char *outputs[] = {recovered.data()};
        std::vector<unsigned char> tables(sources.size() * 32);
        ECProject::ec_init_tables(static_cast<int>(sources.size()), 1,
                                  coefficients.data(), tables.data());
        ECProject::ec_encode_data_avx2(block_size, static_cast<int>(sources.size()), 1,
                                       tables.data(), inputs.data(), outputs);
        require(recovered == blocks[cases[0].failed_block_id], "AVX2 recovery mismatch");

        std::vector<unsigned char> scalar(block_size, 0);
        for (size_t source = 0; source < sources.size(); ++source)
            for (int byte = 0; byte < block_size; ++byte)
                scalar[byte] ^= ECProject::gf_mul(coefficients[source],
                                                  blocks[sources[source]][byte]);
        require(scalar == blocks[cases[0].failed_block_id], "scalar recovery mismatch");
        require(ECProject::get_recovery_group_and_block_ids("OLRC", k, r, z, 100).empty(),
                "unsupported failure produced a plan");
        std::cout << "OLRC k96 L0 single repair verification passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "OLRC k96 single repair verification failed: " << error.what() << '\n';
        return 1;
    }
}
