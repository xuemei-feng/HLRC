#include "encoder.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <numeric>
#include <tuple>
#include <string>
#include <vector>

namespace {

bool verify_case(int k, int r, int z)
{
    const int n = k + r + z;
    const int local_group_num = (k == 24) ? 2 : 4;
    const int non_coupled_num = local_group_num / 2;
    const int coupled_num = local_group_num - non_coupled_num;

    std::vector<unsigned char> matrix(static_cast<size_t>(n) * k, 0);
    ECProject::gen_hlrc_matrix(matrix.data(), k, r, z);

    for (int row = 0; row < k; ++row)
        for (int col = 0; col < k; ++col)
            if (matrix[static_cast<size_t>(row) * k + col] !=
                static_cast<unsigned char>(row == col))
                return false;

    std::vector<unsigned char> base(static_cast<size_t>(k + r + 2) * k, 0);
    ECProject::gf_gen_cauchy_matrix1(base.data(), k + r + 2, k);
    for (int row = 0; row < r; ++row)
        for (int col = 0; col < k; ++col)
            if (matrix[static_cast<size_t>(k + row) * k + col] !=
                base[static_cast<size_t>(k + row) * k + col])
                return false;

    const unsigned char *seed0 = base.data() + static_cast<size_t>(k + r) * k;
    const unsigned char *seed1 = seed0 + k;
    int data_begin = 0;
    std::vector<int> global_counts(coupled_num, 0);
    for (int group = 0; group < local_group_num; ++group)
    {
        const int data_count = k / local_group_num + (group < k % local_group_num ? 1 : 0);
        const int row0 = k + r + 2 * group;
        const int row1 = row0 + 1;
        std::vector<unsigned char> expected0(k, 0), expected1(k, 0);

        if (group < non_coupled_num)
        {
            const int first_half = data_count / 2 + data_count % 2;
            for (int col = data_begin; col < data_begin + first_half; ++col)
                expected0[col] = seed0[col] ^ seed1[col];
            for (int col = data_begin + first_half; col < data_begin + data_count; ++col)
                expected1[col] = seed0[col] ^ seed1[col];
        }
        else
        {
            const int coupled_index = group - non_coupled_num;
            for (int col = data_begin; col < data_begin + data_count; ++col)
            {
                expected0[col] = seed0[col];
                expected1[col] = seed1[col];
            }
            for (int gp = coupled_index; gp < r; gp += coupled_num)
            {
                ++global_counts[coupled_index];
                const unsigned char *global = matrix.data() + static_cast<size_t>(k + gp) * k;
                for (int col = 0; col < k; ++col)
                {
                    expected0[col] ^= global[col];
                    expected1[col] ^= global[col];
                }
            }
        }

        for (int col = 0; col < k; ++col)
            if (matrix[static_cast<size_t>(row0) * k + col] != expected0[col] ||
                matrix[static_cast<size_t>(row1) * k + col] != expected1[col])
                return false;
        data_begin += data_count;
    }

    if (data_begin != k)
        return false;
    if (*std::max_element(global_counts.begin(), global_counts.end()) -
            *std::min_element(global_counts.begin(), global_counts.end()) > 1)
        return false;
    if (std::accumulate(global_counts.begin(), global_counts.end(), 0) != r)
        return false;

    constexpr int block_size = 16; // Forces the portable GF path; no AVX dependency in this test.
    std::vector<std::vector<unsigned char>> blocks(n, std::vector<unsigned char>(block_size));
    for (int i = 0; i < k; ++i)
        for (int byte = 0; byte < block_size; ++byte)
            blocks[i][byte] = static_cast<unsigned char>((i * 31 + byte * 17) & 0xff);
    std::vector<unsigned char *> data(k), parity(r + z);
    for (int i = 0; i < k; ++i) data[i] = blocks[i].data();
    for (int i = 0; i < r + z; ++i) parity[i] = blocks[k + i].data();
    ECProject::encode_hlrc(k, r, z, data.data(), parity.data(), block_size);

    for (int out = 0; out < r + z; ++out)
        for (int byte = 0; byte < block_size; ++byte)
        {
            unsigned char expected = 0;
            for (int src = 0; src < k; ++src)
                expected ^= ECProject::gf_mul(
                    matrix[static_cast<size_t>(k + out) * k + src], blocks[src][byte]);
            if (parity[out][byte] != expected)
                return false;
        }

    const auto data_counts = ECProject::get_data_block_num_per_group(k, r, z, "HLRC");
    const auto global_counts_layout = ECProject::get_global_parity_block_num_per_group(k, r, z, "HLRC");
    const auto local_counts = ECProject::get_local_parity_block_num_per_group(k, r, z, "HLRC");
    return data_counts == std::vector<int>{k} &&
           global_counts_layout == std::vector<int>{r} &&
           local_counts == std::vector<int>{z} &&
           ECProject::get_hlrc_block_id_to_group_id(k, r, z).size() == static_cast<size_t>(n);
}

bool rejects(int k, int r, int z)
{
    try
    {
        std::vector<unsigned char> matrix(static_cast<size_t>(std::max(1, k + r + z)) *
                                          static_cast<size_t>(std::max(1, k)));
        ECProject::gen_hlrc_matrix(matrix.data(), k, r, z);
        return false;
    }
    catch (const std::invalid_argument &)
    {
        return true;
    }
}

} // namespace

int main()
{
    const std::vector<std::tuple<int, int, int>> cases = {
        {24, 1, 4},   // Two local groups.
        {25, 5, 8},   // Uneven data and uneven global distribution across two coupled groups.
        {31, 6, 8},   // Uneven data with evenly distributed globals.
        {103, 9, 8},  // Exactly 120 total blocks.
    };

    for (const auto &[k, r, z] : cases)
    {
        if (!verify_case(k, r, z))
        {
            std::cerr << "HLRC verification failed for (" << k << "," << r << "," << z << ")\n";
            return 1;
        }
    }

    if (!rejects(24, 1, 8) || !rejects(25, 1, 4) || !rejects(104, 9, 8) ||
        !rejects(0, 1, 8) || !rejects(25, 0, 8))
    {
        std::cerr << "HLRC invalid-parameter validation failed\n";
        return 2;
    }

    std::cout << "HLRC generalized matrix and encoding verification passed\n";
    return 0;
}
