#include "encoder.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <vector>

namespace {

std::vector<int> member_counts(int k, int r, int z)
{
    const int groups = (z + 1) / 2;
    std::vector<int> counts(groups, (k + r) / groups);
    const int remainder = (k + r) % groups;
    for (int group = groups - remainder; group < groups; ++group) ++counts[group];
    return counts;
}

void verify_case(int k, int r, int z)
{
    const int n = k + r + z;
    const int groups = (z + 1) / 2;
    const auto members = member_counts(k, r, z);
    std::vector<unsigned char> ul(static_cast<size_t>(n) * k);
    std::vector<unsigned char> uniform(static_cast<size_t>(k + r + groups) * k);
    std::vector<unsigned char> cauchy(static_cast<size_t>(k + r) * k);
    ECProject::gen_ul_matrix(ul.data(), k, r, z);
    ECProject::gen_uniform_lrc_matrix(uniform.data(), k, r, groups);
    ECProject::gf_gen_cauchy_matrix1(cauchy.data(), k + r, k);

    for (int row = 0; row < k + r; ++row)
        for (int col = 0; col < k; ++col)
        {
            assert(ul[static_cast<size_t>(row) * k + col] == cauchy[static_cast<size_t>(row) * k + col]);
            assert(ul[static_cast<size_t>(row) * k + col] == uniform[static_cast<size_t>(row) * k + col]);
        }

    int member_start = 0;
    for (int group = 0; group < groups; ++group)
    {
        const int first_local = 2 * group;
        std::vector<unsigned char> expected_first(k, 0), expected_second(k, 0);
        std::vector<std::vector<unsigned char>> columns;
        for (int offset = 0; offset < members[group]; ++offset)
        {
            const int member = member_start + offset;
            const unsigned char a = member < k
                ? ECProject::gf_inv(static_cast<unsigned char>(member ^ (k + r))) : 1;
            const unsigned char b = ECProject::gf_mul(a, ECProject::gff_base[offset]);
            if (member < k)
            {
                expected_first[member] ^= a;
                expected_second[member] ^= b;
            }
            else
                for (int col = 0; col < k; ++col)
                {
                    expected_first[col] ^= cauchy[static_cast<size_t>(member) * k + col];
                    expected_second[col] ^= ECProject::gf_mul(b, cauchy[static_cast<size_t>(member) * k + col]);
                }
            if (group != groups - 1) columns.push_back({a, b});
        }
        for (int col = 0; col < k; ++col)
        {
            assert(ul[static_cast<size_t>(k + r + first_local) * k + col] == expected_first[col]);
            assert(ul[static_cast<size_t>(k + r + first_local) * k + col] ==
                   uniform[static_cast<size_t>(k + r + group) * k + col]);
        }
        if (group != groups - 1)
        {
            for (int col = 0; col < k; ++col)
                assert(ul[static_cast<size_t>(k + r + first_local + 1) * k + col] == expected_second[col]);
            columns.push_back({1, 0});
            columns.push_back({0, 1});
            for (size_t i = 0; i < columns.size(); ++i)
                for (size_t j = i + 1; j < columns.size(); ++j)
                    assert((ECProject::gf_mul(columns[i][0], columns[j][1]) ^
                            ECProject::gf_mul(columns[j][0], columns[i][1])) != 0);
        }
        member_start += members[group];
    }

    const auto data = ECProject::get_data_block_num_per_group(k, r, z, "UL");
    const auto global = ECProject::get_global_parity_block_num_per_group(k, r, z, "UL");
    const auto local = ECProject::get_local_parity_block_num_per_group(k, r, z, "UL");
    assert(data.size() == static_cast<size_t>(groups));
    assert(global.size() == data.size() && local.size() == data.size());
    assert(std::accumulate(data.begin(), data.end(), 0) == k);
    assert(std::accumulate(global.begin(), global.end(), 0) == r);
    assert(std::accumulate(local.begin(), local.end(), 0) == z);
    for (int group = 0; group < groups; ++group)
    {
        assert(data[group] + global[group] == members[group]);
        assert(local[group] == (group == groups - 1 ? 1 : 2));
    }

    const auto placement = ECProject::get_ul_group_id_to_block_ids(k, r, z);
    std::vector<int> seen(n, 0);
    for (int group = 0; group < groups; ++group)
    {
        const auto &blocks = placement.at(group);
        assert(static_cast<int>(blocks.size()) == data[group] + global[group] + local[group]);
        int phase = 0;
        for (int block : blocks)
        {
            const int next_phase = block < k ? 0 : (block < k + r ? 1 : 2);
            assert(next_phase >= phase);
            phase = next_phase;
            ++seen[block];
            assert(ECProject::get_ul_block_id_to_local_group_id(k, r, z, block) == group);
        }
    }
    assert(std::all_of(seen.begin(), seen.end(), [](int count) { return count == 1; }));
}

void verify_invalid_parameters()
{
    std::vector<unsigned char> matrix(600 * 600);
    bool even_failed = false;
    try { ECProject::gen_ul_matrix(matrix.data(), 8, 2, 4); }
    catch (const std::invalid_argument &) { even_failed = true; }
    assert(even_failed);
    bool member_failed = false;
    try { ECProject::gen_ul_matrix(matrix.data(), 508, 4, 3); }
    catch (const std::invalid_argument &) { member_failed = true; }
    assert(member_failed);
    member_failed = false;
    try { (void)ECProject::get_ul_group_id_to_block_ids(508, 4, 3); }
    catch (const std::invalid_argument &) { member_failed = true; }
    assert(member_failed);
}

} // namespace

int main()
{
    verify_case(8, 3, 3);
    verify_case(17, 8, 7);
    verify_case(8, 9, 7);
    verify_invalid_parameters();
    std::cout << "UL matrix and placement tests passed" << std::endl;
    return 0;
}
