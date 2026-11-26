#include "encoder.h"

#include <algorithm>
#include <iostream>
#include <random>
#include <stdexcept>
#include <vector>

namespace {
void require(bool value, const char *message)
{
    if (!value) throw std::runtime_error(message);
}

int rank(std::vector<unsigned char> matrix, int rows, int columns)
{
    int pivot_row = 0;
    for (int column = 0; column < columns && pivot_row < rows; ++column) {
        int candidate = pivot_row;
        while (candidate < rows && matrix[candidate * columns + column] == 0) ++candidate;
        if (candidate == rows) continue;
        for (int current = 0; current < columns; ++current)
            std::swap(matrix[pivot_row * columns + current],
                      matrix[candidate * columns + current]);
        const unsigned char inverse = ECProject::gf_inv(matrix[pivot_row * columns + column]);
        for (int current = column; current < columns; ++current)
            matrix[pivot_row * columns + current] =
                ECProject::gf_mul(matrix[pivot_row * columns + current], inverse);
        for (int row = 0; row < rows; ++row) {
            if (row == pivot_row) continue;
            const unsigned char factor = matrix[row * columns + column];
            if (factor == 0) continue;
            for (int current = column; current < columns; ++current)
                matrix[row * columns + current] ^=
                    ECProject::gf_mul(factor, matrix[pivot_row * columns + current]);
        }
        ++pivot_row;
    }
    return pivot_row;
}

void verify_local_double_erasure(const std::vector<unsigned char> &matrix, int k,
                                 int begin, int end, int local0, int local1)
{
    std::vector<int> members;
    for (int data = begin; data < end; ++data) members.push_back(data);
    members.push_back(local0);
    members.push_back(local1);
    const int dimension = end - begin;
    for (size_t first = 0; first < members.size(); ++first) {
        for (size_t second = first + 1; second < members.size(); ++second) {
            std::vector<unsigned char> survivors;
            for (size_t index = 0; index < members.size(); ++index) {
                if (index == first || index == second) continue;
                const unsigned char *row = matrix.data() + static_cast<size_t>(members[index]) * k;
                survivors.insert(survivors.end(), row + begin, row + end);
            }
            require(rank(std::move(survivors), dimension, dimension) == dimension,
                    "same-group double erasure is not recoverable");
        }
    }
}
}

void verify_case(int k, int r, int z, const std::vector<std::pair<int, int>> &bounds)
{
    const int n = k + r + z;
    std::vector<unsigned char> matrix(static_cast<size_t>(n) * k);
    ECProject::gen_alrc_matrix(matrix.data(), k, r, z);

    for (int row = 0; row < k; ++row)
        for (int column = 0; column < k; ++column)
            require(matrix[static_cast<size_t>(row) * k + column] == (row == column),
                    "matrix is not systematic");

    std::vector<unsigned char> rs(static_cast<size_t>(k + r) * k);
    ECProject::gf_gen_rs_matrix1(rs.data(), k + r, k);
    require(std::equal(matrix.begin() + static_cast<size_t>(k) * k,
                       matrix.begin() + static_cast<size_t>(k + r) * k,
                       rs.begin() + static_cast<size_t>(k) * k),
            "global parity rows differ from RS rows");

    int local = 0;
    for (size_t group = 0; group < bounds.size(); ++group) {
        const int parity_count = group + 1 < bounds.size() ? 2 : 1;
        for (int parity = 0; parity < parity_count; ++parity, ++local)
            for (int column = 0; column < k; ++column) {
                const bool in_group = column >= bounds[group].first && column < bounds[group].second;
                require((matrix[static_cast<size_t>(k + r + local) * k + column] != 0) == in_group,
                        "local parity support mismatch");
            }
        if (parity_count == 2)
            verify_local_double_erasure(matrix, k, bounds[group].first, bounds[group].second,
                                        k + r + local - 2, k + r + local - 1);
    }
    require(local == z, "local parity count mismatch");

    constexpr int block_size = 257;
    std::mt19937 generator(0x41524c43u + static_cast<unsigned int>(k));
    std::vector<std::vector<unsigned char>> blocks(n, std::vector<unsigned char>(block_size));
    for (int data = 0; data < k; ++data)
        for (auto &byte : blocks[data]) byte = static_cast<unsigned char>(generator());
    std::vector<unsigned char *> data_ptrs(k), parity_ptrs(r + z);
    for (int data = 0; data < k; ++data) data_ptrs[data] = blocks[data].data();
    for (int parity = 0; parity < r + z; ++parity)
        parity_ptrs[parity] = blocks[k + parity].data();
    ECProject::encode_alrc(k, r, z, data_ptrs.data(), parity_ptrs.data(), block_size);
    for (int row = k; row < n; ++row)
        for (int position = 0; position < block_size; ++position) {
            unsigned char expected = 0;
            for (int column = 0; column < k; ++column)
                expected ^= ECProject::gf_mul(matrix[static_cast<size_t>(row) * k + column],
                                              blocks[column][position]);
            require(blocks[row][position] == expected,
                    "encoded bytes do not match generator matrix");
        }
}

int main()
{
    try {
        verify_case(64, 3, 5, {{0, 21}, {21, 42}, {42, 64}});
        verify_case(96, 3, 7, {{0, 24}, {24, 48}, {48, 72}, {72, 96}});
        verify_case(128, 3, 9, {{0, 25}, {25, 50}, {50, 76}, {76, 102}, {102, 128}});

        std::vector<unsigned char> matrix(140 * 128);
        bool rejected = false;
        try { ECProject::gen_alrc_matrix(matrix.data(), 96, 3, 5); }
        catch (const std::invalid_argument &) { rejected = true; }
        require(rejected, "invalid ALRC parameters were accepted");
        std::cout << "ALRC encoding verification passed" << std::endl;
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "ALRC encoding verification failed: " << error.what() << std::endl;
        return 1;
    }
}
