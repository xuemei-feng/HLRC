#include "encoder.h"
#include "repair_benchmark_plan.h"

#include <iostream>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void require(bool condition, const std::string &message)
{
    if (!condition)
        throw std::runtime_error(message);
}

std::vector<int> range(int begin, int end)
{
    std::vector<int> result(static_cast<size_t>(end - begin));
    std::iota(result.begin(), result.end(), begin);
    return result;
}

std::vector<int> flatten(const std::vector<std::pair<int, std::vector<int>>> &plan)
{
    std::vector<int> result;
    for (const auto &entry : plan)
        result.insert(result.end(), entry.second.begin(), entry.second.end());
    return result;
}

} // namespace

int main()
{
    try {
        constexpr int k = 128;
        constexpr int r = 3;
        constexpr int z = 5;
        constexpr int block_size = 4096;
        constexpr int g0_id = 128;
        constexpr int l0_id = 131;
        constexpr int l2_id = 133;

        const auto benchmark_cases =
            ECProject::RepairBenchmark::make_azure_uneven_single_block_cases(k, r, z);
        require(benchmark_cases.size() == 6, "unexpected AzureLRC k128 case count");
        require(benchmark_cases[0].failed_name == "L0" &&
                    benchmark_cases[0].failed_block_id == l0_id &&
                    benchmark_cases[0].source_block_ids == range(0, 25),
                "L0 must read D0-D24");
        require(benchmark_cases[2].failed_name == "L2" &&
                    benchmark_cases[2].failed_block_id == l2_id &&
                    benchmark_cases[2].source_block_ids == range(50, 76),
                "L2 must read D50-D75");
        require(benchmark_cases[5].failed_name == "G0" &&
                    benchmark_cases[5].failed_block_id == g0_id &&
                    benchmark_cases[5].source_block_ids == range(0, 128),
                "G0 must read D0-D127");

        std::vector<std::vector<unsigned char>> blocks(
            k + r + z, std::vector<unsigned char>(block_size));
        std::mt19937 generator(0x415a3132u);
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

        const std::vector<ECProject::RepairBenchmark::RepairCase> cases = {
            benchmark_cases[0], benchmark_cases[2], benchmark_cases[5]};
        const auto group_by_block = ECProject::get_azurelrc_block_id_to_group_id(k, r, z);

        for (const auto &test_case : cases) {
            const auto plan = ECProject::get_recovery_group_and_block_ids(
                "AzureLRC", k, r, z, test_case.failed_block_id);
            const auto sources = flatten(plan);
            require(sources == test_case.source_block_ids,
                    test_case.failed_name + " recovery read set mismatch");
            for (const auto &entry : plan)
                for (int source : entry.second)
                    require(group_by_block.at(source) == entry.first,
                            test_case.failed_name + " source assigned to wrong physical group");

            std::vector<unsigned char> coefficients;
            require(ECProject::get_local_fill_plan(k, r, z, "AzureLRC",
                                                   {test_case.failed_block_id}, sources,
                                                   coefficients),
                    test_case.failed_name + " coefficient solve failed");
            require(coefficients.size() == sources.size(),
                    test_case.failed_name + " coefficient count mismatch");

            std::vector<unsigned char> direct(block_size, 0);
            for (size_t i = 0; i < sources.size(); ++i)
                for (int byte = 0; byte < block_size; ++byte)
                    direct[byte] ^=
                        ECProject::gf_mul(coefficients[i], blocks[sources[i]][byte]);
            require(direct == blocks[test_case.failed_block_id],
                    test_case.failed_name + " direct recovery mismatch");

            std::vector<unsigned char *> source_ptrs;
            for (int source : sources)
                source_ptrs.push_back(blocks[source].data());
            std::vector<unsigned char> decoded(block_size, 0);
            ECProject::decode_azure_lrc(k, r, z, static_cast<int>(sources.size()),
                                        &sources, source_ptrs.data(), decoded.data(),
                                        block_size, test_case.failed_block_id);
            require(decoded == blocks[test_case.failed_block_id],
                    test_case.failed_name + " decode_azure_lrc mismatch");
        }

        std::cout << "AzureLRC k128 L0/L2/G0 single repair verification passed"
                  << std::endl;
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "AzureLRC k128 single repair verification failed: "
                  << error.what() << std::endl;
        return 1;
    }
}
