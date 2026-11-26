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
        constexpr int k = 64, r = 3, z = 5, block_size = 4096;
        const auto cases = ECProject::RepairBenchmark::make_olrc_k64_single_block_cases();
        require(cases.size() == 2, "case count mismatch");
        require(cases[0].failed_name == "L0" && cases[0].failed_block_id == 67,
                "L0 case mismatch");
        require(cases[1].failed_name == "L4" && cases[1].failed_block_id == 71,
                "L4 case mismatch");

        std::vector<std::vector<unsigned char>> blocks(k + r + z,
                                                       std::vector<unsigned char>(block_size));
        std::mt19937 generator(0x4f4c5336u);
        for (int id = 0; id < k; ++id)
            for (auto &value : blocks[id]) value = static_cast<unsigned char>(generator());
        std::vector<unsigned char *> data(k), parity(r + z);
        for (int id = 0; id < k; ++id) data[id] = blocks[id].data();
        for (int id = 0; id < r + z; ++id) parity[id] = blocks[k + id].data();
        ECProject::encode_olrc(k, r, z, data.data(), parity.data(), block_size);

        for (const auto &test_case : cases) {
            const auto sources = flatten(ECProject::get_recovery_group_and_block_ids(
                "OLRC", k, r, z, test_case.failed_block_id));
            require(sources == test_case.source_block_ids,
                    test_case.failed_name + " read set mismatch");
            require(std::find(sources.begin(), sources.end(), test_case.failed_block_id) == sources.end(),
                    test_case.failed_name + " source contains failure");

            std::vector<unsigned char> coefficients;
            require(ECProject::get_local_fill_plan(k, r, z, "OLRC",
                    {test_case.failed_block_id}, sources, coefficients),
                    test_case.failed_name + " coefficient solve failed");
            require(coefficients.size() == sources.size(),
                    test_case.failed_name + " coefficient count mismatch");

            std::vector<unsigned char *> source_ptrs;
            for (int source : sources) source_ptrs.push_back(blocks[source].data());
            std::vector<unsigned char> recovered(block_size, 0);
            unsigned char *output[] = {recovered.data()};
            std::vector<unsigned char> tables(sources.size() * 32);
            ECProject::ec_init_tables(static_cast<int>(sources.size()), 1,
                                      coefficients.data(), tables.data());
            ECProject::ec_encode_data_avx2(block_size, static_cast<int>(sources.size()), 1,
                                           tables.data(), source_ptrs.data(), output);
            require(recovered == blocks[test_case.failed_block_id],
                    test_case.failed_name + " AVX2 recovery mismatch");

            std::vector<unsigned char> scalar(block_size, 0);
            for (size_t source = 0; source < sources.size(); ++source)
                for (int byte = 0; byte < block_size; ++byte)
                    scalar[byte] ^= ECProject::gf_mul(coefficients[source],
                                                      blocks[sources[source]][byte]);
            require(scalar == blocks[test_case.failed_block_id],
                    test_case.failed_name + " scalar recovery mismatch");
        }

        require(ECProject::get_recovery_group_and_block_ids("OLRC", k, r, z, 68).empty(),
                "unsupported failure produced a plan");
        require(ECProject::get_recovery_group_and_block_ids("OLRC", 96, 3, 7, 99).empty(),
                "unsupported parameters produced a plan");
        std::cout << "OLRC k64 L0/L4 single repair verification passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "OLRC k64 single repair verification failed: " << error.what() << '\n';
        return 1;
    }
}
