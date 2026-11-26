#include "encoder.h"
#include <algorithm>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void require(bool condition, const std::string &message) { if (!condition) throw std::runtime_error(message); }
std::vector<int> range(int begin, int end) { std::vector<int> result; for (int id = begin; id < end; ++id) result.push_back(id); return result; }
std::vector<int> flatten(const std::vector<std::pair<int, std::vector<int>>> &plan) { std::vector<int> result; for (const auto &entry : plan) result.insert(result.end(), entry.second.begin(), entry.second.end()); return result; }
}

int main()
{
    try {
        constexpr int k = 64, r = 3, z = 5, block_size = 4096;
        struct RepairCase { std::string name; int failed; std::vector<int> sources; };
        const std::vector<RepairCase> cases = {
            {"L0", 67, range(0, 21)}, {"L4", 71, range(42, 64)}, {"G0", 64, range(0, 64)}};
        std::vector<std::vector<unsigned char>> blocks(k + r + z, std::vector<unsigned char>(block_size));
        std::mt19937 generator(0x414c5243u);
        std::uniform_int_distribution<int> distribution(0, 255);
        for (int id = 0; id < k; ++id) for (auto &value : blocks[id]) value = static_cast<unsigned char>(distribution(generator));
        std::vector<unsigned char *> data(k), parity(r + z);
        for (int id = 0; id < k; ++id) data[id] = blocks[id].data();
        for (int id = 0; id < r + z; ++id) parity[id] = blocks[k + id].data();
        ECProject::encode_alrc(k, r, z, data.data(), parity.data(), block_size);

        for (const auto &test_case : cases) {
            const auto sources = flatten(ECProject::get_recovery_group_and_block_ids("ALRC", k, r, z, test_case.failed));
            require(sources == test_case.sources, test_case.name + " read set mismatch");
            require(std::find(sources.begin(), sources.end(), test_case.failed) == sources.end(), test_case.name + " source contains failure");
            std::vector<unsigned char> coefficients;
            require(ECProject::get_local_fill_plan(k, r, z, "ALRC", {test_case.failed}, sources, coefficients), test_case.name + " coefficient solve failed");
            require(coefficients.size() == sources.size(), test_case.name + " coefficient count mismatch");
            std::vector<unsigned char> direct(block_size, 0);
            for (size_t source = 0; source < sources.size(); ++source)
                for (int byte = 0; byte < block_size; ++byte)
                    direct[byte] ^= ECProject::gf_mul(coefficients[source], blocks[sources[source]][byte]);
            require(direct == blocks[test_case.failed], test_case.name + " direct recovery mismatch");
            std::vector<unsigned char *> source_ptrs;
            for (int source : sources) source_ptrs.push_back(blocks[source].data());
            std::vector<unsigned char> decoded(block_size, 0);
            ECProject::decode_alrc(k, r, z, static_cast<int>(sources.size()), &sources, source_ptrs.data(), decoded.data(), block_size, test_case.failed);
            require(decoded == blocks[test_case.failed], test_case.name + " decode mismatch");
        }
        for (int failed = 0; failed < k + r + z; ++failed)
            if (failed != 64 && failed != 67 && failed != 71)
                require(ECProject::get_recovery_group_and_block_ids("ALRC", k, r, z, failed).empty(), "unsupported failure produced a plan");
        require(ECProject::get_recovery_group_and_block_ids("ALRC", 96, 3, 7, 99).empty(), "unsupported parameters produced a plan");
        std::cout << "ALRC k64 L0/L4/G0 single repair verification passed" << std::endl;
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "ALRC k64 single repair verification failed: " << error.what() << std::endl;
        return 1;
    }
}
