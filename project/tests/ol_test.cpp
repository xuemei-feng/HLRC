#include "encoder.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <vector>

namespace {

void verify_case(int k, int r, int z)
{
    const int n = k + r + z;
    const int group_num = (z + 1) / 2;
    std::vector<unsigned char> ol(static_cast<size_t>(n) * k);
    std::vector<unsigned char> cauchy(static_cast<size_t>(k + r) * k);
    ECProject::gen_ol_matrix(ol.data(), k, r, z);
    ECProject::gf_gen_cauchy_matrix1(cauchy.data(), k + r, k);

    for (int row = 0; row < k + r; ++row)
        for (int col = 0; col < k; ++col)
            assert(ol[static_cast<size_t>(row) * k + col] == cauchy[static_cast<size_t>(row) * k + col]);

    const auto data_counts = ECProject::get_data_block_num_per_group_ol(k, r, z);
    int data_start = 0;
    for (int group = 0; group < group_num; ++group)
    {
        const int count = data_counts[group];
        const int first_local = 2 * group;
        std::vector<unsigned char> expected(k, 0);
        for (int offset = 0; offset < count; ++offset)
        {
            const int data_index = data_start + offset;
            expected[data_index] = ECProject::gf_inv(static_cast<unsigned char>(data_index ^ (k + r)));
        }
        for (int global = 0; global < r; ++global)
            for (int col = 0; col < k; ++col)
                expected[col] ^= cauchy[static_cast<size_t>(k + global) * k + col];
        for (int col = 0; col < k; ++col)
            assert(ol[static_cast<size_t>(k + r + first_local) * k + col] == expected[col]);

        if (group != group_num - 1)
        {
            std::vector<unsigned char> expected_second(k, 0);
            for (int offset = 0; offset < count; ++offset)
            {
                const int data_index = data_start + offset;
                const unsigned char a = ECProject::gf_inv(static_cast<unsigned char>(data_index ^ (k + r)));
                expected_second[data_index] = ECProject::gf_mul(a, ECProject::gff_base[offset]);
            }
            for (int global = 0; global < r; ++global)
                for (int col = 0; col < k; ++col)
                    expected_second[col] ^= ECProject::gf_mul(
                        ECProject::gff_base[count + global],
                        cauchy[static_cast<size_t>(k + global) * k + col]);
            for (int col = 0; col < k; ++col)
                assert(ol[static_cast<size_t>(k + r + first_local + 1) * k + col] == expected_second[col]);

            std::vector<std::vector<unsigned char>> columns;
            for (int offset = 0; offset < count; ++offset)
            {
                const int data_index = data_start + offset;
                const unsigned char a = ECProject::gf_inv(static_cast<unsigned char>(data_index ^ (k + r)));
                const unsigned char b = ECProject::gf_mul(a, ECProject::gff_base[offset]);
                assert(a != 0 && b != 0);
                columns.push_back({a, b});
            }
            for (int global = 0; global < r; ++global)
            {
                const unsigned char b = ECProject::gff_base[count + global];
                assert(b != 0);
                columns.push_back({1, b});
            }
            columns.push_back({1, 0});
            columns.push_back({0, 1});
            for (size_t i = 0; i < columns.size(); ++i)
                for (size_t j = i + 1; j < columns.size(); ++j)
                {
                    const unsigned char determinant =
                        ECProject::gf_mul(columns[i][0], columns[j][1]) ^
                        ECProject::gf_mul(columns[j][0], columns[i][1]);
                    assert(determinant != 0);
                }
        }
        data_start += count;
    }

    const auto global_counts = ECProject::get_global_parity_block_num_per_group_ol(k, r, z);
    const auto local_counts = ECProject::get_local_parity_block_num_per_group_ol(k, r, z);
    assert(data_counts.size() == static_cast<size_t>(group_num + 1));
    assert(global_counts.size() == data_counts.size());
    assert(local_counts.size() == data_counts.size());
    assert(std::accumulate(data_counts.begin(), data_counts.end(), 0) == k);
    assert(std::accumulate(global_counts.begin(), global_counts.end(), 0) == r);
    assert(std::accumulate(local_counts.begin(), local_counts.end(), 0) == z);
    for (int group = 0; group < group_num - 1; ++group) assert(local_counts[group] == 2);
    assert(local_counts[group_num - 1] == 1);
    assert(global_counts.back() == r);

    const auto placement = ECProject::get_ol_group_id_to_block_ids(k, r, z);
    std::vector<int> seen(n, 0);
    for (int group = 0; group <= group_num; ++group)
    {
        const auto &blocks = placement.at(group);
        assert(static_cast<int>(blocks.size()) == data_counts[group] + global_counts[group] + local_counts[group]);
        for (int block : blocks)
        {
            assert(block >= 0 && block < n);
            ++seen[block];
        }
    }
    assert(std::all_of(seen.begin(), seen.end(), [](int count) { return count == 1; }));
}

void verify_divisible_matches_optimal()
{
    const int k = 8, r = 3, z = 3, groups = 2;
    std::vector<unsigned char> ol(static_cast<size_t>(k + r + z) * k);
    std::vector<unsigned char> optimal(static_cast<size_t>(k + r + groups) * k);
    ECProject::gen_ol_matrix(ol.data(), k, r, z);
    ECProject::gen_optimal_lrc_matrix(optimal.data(), k, r, groups);
    for (int row = k; row < k + r; ++row)
        for (int col = 0; col < k; ++col)
            assert(ol[static_cast<size_t>(row) * k + col] == optimal[static_cast<size_t>(row) * k + col]);
    for (int group = 0; group < groups; ++group)
        for (int col = 0; col < k; ++col)
            assert(ol[static_cast<size_t>(k + r + 2 * group) * k + col] ==
                   optimal[static_cast<size_t>(k + r + group) * k + col]);
}

void verify_invalid_parameters()
{
    std::vector<unsigned char> matrix(600 * 600);
    bool even_failed = false;
    try { ECProject::gen_ol_matrix(matrix.data(), 8, 2, 4); }
    catch (const std::invalid_argument &) { even_failed = true; }
    assert(even_failed);
    bool member_failed = false;
    try { ECProject::gen_ol_matrix(matrix.data(), 507, 3, 3); }
    catch (const std::invalid_argument &) { member_failed = true; }
    assert(member_failed);
}

} // namespace

int main()
{
    verify_case(8, 3, 3);
    verify_case(17, 4, 7);
    verify_divisible_matches_optimal();
    verify_invalid_parameters();
    std::cout << "OL matrix and placement tests passed" << std::endl;
    return 0;
}
