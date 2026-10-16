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

std::vector<int> member_group_sizes(int k, int r, int z)
{
    const int member_count = k + r - 1;
    const int base = member_count / z;
    const int remainder = member_count % z;
    std::vector<int> sizes;
    for (int group = 0; group < z; ++group)
        sizes.push_back(base + (group >= z - remainder ? 1 : 0));
    return sizes;
}

void verify_case(int k, int r, int z)
{
    const int n = k + r + z;
    const int m = k + r;
    std::vector<unsigned char> uniform(static_cast<size_t>(n) * k);
    std::vector<unsigned char> cp(static_cast<size_t>(n) * k);
    ECProject::gen_uniform_lrc_matrix(uniform.data(), k, r, z);
    ECProject::gen_cp_ulrc_matrix(cp.data(), k, r, z);

    assert(std::equal(uniform.begin(), uniform.begin() + static_cast<ptrdiff_t>(m * k), cp.begin()));

    std::vector<unsigned char> data_coefficients(static_cast<size_t>(k));
    for (int data = 0; data < k; ++data) {
        unsigned char coefficient = cp[(k + r - 1) * k + data];
        for (int global = 0; global < r - 1; ++global)
            coefficient ^= cp[(k + global) * k + data];
        data_coefficients[data] = coefficient;
    }

    const std::vector<int> sizes = member_group_sizes(k, r, z);
    int member_begin = 0;
    for (int group = 0; group < z; ++group) {
        const int member_end = member_begin + sizes[group];
        for (int data = 0; data < k; ++data) {
            unsigned char expected = 0;
            if (data >= member_begin && data < member_end)
                expected ^= data_coefficients[data];
            for (int global = 0; global < r - 1; ++global) {
                const int member = k + global;
                if (member >= member_begin && member < member_end)
                    expected ^= cp[(k + global) * k + data];
            }
            assert(cp[(m + group) * k + data] == expected);
        }
        member_begin = member_end;
    }
    assert(member_begin == k + r - 1);

    for (int data = 0; data < k; ++data) {
        unsigned char coefficient = 0;
        for (int group = 0; group < z; ++group)
            coefficient ^= cp[(m + group) * k + data];
        assert(coefficient == cp[(k + r - 1) * k + data]);
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
    ECProject::encode_cp_ulrc(k, r, z, data_ptrs.data(), parity_ptrs.data(), block_size);

    member_begin = 0;
    for (int group = 0; group < z; ++group) {
        const int member_end = member_begin + sizes[group];
        std::vector<unsigned char> expected(block_size, 0);
        for (int member = member_begin; member < member_end; ++member) {
            if (member < k) {
                for (int byte = 0; byte < block_size; ++byte)
                    expected[byte] ^= ECProject::gf_mul(data_coefficients[member], data[member][byte]);
            } else {
                const int global = member - k;
                for (int byte = 0; byte < block_size; ++byte)
                    expected[byte] ^= parity[global][byte];
            }
        }
        assert(expected == parity[r + group]);
        member_begin = member_end;
    }

    for (int byte = 0; byte < block_size; ++byte) {
        unsigned char local_xor = 0;
        for (int group = 0; group < z; ++group)
            local_xor ^= parity[r + group][byte];
        assert(local_xor == parity[r - 1][byte]);
    }
    for (int erased = 0; erased <= z; ++erased) {
        const auto &expected = erased == z ? parity[r - 1] : parity[r + erased];
        std::vector<unsigned char> rebuilt(block_size, 0);
        for (int member = 0; member <= z; ++member) {
            if (member == erased)
                continue;
            const auto &source = member == z ? parity[r - 1] : parity[r + member];
            for (int byte = 0; byte < block_size; ++byte)
                rebuilt[byte] ^= source[byte];
        }
        assert(rebuilt == expected);
    }

    const auto groups = ECProject::get_cp_ulrc_group_id_to_block_ids(k, r, z);
    const auto reverse = ECProject::get_cp_ulrc_block_id_to_group_id(k, r, z);
    const auto data_counts = ECProject::get_data_block_num_per_group_cp_ulrc(k, r, z);
    const auto global_counts = ECProject::get_global_parity_block_num_per_group_cp_ulrc(k, r, z);
    const auto local_counts = ECProject::get_local_parity_block_num_per_group_cp_ulrc(k, r, z);
    assert(static_cast<int>(groups.size()) == z + 1);
    assert(static_cast<int>(reverse.size()) == n);
    assert(static_cast<int>(data_counts.size()) == z + 1);
    assert(static_cast<int>(global_counts.size()) == z + 1);
    assert(static_cast<int>(local_counts.size()) == z + 1);

    std::unordered_set<int> seen;
    member_begin = 0;
    for (int group = 0; group < z; ++group) {
        const auto &blocks = groups.at(group);
        assert(static_cast<int>(blocks.size()) == sizes[group] + 1);
        for (int offset = 0; offset < sizes[group]; ++offset)
            assert(blocks[offset] == member_begin + offset);
        assert(blocks.back() == k + r + group);
        assert(data_counts[group] + global_counts[group] == sizes[group]);
        assert(local_counts[group] == 1);
        for (int block : blocks) {
            assert(seen.insert(block).second);
            assert(reverse.at(block) == group);
            assert(ECProject::get_cp_ulrc_block_id_to_local_group_id(k, r, z, block) == group);
        }
        member_begin += sizes[group];
    }
    assert(groups.at(z) == std::vector<int>{k + r - 1});
    assert(data_counts[z] == 0 && global_counts[z] == 1 && local_counts[z] == 0);
    assert(seen.insert(k + r - 1).second);
    assert(reverse.at(k + r - 1) == z);
    assert(ECProject::get_cp_ulrc_block_id_to_local_group_id(k, r, z, k + r - 1) == -1);
    assert(static_cast<int>(seen.size()) == n);
}

void verify_invalid(int k, int r, int z)
{
    std::vector<unsigned char> matrix(64);
    bool rejected = false;
    try {
        ECProject::gen_cp_ulrc_matrix(matrix.data(), k, r, z);
    } catch (const std::invalid_argument &) {
        rejected = true;
    }
    assert(rejected);
    rejected = false;
    try {
        (void)ECProject::get_cp_ulrc_group_id_to_block_ids(k, r, z);
    } catch (const std::invalid_argument &) {
        rejected = true;
    }
    assert(rejected);
}

} // namespace

int main()
{
    verify_case(8, 2, 2);
    verify_case(10, 3, 4);
    verify_case(3, 8, 5);
    verify_case(4, 1, 3);
    verify_case(3, 2, 4);
    verify_invalid(3, 2, 5);
    verify_invalid(0, 2, 1);
    verify_invalid(4, 0, 2);
    verify_invalid(4, 2, 0);
    std::cout << "CP-ULRC tests passed" << std::endl;
    return 0;
}
