#include "encoder.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <numeric>
#include <set>
#include <vector>

namespace {

void verify_case(int k, int r, int z)
{
    const int n = k + r + z;
    const int groups = (z + 1) / 2;
    const int data_per_group = k / groups;
    std::vector<unsigned char> al(static_cast<size_t>(n) * k);
    std::vector<unsigned char> azure(static_cast<size_t>(n) * k);
    ECProject::gen_al_matrix(al.data(), k, r, z);
    ECProject::gen_azure_lrc_matrix(azure.data(), k, r, groups);

    for (int row = 0; row < k + r; ++row)
        for (int col = 0; col < k; ++col)
            assert(al[static_cast<size_t>(row) * k + col] == azure[static_cast<size_t>(row) * k + col]);

    for (int group = 0; group < groups; ++group)
    {
        const int first_row = k + r + 2 * group;
        const int start = group * data_per_group;
        for (int col = 0; col < k; ++col)
            assert(al[static_cast<size_t>(first_row) * k + col] ==
                   static_cast<unsigned char>(col >= start && col < start + data_per_group));

        if (group == groups - 1)
            continue;
        const int second_row = first_row + 1;
        std::set<unsigned char> coefficients;
        for (int col = 0; col < k; ++col)
        {
            const unsigned char value = al[static_cast<size_t>(second_row) * k + col];
            if (col >= start && col < start + data_per_group)
            {
                assert(value != 0);
                assert(coefficients.insert(value).second);
            }
            else
                assert(value == 0);
        }

        std::vector<std::vector<unsigned char>> columns;
        for (int col = start; col < start + data_per_group; ++col)
            columns.push_back({1, al[static_cast<size_t>(second_row) * k + col]});
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

    const auto data_counts = ECProject::get_data_block_num_per_group(k, r, z, "AL");
    const auto global_counts = ECProject::get_global_parity_block_num_per_group(k, r, z, "AL");
    const auto local_counts = ECProject::get_local_parity_block_num_per_group(k, r, z, "AL");
    assert(data_counts.size() == static_cast<size_t>(groups + 1));
    assert(global_counts.size() == data_counts.size());
    assert(local_counts.size() == data_counts.size());
    assert(std::accumulate(data_counts.begin(), data_counts.end(), 0) == k);
    assert(std::accumulate(global_counts.begin(), global_counts.end(), 0) == r);
    assert(std::accumulate(local_counts.begin(), local_counts.end(), 0) == z);
    for (int group = 0; group < groups - 1; ++group) assert(local_counts[group] == 2);
    assert(local_counts[groups - 1] == 1);
    assert(global_counts.back() == r);

    const auto placement = ECProject::get_al_group_id_to_block_ids(k, r, z);
    assert(placement.size() == static_cast<size_t>(groups + 1));
    std::vector<int> seen(n, 0);
    for (int group = 0; group <= groups; ++group)
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

} // namespace

int main()
{
    verify_case(8, 3, 3);
    verify_case(16, 4, 7);
    std::cout << "AL matrix and placement tests passed" << std::endl;
    return 0;
}
