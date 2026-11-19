#include "encoder.h"
#include "repair_benchmark_plan.h"

#include <algorithm>
#include <iostream>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace {
constexpr int k = 64;
constexpr int r = 3;
constexpr int z = 3;

void require(bool condition, const std::string &message)
{
    if (!condition) throw std::runtime_error(message);
}

std::vector<int> sources_for(int first, int last)
{
    std::vector<int> sources(static_cast<size_t>(last - first + 1));
    std::iota(sources.begin(), sources.end(), first);
    sources.insert(sources.end(), {64, 65, 66});
    return sources;
}

void verify(int block_size)
{
    const auto cases = ECProject::RepairBenchmark::make_optimal_k64_single_block_cases();
    const std::vector<ECProject::RepairBenchmark::RepairCase> expected = {
        {"L0", 67, sources_for(0, 20)},
        {"L2", 69, sources_for(42, 63)},
    };
    require(cases.size() == expected.size(), "benchmark must contain only L0 and L2");
    require(ECProject::get_optimal_lrc_local_group_bounds(k, z) ==
                std::vector<std::pair<int, int>>({{0, 21}, {21, 42}, {42, 64}}),
            "unexpected k64 local group boundaries");

    std::vector<std::vector<unsigned char>> blocks(
        k + r + z, std::vector<unsigned char>(static_cast<size_t>(block_size)));
    std::mt19937 generator(0x4f503634u + static_cast<unsigned>(block_size));
    std::uniform_int_distribution<int> distribution(0, 255);
    for (int id = 0; id < k; ++id)
        for (auto &value : blocks[id])
            value = static_cast<unsigned char>(distribution(generator));

    std::vector<unsigned char *> data(k), parity(r + z);
    for (int id = 0; id < k; ++id) data[id] = blocks[id].data();
    for (int id = 0; id < r + z; ++id) parity[id] = blocks[k + id].data();
    ECProject::encode_optimal_lrc(k, r, z, data.data(), parity.data(), block_size);
    const auto physical_groups = ECProject::get_optimal_lrc_block_id_to_group_id(k, r, z);
    std::vector<unsigned char> local_weights(k);
    ECProject::gf_gen_local_vector(local_weights.data(), k, r);

    for (size_t index = 0; index < cases.size(); ++index) {
        const auto &test_case = cases[index];
        const auto &wanted = expected[index];
        const std::string label = wanted.failed_name + " block_size=" + std::to_string(block_size);
        require(test_case.failed_name == wanted.failed_name &&
                    test_case.failed_block_id == wanted.failed_block_id,
                label + ": wrong benchmark case");
        require(test_case.source_block_ids == wanted.source_block_ids,
                label + ": wrong benchmark read set/order");

        const auto plan = ECProject::get_recovery_group_and_block_ids(
            "OptimalLRC", k, r, z, test_case.failed_block_id);
        std::vector<int> sources;
        for (const auto &group : plan) {
            for (int source : group.second) {
                require(physical_groups.at(source) == group.first,
                        label + ": source assigned to wrong physical group");
                sources.push_back(source);
            }
        }
        require(sources == wanted.source_block_ids, label + ": production read set/order mismatch");
        const std::unordered_set<int> unique_sources(sources.begin(), sources.end());
        require(unique_sources.size() == sources.size(), label + ": duplicate read");
        require(!unique_sources.count(test_case.failed_block_id), label + ": reads failed block");

        const auto original = blocks[test_case.failed_block_id];
        // Erase the failed parity before either recovery path runs.
        std::fill(blocks[test_case.failed_block_id].begin(),
                  blocks[test_case.failed_block_id].end(), 0);

        // Independent oracle: weighted local data XOR G0 XOR G1 XOR G2.
        std::vector<unsigned char> oracle(static_cast<size_t>(block_size), 0);
        for (int source : sources) {
            const unsigned char weight = source < k ? local_weights[source] : 1;
            for (int byte = 0; byte < block_size; ++byte)
                oracle[byte] ^= ECProject::gf_mul(weight, blocks[source][byte]);
        }
        require(oracle == original, label + ": weighted repair identity mismatch");

        std::vector<unsigned char> coefficients;
        require(ECProject::get_local_fill_plan(k, r, z, "OptimalLRC",
                                              {test_case.failed_block_id}, sources, coefficients),
                label + ": coefficient solve failed");
        require(coefficients.size() == sources.size(), label + ": coefficient count mismatch");
        std::vector<unsigned char> direct(static_cast<size_t>(block_size), 0);
        for (size_t column = 0; column < sources.size(); ++column)
            for (int byte = 0; byte < block_size; ++byte)
                direct[byte] ^= ECProject::gf_mul(coefficients[column], blocks[sources[column]][byte]);
        require(direct == original, label + ": exact-source recovery mismatch");

        // The production decoder must also handle source buffers in a different order.
        std::reverse(sources.begin(), sources.end());
        std::vector<unsigned char *> source_ptrs;
        for (int source : sources) source_ptrs.push_back(blocks[source].data());
        std::vector<unsigned char> decoded(static_cast<size_t>(block_size), 0xa5);
        ECProject::decode_optimal_lrc(k, r, z, static_cast<int>(sources.size()), &sources,
                                      source_ptrs.data(), decoded.data(), block_size,
                                      test_case.failed_block_id);
        require(decoded == original, label + ": production decoder mismatch");
        blocks[test_case.failed_block_id] = original;
        std::cout << label << ": passed (" << sources.size() << " source blocks)\n";
    }
}
} // namespace

int main()
{
    try {
        verify(4096);
        verify(1048576);
        std::cout << "OptimalLRC k64 L0/L2 single repair verification passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "OptimalLRC k64 single repair verification failed: " << error.what() << '\n';
        return 1;
    }
}
