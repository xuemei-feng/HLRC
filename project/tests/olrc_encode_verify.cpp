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
            std::swap(matrix[pivot_row * columns + current], matrix[candidate * columns + current]);
        const unsigned char inverse = ECProject::gf_inv(matrix[pivot_row * columns + column]);
        for (int current = column; current < columns; ++current)
            matrix[pivot_row * columns + current] =
                ECProject::gf_mul(matrix[pivot_row * columns + current], inverse);
        for (int row = 0; row < rows; ++row) {
            if (row == pivot_row) continue;
            const unsigned char factor = matrix[row * columns + column];
            for (int current = column; factor != 0 && current < columns; ++current)
                matrix[row * columns + current] ^=
                    ECProject::gf_mul(factor, matrix[pivot_row * columns + current]);
        }
        ++pivot_row;
    }
    return pivot_row;
}

void verify_double_erasure(const std::vector<unsigned char> &matrix, int k, int r,
                           int begin, int end, int local0, int local1)
{
    const int data_count = end - begin;
    const int dimension = data_count + 3;
    std::vector<int> members;
    for (int data = begin; data < end; ++data) members.push_back(data);
    for (int global = 0; global < r; ++global) members.push_back(k + global);
    members.insert(members.end(), {local0, local1});
    for (size_t first = 0; first < members.size(); ++first)
        for (size_t second = first + 1; second < members.size(); ++second) {
            std::vector<unsigned char> survivors;
            for (size_t index = 0; index < members.size(); ++index) {
                if (index == first || index == second) continue;
                const unsigned char *row = matrix.data() + static_cast<size_t>(members[index]) * k;
                survivors.insert(survivors.end(), row, row + k);
            }
            require(rank(std::move(survivors), dimension, k) == dimension,
                    "same-group double erasure is not recoverable");
        }
}
}

void verify_case(int k, int r, int z, const std::vector<std::pair<int, int>> &bounds)
{
    const int n = k + r + z;
    std::vector<unsigned char> matrix(static_cast<size_t>(n) * k);
    ECProject::gen_olrc_matrix(matrix.data(), k, r, z);

    for (int row = 0; row < k; ++row)
        for (int column = 0; column < k; ++column)
            require(matrix[static_cast<size_t>(row) * k + column] == (row == column),
                    "matrix is not systematic");
    std::vector<unsigned char> cauchy(static_cast<size_t>(k + r) * k);
    ECProject::gf_gen_cauchy_matrix1(cauchy.data(), k + r, k);
    require(std::equal(matrix.begin() + static_cast<size_t>(k) * k,
                       matrix.begin() + static_cast<size_t>(k + r) * k,
                       cauchy.begin() + static_cast<size_t>(k) * k),
            "global rows differ from Cauchy rows");

    int local = 0;
    for (size_t group = 0; group < bounds.size(); ++group) {
        const int parity_count = group + 1 < bounds.size() ? 2 : 1;
        if (parity_count == 2)
            verify_double_erasure(matrix, k, r, bounds[group].first, bounds[group].second,
                                  k + r + local, k + r + local + 1);
        local += parity_count;
    }
    require(local == z, "local parity count mismatch");

    constexpr int block_size = 257;
    std::mt19937 generator(0x4f4c5243u + static_cast<unsigned int>(k));
    std::vector<std::vector<unsigned char>> blocks(n, std::vector<unsigned char>(block_size));
    for (int data = 0; data < k; ++data)
        for (auto &byte : blocks[data]) byte = static_cast<unsigned char>(generator());
    std::vector<unsigned char *> data(k), parity(r + z);
    for (int id = 0; id < k; ++id) data[id] = blocks[id].data();
    for (int id = 0; id < r + z; ++id) parity[id] = blocks[k + id].data();
    ECProject::encode_olrc(k, r, z, data.data(), parity.data(), block_size);
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
        try { ECProject::gen_olrc_matrix(matrix.data(), 96, 3, 5); }
        catch (const std::invalid_argument &) { rejected = true; }
        require(rejected, "invalid OLRC parameters were accepted");
        std::cout << "OLRC encoding verification passed" << std::endl;
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "OLRC encoding verification failed: " << error.what() << std::endl;
        return 1;
    }
}
