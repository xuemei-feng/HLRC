#include "encoder.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <iostream>
#include <random>
#include <stdexcept>
#include <unordered_set>
#include <vector>

namespace {

void verify_case(int k, int r, int z)
{
    const int n = k + r + z;
    const int group_size = k / z;
    std::vector<unsigned char> azure(static_cast<size_t>(n) * k);
    std::vector<unsigned char> cp(static_cast<size_t>(n) * k);
    ECProject::gen_azure_lrc_matrix(azure.data(), k, r, z);
    ECProject::gen_cp_alrc_matrix(cp.data(), k, r, z);

    assert(std::equal(azure.begin(), azure.begin() + static_cast<ptrdiff_t>((k + r) * k), cp.begin()));
    for (int group = 0; group < z; ++group) {
        for (int data = 0; data < k; ++data) {
            const unsigned char coefficient = cp[(k + r + group) * k + data];
            const bool in_group = data / group_size == group;
            assert((coefficient != 0) == in_group);
            assert(coefficient == (in_group ? cp[(k + r - 1) * k + data] : 0));
        }
    }
    for (int data = 0; data < k; ++data) {
        unsigned char local_xor = 0;
        for (int group = 0; group < z; ++group)
            local_xor ^= cp[(k + r + group) * k + data];
        assert(local_xor == cp[(k + r - 1) * k + data]);
    }

    constexpr int block_size = 4096;
    std::mt19937 rng(static_cast<unsigned int>(k * 100 + r * 10 + z));
    std::uniform_int_distribution<int> byte_dist(0, 255);
    std::vector<std::vector<unsigned char>> data(k, std::vector<unsigned char>(block_size));
    std::vector<std::vector<unsigned char>> parity(r + z, std::vector<unsigned char>(block_size));
    std::vector<unsigned char *> data_ptrs(k), parity_ptrs(r + z);
    for (int i = 0; i < k; ++i) {
        std::generate(data[i].begin(), data[i].end(), [&] { return static_cast<unsigned char>(byte_dist(rng)); });
        data_ptrs[i] = data[i].data();
    }
    for (int i = 0; i < r + z; ++i)
        parity_ptrs[i] = parity[i].data();
    ECProject::encode_cp_alrc(k, r, z, data_ptrs.data(), parity_ptrs.data(), block_size);

    for (int byte = 0; byte < block_size; ++byte) {
        unsigned char value = parity[r - 1][byte];
        for (int group = 0; group < z; ++group)
            value ^= parity[r + group][byte];
        assert(value == 0);
    }
    for (int erased = 0; erased <= z; ++erased) {
        const std::vector<unsigned char> &expected = erased == z ? parity[r - 1] : parity[r + erased];
        std::vector<unsigned char> rebuilt(block_size, 0);
        for (int member = 0; member <= z; ++member) {
            if (member == erased)
                continue;
            const std::vector<unsigned char> &source = member == z ? parity[r - 1] : parity[r + member];
            for (int byte = 0; byte < block_size; ++byte)
                rebuilt[byte] ^= source[byte];
        }
        assert(rebuilt == expected);
    }

    const auto by_group = ECProject::get_cp_alrc_group_id_to_block_ids(k, r, z);
    const auto by_block = ECProject::get_cp_alrc_block_id_to_group_id(k, r, z);
    assert(static_cast<int>(by_block.size()) == n);
    std::unordered_set<int> seen;
    for (const auto &entry : by_group)
        for (int block : entry.second) {
            assert(seen.insert(block).second);
            assert(by_block.at(block) == entry.first);
        }
    assert(static_cast<int>(seen.size()) == n);
}

} // namespace

int main()
{
    verify_case(8, 2, 2);
    verify_case(12, 3, 4);

    std::vector<unsigned char> invalid(32 * 32);
    bool rejected = false;
    try {
        ECProject::gen_cp_alrc_matrix(invalid.data(), 10, 3, 4);
    } catch (const std::invalid_argument &) {
        rejected = true;
    }
    assert(rejected);
    rejected = false;
    try {
        (void)ECProject::get_cp_alrc_block_id_to_group_id(10, 3, 4);
    } catch (const std::invalid_argument &) {
        rejected = true;
    }
    assert(rejected);
    std::cout << "CP-ALRC tests passed" << std::endl;
    return 0;
}
