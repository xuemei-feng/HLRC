#include "encoder.h"
#include <algorithm>
#include <iostream>
#include <random>
#include <set>
#include <stdexcept>
#include <vector>

namespace {
void require(bool value, const char *message) { if (!value) throw std::runtime_error(message); }

void verify_case(int k, int r, int z)
{
    const int n = k + r + z;
    std::vector<unsigned char> matrix(static_cast<size_t>(n) * k);
    ECProject::gen_nlrc_matrix(matrix.data(), k, r, z);
    for (int row = 0; row < k; ++row)
        for (int col = 0; col < k; ++col)
            require(matrix[static_cast<size_t>(row) * k + col] == (row == col), "matrix is not systematic");

    std::vector<unsigned char> cauchy(static_cast<size_t>(k + r) * k);
    ECProject::gf_gen_cauchy_matrix1(cauchy.data(), k + r, k);
    for (int global = 0; global < r; ++global)
        for (int col = 0; col < k; ++col) {
            require(matrix[static_cast<size_t>(k + global) * k + col] == cauchy[static_cast<size_t>(k + global) * k + col], "global row mismatch");
            require(matrix[static_cast<size_t>(k + global) * k + col] != 0, "global row has zero coefficient");
        }

    const auto logical = ECProject::get_nlrc_local_group_block_ids(k, r, z);
    require(logical.size() == static_cast<size_t>((z + 1) / 2), "logical group count mismatch");
    for (size_t group = 0; group + 1 < logical.size(); ++group) {
        const int local0 = logical[group][logical[group].size() - 2];
        const int local1 = logical[group].back();
        require(local1 == local0 + 1, "local parity ids are not contiguous");
        std::set<int> data(logical[group].begin(), logical[group].end() - 2);
        for (int col = 0; col < k; ++col) {
            require((matrix[static_cast<size_t>(local0) * k + col] != 0) == data.count(col), "first local support mismatch");
            require((matrix[static_cast<size_t>(local1) * k + col] != 0) == data.count(col), "second local support mismatch");
        }
        for (size_t a = 0; a < logical[group].size(); ++a)
            for (size_t b = a + 1; b < logical[group].size(); ++b) {
                std::vector<int> failed{logical[group][a], logical[group][b]}, sources;
                for (int id : logical[group]) if (id != failed[0] && id != failed[1]) sources.push_back(id);
                std::vector<unsigned char> coeffs;
                require(ECProject::get_local_fill_plan(k, r, z, "NLRC", failed, sources, coeffs), "double local erasure is not recoverable");
                for (size_t recovered = 0; recovered < failed.size(); ++recovered)
                    for (int col = 0; col < k; ++col) {
                        unsigned char actual = 0;
                        for (size_t source = 0; source < sources.size(); ++source)
                            actual ^= ECProject::gf_mul(coeffs[recovered * sources.size() + source], matrix[static_cast<size_t>(sources[source]) * k + col]);
                        require(actual == matrix[static_cast<size_t>(failed[recovered]) * k + col], "double erasure coefficients are incorrect");
                    }
            }
    }
    const auto &tail = logical.back();
    const int tail_local = tail.back();
    const int tail_data = static_cast<int>(tail.size()) - r - 1;
    const int member_count = tail_data + r;
    std::vector<unsigned char> tail_cauchy(static_cast<size_t>(member_count + 1) * member_count);
    ECProject::gf_gen_cauchy_matrix1(tail_cauchy.data(), member_count + 1, member_count);
    const unsigned char *tail_coeff = tail_cauchy.data() + static_cast<size_t>(member_count) * member_count;
    for (int col = 0; col < k; ++col) {
        unsigned char expected = 0;
        for (int global = 0; global < r; ++global)
            expected ^= ECProject::gf_mul(tail_coeff[tail_data + global], matrix[static_cast<size_t>(k + global) * k + col]);
        auto it = std::find(tail.begin(), tail.begin() + tail_data, col);
        if (it != tail.begin() + tail_data)
            expected ^= tail_coeff[it - tail.begin()];
        require(matrix[static_cast<size_t>(tail_local) * k + col] == expected, "folded tail row formula mismatch");
    }
    for (int failed : tail) {
        std::vector<int> sources;
        for (int id : tail) if (id != failed) sources.push_back(id);
        std::vector<unsigned char> coeffs;
        require(ECProject::get_local_fill_plan(k, r, z, "NLRC", {failed}, sources, coeffs), "tail single erasure is not recoverable");
    }

    const auto placement = ECProject::get_nlrc_group_id_to_block_ids(k, r, z);
    std::vector<int> seen(n, 0);
    for (size_t group = 0; group < placement.size(); ++group) {
        require(placement.at(group).size() <= static_cast<size_t>(r + 1), "physical group exceeds rack capacity");
        for (int id : placement.at(group)) { require(id >= 0 && id < n, "placement id out of range"); ++seen[id]; }
    }
    require(std::all_of(seen.begin(), seen.end(), [](int count) { return count == 1; }), "placement is not a partition");

    const int block_size = 257;
    std::mt19937 rng(17 + k);
    std::vector<std::vector<unsigned char>> blocks(n, std::vector<unsigned char>(block_size));
    for (int data = 0; data < k; ++data) for (auto &byte : blocks[data]) byte = static_cast<unsigned char>(rng());
    std::vector<unsigned char *> data_ptrs(k), parity_ptrs(r + z);
    for (int i = 0; i < k; ++i) data_ptrs[i] = blocks[i].data();
    for (int i = 0; i < r + z; ++i) parity_ptrs[i] = blocks[k + i].data();
    ECProject::encode_nlrc(k, r, z, data_ptrs.data(), parity_ptrs.data(), block_size);
    for (int row = k; row < n; ++row)
        for (int pos = 0; pos < block_size; ++pos) {
            unsigned char expected = 0;
            for (int col = 0; col < k; ++col) expected ^= ECProject::gf_mul(matrix[static_cast<size_t>(row) * k + col], blocks[col][pos]);
            require(blocks[row][pos] == expected, "encoded bytes do not match generator matrix");
        }
}
}

int main()
{
    try {
        verify_case(64, 3, 5); verify_case(96, 3, 7); verify_case(128, 3, 9);
        std::vector<unsigned char> matrix(72 * 64);
        bool rejected = false;
        try { ECProject::gen_nlrc_matrix(matrix.data(), 64, 2, 6); } catch (const std::invalid_argument &) { rejected = true; }
        require(rejected, "invalid NLRC parameters were accepted");
        std::cout << "NLRC encoding verification passed" << std::endl;
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "NLRC encoding verification failed: " << error.what() << std::endl;
        return 1;
    }
}
