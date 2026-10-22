#include "encoder.h"

#include <iostream>
#include <string>
#include <vector>

namespace {

bool verify_plan(const std::vector<int> &failed,
                 const std::vector<int> &sources,
                 const std::string &label)
{
    constexpr int k = 24;
    constexpr int r = 1;
    constexpr int z = 4;
    std::vector<unsigned char> coefficients;
    const bool ok = ECProject::get_local_fill_plan(
        k, r, z, "LotusLRC", failed, sources, coefficients);
    const size_t expected = failed.size() * sources.size();
    if (!ok || coefficients.size() != expected)
    {
        std::cerr << label << " is not recoverable from the registered sources" << std::endl;
        return false;
    }
    return true;
}

} // namespace

int main()
{
    constexpr int k = 24;
    constexpr int r = 1;
    constexpr int z = 4;
    constexpr int n = k + r + z;

    std::vector<unsigned char> matrix(static_cast<size_t>(n) * k, 0);
    ECProject::gen_lotuslrc_matrix(matrix.data(), k, r, z);

    const auto data_counts = ECProject::get_data_block_num_per_local_group_lotuslrc(k, r, z);
    const auto global_counts = ECProject::get_global_parity_block_num_per_local_group_lotuslrc(k, r, z);
    const auto local_counts = ECProject::get_local_parity_block_num_per_local_group_lotuslrc(k, r, z);
    if (data_counts != std::vector<int>({12, 12}) ||
        global_counts != std::vector<int>({0, 1}) ||
        local_counts != std::vector<int>({2, 2}))
    {
        std::cerr << "Unexpected LotusLRC k=24,r=1,z=4 layout" << std::endl;
        return 1;
    }

    std::vector<int> first_group_data;
    std::vector<int> second_group_data_and_global;
    for (int id = 0; id <= 11; ++id) first_group_data.push_back(id);
    for (int id = 12; id <= 24; ++id) second_group_data_and_global.push_back(id);

    bool ok = true;
    ok &= verify_plan({25}, first_group_data, "L0");
    ok &= verify_plan({27}, second_group_data_and_global, "L2");
    ok &= verify_plan({27, 28}, second_group_data_and_global, "L2+L3");

    if (!ok) return 1;
    std::cout << "LotusLRC k=24,r=1,z=4 matrix and repair plans verified" << std::endl;
    return 0;
}
