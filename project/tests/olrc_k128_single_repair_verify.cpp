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
        constexpr int k = 128, r = 3, z = 9, block_size = 4096;
        const auto cases = ECProject::RepairBenchmark::make_olrc_k128_single_block_cases();
        require(cases.size() == 2, "case count mismatch");
        auto l0_sources = ECProject::RepairBenchmark::inclusive_range(0, 24);
        l0_sources.insert(l0_sources.end(), {128, 129, 130});
        auto l4_sources = ECProject::RepairBenchmark::inclusive_range(50, 75);
        l4_sources.insert(l4_sources.end(), {128, 129, 130});
        require(cases[0].failed_name == "L0" && cases[0].failed_block_id == 131,
                "L0 case mismatch");
        require(cases[1].failed_name == "L4" && cases[1].failed_block_id == 135,
                "L4 case mismatch");
        require(cases[0].source_block_ids == l0_sources, "L0 source set mismatch");
        require(cases[1].source_block_ids == l4_sources, "L4 source set mismatch");

        std::vector<std::vector<unsigned char>> blocks(k + r + z,
                                                       std::vector<unsigned char>(block_size));
        std::mt19937 generator(0x4f4c5338u);
        for (int id = 0; id < k; ++id)
            for (auto &value : blocks[id]) value = static_cast<unsigned char>(generator());
        std::vector<unsigned char *> data(k), parity(r + z);
        for (int id = 0; id < k; ++id) data[id] = blocks[id].data();
        for (int id = 0; id < r + z; ++id) parity[id] = blocks[k + id].data();
        ECProject::encode_olrc(k, r, z, data.data(), parity.data(), block_size);

        for (const auto &test_case : cases) {
            const auto sources = flatten(ECProject::get_recovery_group_and_block_ids(
                "OLRC", k, r, z, test_case.failed_block_id));
            require(sources == test_case.source_block_ids, test_case.failed_name + " read set mismatch");
            require(std::find(sources.begin(), sources.end(), test_case.failed_block_id) == sources.end(),
                    "source contains failure");
            std::vector<unsigned char> coefficients;
            require(ECProject::get_local_fill_plan(k, r, z, "OLRC", {test_case.failed_block_id},
                                                   sources, coefficients), "coefficient solve failed");
            std::vector<unsigned char *> inputs;
            for (int source : sources) inputs.push_back(blocks[source].data());
            std::vector<unsigned char> recovered(block_size, 0);
            unsigned char *outputs[] = {recovered.data()};
            std::vector<unsigned char> tables(sources.size() * 32);
            ECProject::ec_init_tables(static_cast<int>(sources.size()), 1, coefficients.data(), tables.data());
            ECProject::ec_encode_data_avx2(block_size, static_cast<int>(sources.size()), 1,
                                           tables.data(), inputs.data(), outputs);
            require(recovered == blocks[test_case.failed_block_id], "AVX2 recovery mismatch");
            std::vector<unsigned char> scalar(block_size, 0);
            for (size_t source = 0; source < sources.size(); ++source)
                for (int byte = 0; byte < block_size; ++byte)
                    scalar[byte] ^= ECProject::gf_mul(coefficients[source], blocks[sources[source]][byte]);
            require(scalar == blocks[test_case.failed_block_id], "scalar recovery mismatch");
        }
        std::cout << "OLRC k128 L0/L4 single repair verification passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "OLRC k128 single repair verification failed: " << error.what() << '\n';
        return 1;
    }
}
