#include "encoder.h"
#include "repair_benchmark_plan.h"

#include <algorithm>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void require(bool condition, const std::string &message)
{
    if (!condition) throw std::runtime_error(message);
}

void verify(int block_size)
{
    constexpr int k = 64, r = 3, z = 3;
    const auto cases = ECProject::RepairBenchmark::make_optimal_quadruple_block_cases(k);
    require(cases.size() == 1, "expected exactly one k64 quadruple case");
    require(cases[0].failed_name == "G0+G1+G2+L0" && cases[0].tasks.size() == 1,
            "four failures must share one read/compute task");
    const auto &task = cases[0].tasks[0];
    require(task.failed_block_ids == std::vector<int>({64, 65, 66, 67}),
            "G0 must be first so its proxy coordinates the repair");
    require(task.source_block_ids == ECProject::RepairBenchmark::inclusive_range(0, 63),
            "the task must read each of D0-D63 exactly once");

    std::vector<std::vector<unsigned char>> blocks(k + r + z,
        std::vector<unsigned char>(block_size));
    std::mt19937 generator(0x643456u);
    for (int id = 0; id < k; ++id)
        for (auto &byte : blocks[id]) byte = static_cast<unsigned char>(generator());
    std::vector<unsigned char *> data(k), parity(r + z);
    for (int id = 0; id < k; ++id) data[id] = blocks[id].data();
    for (int id = 0; id < r + z; ++id) parity[id] = blocks[k + id].data();
    ECProject::encode_optimal_lrc(k, r, z, data.data(), parity.data(), block_size);

    std::vector<std::vector<unsigned char>> expected;
    for (int id : task.failed_block_ids) {
        expected.push_back(blocks[id]);
        std::fill(blocks[id].begin(), blocks[id].end(), 0);
    }
    // Discard every parity buffer: neither repair path can read a failed or
    // surviving parity as an accidental source.
    for (int id = k; id < k + r + z; ++id)
        std::fill(blocks[id].begin(), blocks[id].end(), 0);

    // Also test a different source order, as buffers must follow source IDs.
    for (bool reversed : {false, true}) {
        auto sources = task.source_block_ids;
        if (reversed) std::reverse(sources.begin(), sources.end());
        std::vector<unsigned char> coefficients;
        require(ECProject::get_local_fill_plan(k, r, z, "OptimalLRC",
                    task.failed_block_ids, sources, coefficients), "coefficient solve failed");
        require(coefficients.size() == 4U * k, "expected four coefficient rows");
        std::vector<std::vector<unsigned char>> recovered(4,
            std::vector<unsigned char>(block_size, 0xa5));
        std::vector<unsigned char *> inputs, outputs;
        for (int id : sources) inputs.push_back(blocks[id].data());
        for (auto &output : recovered) outputs.push_back(output.data());
        std::vector<unsigned char> tables(32U * k * 4);
        ECProject::ec_init_tables(k, 4, coefficients.data(), tables.data());
        ECProject::ec_encode_data_avx2(block_size, k, 4, tables.data(),
                                      inputs.data(), outputs.data());
        require(recovered == expected, "AVX2 four-output repair differs from original parity");

        for (int row = 0; row < 4; ++row) {
            std::vector<unsigned char> direct(block_size, 0);
            for (int col = 0; col < k; ++col)
                for (int byte = 0; byte < block_size; ++byte)
                    direct[byte] ^= ECProject::gf_mul(coefficients[row * k + col],
                                                       blocks[sources[col]][byte]);
            require(direct == expected[row], "scalar GF repair differs from original parity");
        }
    }
    std::cout << "k64 G0+G1+G2+L0 block_size=" << block_size << ": passed\n";
}
} // namespace

int main()
{
    try {
        verify(4096);
        verify(1048576);
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "OptimalLRC k64 quadruple repair failed: " << error.what() << '\n';
        return 1;
    }
}
