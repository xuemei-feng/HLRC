/**
 * Data layout / placement: per-group block counts by code type (AzureLRC, OptimalLRC, UniformLRC, UniLRC, LotusLRC).
 * Used by client and coordinator; no dependency on proto or RPC.
 */
#include "encoder.h"
#include <stdexcept>
#include <string>
#include <vector>
#include <algorithm>
#include <numeric>
namespace ECProject
{
  namespace
  {
    std::vector<int> get_uneven_group_sizes(int k, int z, const char *code_type)
    {
      if (k <= 0 || z <= 0 || z > k)
        throw std::invalid_argument(std::string(code_type) + " requires 0 < z <= k");
      std::vector<int> sizes(z, k / z);
      const int remainder = k % z;
      for (int group = z - remainder; group < z; ++group)
        sizes[group]++;
      return sizes;
    }

    std::vector<std::pair<int, int>> get_group_bounds(const std::vector<int> &sizes)
    {
      std::vector<std::pair<int, int>> bounds;
      int begin = 0;
      for (int size : sizes) {
        bounds.push_back({begin, begin + size});
        begin += size;
      }
      return bounds;
    }
  }


  /* ----- Dispatchers (by code_type) ----- */
  std::vector<int> get_data_block_num_per_group(int k, int r, int z, const std::string &code_type)
  {
    if (code_type == "AzureLRC")
      return get_data_block_num_per_group_azurelrc(k, r, z);
    if (code_type == "OptimalLRC")
      return get_data_block_num_per_group_optimal_lrc(k, r, z);
    if (code_type == "UniformLRC")
      return get_data_block_num_per_group_uniform_lrc(k, r, z);
    if (code_type == "UniLRC")
      return get_data_block_num_per_group_unilrc(k, r, z);
    if (code_type == "LotusLRC")
      return get_data_block_num_per_group_lotuslrc(k, r, z);
    if (code_type == "HLRC")
      return get_data_block_num_per_group_hlrc(k, r, z);
    if (code_type == "NLRC")
      return get_data_block_num_per_group_nlrc(k, r, z);
    return {};
  }

  std::vector<int> get_global_parity_block_num_per_group(int k, int r, int z, const std::string &code_type)
  {
    if (code_type == "AzureLRC")
      return get_global_parity_block_num_per_group_azurelrc(k, r, z);
    if (code_type == "OptimalLRC")
      return get_global_parity_block_num_per_group_optimal_lrc(k, r, z);
    if (code_type == "UniformLRC")
      return get_global_parity_block_num_per_group_uniform_lrc(k, r, z);
    if (code_type == "UniLRC")
      return get_global_parity_block_num_per_group_unilrc(k, r, z);
    if (code_type == "LotusLRC")
      return get_global_parity_block_num_per_group_lotuslrc(k, r, z);
    if (code_type == "HLRC")
      return get_global_parity_block_num_per_group_hlrc(k, r, z);
    if (code_type == "NLRC")
      return get_global_parity_block_num_per_group_nlrc(k, r, z);
    return {};
  }

  std::vector<int> get_local_parity_block_num_per_group(int k, int r, int z, const std::string &code_type)
  {
    if (code_type == "AzureLRC")
      return get_local_parity_block_num_per_group_azurelrc(k, r, z);
    if (code_type == "OptimalLRC")
      return get_local_parity_block_num_per_group_optimal_lrc(k, r, z);
    if (code_type == "UniformLRC")
      return get_local_parity_block_num_per_group_uniform_lrc(k, r, z);
    if (code_type == "UniLRC")
      return get_local_parity_block_num_per_group_unilrc(k, r, z);
    if (code_type == "LotusLRC")
      return get_local_parity_block_num_per_group_lotuslrc(k, r, z);
    if (code_type == "HLRC")
      return get_local_parity_block_num_per_group_hlrc(k, r, z);
    if (code_type == "NLRC")
      return get_local_parity_block_num_per_group_nlrc(k, r, z);
    return {};
  }

  namespace
  {
    void validate_nlrc_layout_parameters(int k, int r, int z)
    {
      if (!((k == 64 && r == 3 && z == 5) ||
            (k == 96 && r == 3 && z == 7) ||
            (k == 128 && r == 3 && z == 9)))
        throw std::invalid_argument("NLRC requires (k=64,r=3,z=5), (k=96,r=3,z=7), or (k=128,r=3,z=9)");
    }

    std::vector<std::pair<int, int>> nlrc_bounds(int k)
    {
      if (k == 64) return {{0,22},{22,44},{44,64}};
      if (k == 96) return {{0,24},{24,49},{49,74},{74,96}};
      return {{0,26},{26,52},{52,78},{78,104},{104,128}};
    }

    void validate_hlrc_layout_parameters(int k, int r, int z)
    {
      if (!((k == 64 && r == 2 && z == 6) ||
            (k == 96 && r == 2 && z == 8) ||
            (k == 128 && r == 2 && z == 10)))
        throw std::invalid_argument("HLRC requires (k=64,r=2,z=6), (k=96,r=2,z=8), or (k=128,r=2,z=10)");
    }

    std::vector<int> data_range(int begin)
    {
      std::vector<int> blocks;
      for (int id = begin; id < begin + 16; ++id)
        blocks.push_back(id);
      return blocks;
    }

    void append(std::vector<int> &blocks, std::initializer_list<int> ids)
    {
      blocks.insert(blocks.end(), ids.begin(), ids.end());
    }
  }

  std::vector<std::vector<int>> get_nlrc_local_group_block_ids(int k, int r, int z)
  {
    validate_nlrc_layout_parameters(k, r, z);
    const auto bounds = nlrc_bounds(k);
    std::vector<std::vector<int>> groups(bounds.size());
    int local = k + r;
    for (size_t group = 0; group < bounds.size(); ++group) {
      for (int id = bounds[group].first; id < bounds[group].second; ++id)
        groups[group].push_back(id);
      if (group + 1 == bounds.size())
        for (int global = 0; global < r; ++global) groups[group].push_back(k + global);
      const int count = group + 1 == bounds.size() ? 1 : 2;
      for (int i = 0; i < count; ++i) groups[group].push_back(local++);
    }
    return groups;
  }

  int get_nlrc_block_id_to_local_group_id(int k, int r, int z, int block_id)
  {
    const auto groups = get_nlrc_local_group_block_ids(k, r, z);
    for (size_t group = 0; group < groups.size(); ++group)
      if (std::find(groups[group].begin(), groups[group].end(), block_id) != groups[group].end())
        return static_cast<int>(group);
    throw std::runtime_error("NLRC block id is out of range");
  }

  std::unordered_map<int, std::vector<int>> get_nlrc_group_id_to_block_ids(int k, int r, int z)
  {
    const auto logical = get_nlrc_local_group_block_ids(k, r, z);
    std::unordered_map<int, std::vector<int>> physical;
    int physical_id = 0;
    for (const auto &members : logical) {
      const int count = (static_cast<int>(members.size()) + r) / (r + 1);
      const int base = static_cast<int>(members.size()) / count;
      const int extra = static_cast<int>(members.size()) % count;
      size_t offset = 0;
      for (int part = 0; part < count; ++part) {
        const int size = base + (part < extra ? 1 : 0);
        physical[physical_id++] = std::vector<int>(members.begin() + offset,
                                                   members.begin() + offset + size);
        offset += size;
      }
    }
    return physical;
  }

  std::unordered_map<int, int> get_nlrc_block_id_to_group_id(int k, int r, int z)
  {
    std::unordered_map<int, int> result;
    for (const auto &entry : get_nlrc_group_id_to_block_ids(k, r, z))
      for (int id : entry.second) result[id] = entry.first;
    return result;
  }

  std::vector<int> get_data_block_num_per_group_nlrc(int k, int r, int z)
  {
    std::vector<int> result;
    const auto groups = get_nlrc_group_id_to_block_ids(k, r, z);
    for (size_t group = 0; group < groups.size(); ++group)
      result.push_back(std::count_if(groups.at(group).begin(), groups.at(group).end(),
                                     [k](int id) { return id < k; }));
    return result;
  }

  std::vector<int> get_global_parity_block_num_per_group_nlrc(int k, int r, int z)
  {
    std::vector<int> result;
    const auto groups = get_nlrc_group_id_to_block_ids(k, r, z);
    for (size_t group = 0; group < groups.size(); ++group)
      result.push_back(std::count_if(groups.at(group).begin(), groups.at(group).end(),
                                     [k,r](int id) { return id >= k && id < k+r; }));
    return result;
  }

  std::vector<int> get_local_parity_block_num_per_group_nlrc(int k, int r, int z)
  {
    std::vector<int> result;
    const auto groups = get_nlrc_group_id_to_block_ids(k, r, z);
    for (size_t group = 0; group < groups.size(); ++group)
      result.push_back(std::count_if(groups.at(group).begin(), groups.at(group).end(),
                                     [k,r](int id) { return id >= k+r; }));
    return result;
  }

  std::unordered_map<int, std::vector<int>> get_hlrc_group_id_to_block_ids(int k, int r, int z)
  {
    validate_hlrc_layout_parameters(k, r, z);
    if (k == 64)
      return {
          {0, {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,64,66}},
          {1, {16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,67,70}},
          {2, {32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,65,68}},
          {3, {48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,69,71}},
      };
    if (k == 96)
      return {
          {0, {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,96,98}},
          {1, {16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,99,104}},
          {2, {32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,100}},
          {3, {48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,97,101}},
          {4, {64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,102,105}},
          {5, {80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,103}},
      };

    std::unordered_map<int, std::vector<int>> groups;
    for (int group = 0; group < 8; ++group)
      groups[group] = data_range(group * 16);
    append(groups[0], {128, 130});
    append(groups[1], {131, 138});
    append(groups[2], {132});
    append(groups[3], {133});
    append(groups[4], {129, 134});
    append(groups[5], {135, 139});
    append(groups[6], {136});
    append(groups[7], {137});
    return groups;
  }

  std::unordered_map<int, int> get_hlrc_block_id_to_group_id(int k, int r, int z)
  {
    std::unordered_map<int, int> result;
    for (const auto &entry : get_hlrc_group_id_to_block_ids(k, r, z))
      for (int block_id : entry.second)
        result[block_id] = entry.first;
    return result;
  }

  std::vector<int> get_data_block_num_per_group_hlrc(int k, int r, int z)
  {
    validate_hlrc_layout_parameters(k, r, z);
    return std::vector<int>(k / 16, 16);
  }

  std::vector<int> get_global_parity_block_num_per_group_hlrc(int k, int r, int z)
  {
    validate_hlrc_layout_parameters(k, r, z);
    if (k == 64) return {1, 0, 1, 0};
    if (k == 96) return {1, 0, 0, 1, 0, 0};
    return {1, 0, 0, 0, 1, 0, 0, 0};
  }

  std::vector<int> get_local_parity_block_num_per_group_hlrc(int k, int r, int z)
  {
    validate_hlrc_layout_parameters(k, r, z);
    if (k == 64) return {1, 2, 1, 2};
    if (k == 96) return {1, 2, 1, 1, 2, 1};
    return {1, 2, 1, 1, 1, 2, 1, 1};
  }

  /* ----- AzureLRC ----- */
  std::vector<int> get_azure_lrc_local_group_sizes(int k, int z)
  {
    return get_uneven_group_sizes(k, z, "AzureLRC");
  }

  std::vector<std::pair<int, int>> get_azure_lrc_local_group_bounds(int k, int z)
  {
    return get_group_bounds(get_azure_lrc_local_group_sizes(k, z));
  }

  namespace
  {
    std::vector<int> get_azure_lrc_groups_per_local_group(int k, int r, int z)
    {
      std::vector<int> result;
      for (int data_size : get_azure_lrc_local_group_sizes(k, z))
        result.push_back((data_size + 1 + r) / (r + 1));
      return result;
    }
  }

  std::unordered_map<int, std::vector<int>> get_azurelrc_group_id_to_block_ids(int k, int r, int z)
  {
    std::unordered_map<int, std::vector<int>> groups;
    const auto bounds = get_azure_lrc_local_group_bounds(k, z);
    const auto group_counts = get_azure_lrc_groups_per_local_group(k, r, z);
    int group_id = 0;
    for (int local = 0; local < z; ++local)
    {
      const int slots = bounds[local].second - bounds[local].first + 1;
      const int count = group_counts[local];
      const int base = slots / count;
      const int extra = slots % count;
      int data_id = bounds[local].first;
      for (int part = 0; part < count; ++part)
      {
        const int part_slots = base + (part < extra ? 1 : 0);
        const bool last = part == count - 1;
        const int data_slots = part_slots - (last ? 1 : 0);
        for (int i = 0; i < data_slots; ++i)
          groups[group_id].push_back(data_id++);
        if (last)
          groups[group_id].push_back(k + r + local);
        group_id++;
      }
    }
    for (int global = 0; global < r; ++global)
      groups[group_id].push_back(k + global);
    return groups;
  }

  std::unordered_map<int, int> get_azurelrc_block_id_to_group_id(int k, int r, int z)
  {
    std::unordered_map<int, int> result;
    for (const auto &entry : get_azurelrc_group_id_to_block_ids(k, r, z))
      for (int block_id : entry.second)
        result[block_id] = entry.first;
    return result;
  }

  std::vector<int> get_data_block_num_per_group_azurelrc(int k, int r, int z)
  {
    std::vector<int> result;
    const auto groups = get_azurelrc_group_id_to_block_ids(k, r, z);
    for (size_t group = 0; group < groups.size(); ++group)
      result.push_back(std::count_if(groups.at(group).begin(), groups.at(group).end(),
                                     [k](int id) { return id < k; }));
    return result;
  }

  std::vector<int> get_global_parity_block_num_per_group_azurelrc(int k, int r, int z)
  {
    std::vector<int> result;
    const auto groups = get_azurelrc_group_id_to_block_ids(k, r, z);
    for (size_t group = 0; group < groups.size(); ++group)
      result.push_back(std::count_if(groups.at(group).begin(), groups.at(group).end(),
                                     [k, r](int id) { return id >= k && id < k + r; }));
    return result;
  }

  std::vector<int> get_local_parity_block_num_per_group_azurelrc(int k, int r, int z)
  {
    std::vector<int> result;
    const auto groups = get_azurelrc_group_id_to_block_ids(k, r, z);
    for (size_t group = 0; group < groups.size(); ++group)
      result.push_back(std::count_if(groups.at(group).begin(), groups.at(group).end(),
                                     [k, r](int id) { return id >= k + r; }));
    return result;
  }

  int get_azurelrc_block_id_to_local_group_id(int k, int r, int z, int block_id)
  {
    if (block_id < 0 || block_id >= k + r + z)
      throw std::runtime_error("block id is out of range");
    if (block_id < k)
    {
      const auto bounds = get_azure_lrc_local_group_bounds(k, z);
      for (int group = 0; group < z; ++group)
        if (block_id < bounds[group].second)
          return group;
    }
    if (block_id < k + r)
      return -1;
    return block_id - k - r;
  }

  /* ----- OptimalLRC ----- */
  std::vector<int> get_optimal_lrc_local_group_sizes(int k, int z)
  {
    return get_uneven_group_sizes(k, z, "OptimalLRC");
  }

  std::vector<std::pair<int, int>> get_optimal_lrc_local_group_bounds(int k, int z)
  {
    return get_group_bounds(get_optimal_lrc_local_group_sizes(k, z));
  }

  namespace
  {
    std::vector<int> get_optimal_groups_per_local_group(int k, int r, int z)
    {
      std::vector<int> result;
      for (int data_size : get_optimal_lrc_local_group_sizes(k, z))
        result.push_back(data_size / (r + 1) + 1);
      return result;
    }
  }

  std::unordered_map<int, std::vector<int>> get_optimal_lrc_group_id_to_block_ids(int k, int r, int z)
  {
    std::unordered_map<int, std::vector<int>> groups;
    const auto bounds = get_optimal_lrc_local_group_bounds(k, z);
    const auto group_counts = get_optimal_groups_per_local_group(k, r, z);
    int group_id = 0;
    for (int local = 0; local < z; ++local) {
      const int slots = bounds[local].second - bounds[local].first + 1;
      const int count = group_counts[local];
      const int base = slots / count;
      const int extra = slots % count;
      int data_id = bounds[local].first;
      for (int part = 0; part < count; ++part) {
        const int part_slots = base + (part < extra ? 1 : 0);
        const bool last = part == count - 1;
        for (int i = 0; i < part_slots - (last ? 1 : 0); ++i)
          groups[group_id].push_back(data_id++);
        if (last)
          groups[group_id].push_back(k + r + local);
        group_id++;
      }
    }
    for (int global = 0; global < r; ++global)
      groups[group_id].push_back(k + global);
    return groups;
  }

  std::unordered_map<int, int> get_optimal_lrc_block_id_to_group_id(int k, int r, int z)
  {
    std::unordered_map<int, int> result;
    for (const auto &entry : get_optimal_lrc_group_id_to_block_ids(k, r, z))
      for (int block_id : entry.second)
        result[block_id] = entry.first;
    return result;
  }

  std::vector<int> get_data_block_num_per_group_optimal_lrc(int k, int r, int z)
  {
    std::vector<int> result;
    const auto groups = get_optimal_lrc_group_id_to_block_ids(k, r, z);
    for (size_t group = 0; group < groups.size(); ++group)
      result.push_back(std::count_if(groups.at(group).begin(), groups.at(group).end(),
                                     [k](int id) { return id < k; }));
    return result;
  }

  std::vector<int> get_global_parity_block_num_per_group_optimal_lrc(int k, int r, int z)
  {
    std::vector<int> result;
    const auto groups = get_optimal_lrc_group_id_to_block_ids(k, r, z);
    for (size_t group = 0; group < groups.size(); ++group)
      result.push_back(std::count_if(groups.at(group).begin(), groups.at(group).end(),
                                     [k, r](int id) { return id >= k && id < k + r; }));
    return result;
  }

  std::vector<int> get_local_parity_block_num_per_group_optimal_lrc(int k, int r, int z)
  {
    std::vector<int> result;
    const auto groups = get_optimal_lrc_group_id_to_block_ids(k, r, z);
    for (size_t group = 0; group < groups.size(); ++group)
      result.push_back(std::count_if(groups.at(group).begin(), groups.at(group).end(),
                                     [k, r](int id) { return id >= k + r; }));
    return result;
  }

  int get_optimal_lrc_block_id_to_local_group_id(int k, int r, int z, int block_id)
  {
    if (block_id < 0 || block_id >= k + r + z)
      throw std::runtime_error("block id is out of range");
    if (block_id < k) {
      const auto bounds = get_optimal_lrc_local_group_bounds(k, z);
      for (int group = 0; group < z; ++group)
        if (block_id < bounds[group].second)
          return group;
    }
    if (block_id < k + r)
      return 0;
    return block_id - k - r;
  }

  /* ----- UniformLRC ----- */
  std::vector<int> get_data_block_num_per_group_uniform_lrc(int k, int r, int z)
  {
    std::vector<int> data_block_num_per_group;
    std::unordered_map<int, std::vector<int>> group_id_to_block_ids = get_uniform_lrc_group_id_to_block_ids(k, r, z);
    for (size_t i = 0; i < group_id_to_block_ids.size(); i++){
      int data_num = 0;
      for (size_t j = 0; j < group_id_to_block_ids[(int)i].size(); j++){
        if(group_id_to_block_ids[(int)i][j] < k)
          data_num++;
      }
      data_block_num_per_group.push_back(data_num);
    }
    return data_block_num_per_group;
  }

  std::vector<int> get_global_parity_block_num_per_group_uniform_lrc(int k, int r, int z)
  {
    std::vector<int> global_parity_block_num_per_group;
    std::unordered_map<int, std::vector<int>> group_id_to_block_ids = get_uniform_lrc_group_id_to_block_ids(k, r, z);
    for (size_t i = 0; i < group_id_to_block_ids.size(); i++){
      int global_parity_num = 0;
      for (size_t j = 0; j < group_id_to_block_ids[(int)i].size(); j++){
        if(group_id_to_block_ids[(int)i][j] >= k && group_id_to_block_ids[(int)i][j] < k + r)
          global_parity_num++;
      }
      global_parity_block_num_per_group.push_back(global_parity_num);
    }
    return global_parity_block_num_per_group;
  }

  std::vector<int> get_local_parity_block_num_per_group_uniform_lrc(int k, int r, int z)
  {
    std::vector<int> local_parity_block_num_per_group;
    std::unordered_map<int, std::vector<int>> group_id_to_block_ids = get_uniform_lrc_group_id_to_block_ids(k, r, z);
    for (size_t i = 0; i < group_id_to_block_ids.size(); i++){
      int local_parity_num = 0;
      for (size_t j = 0; j < group_id_to_block_ids[(int)i].size(); j++){
        if(group_id_to_block_ids[(int)i][j] >= k + r)
          local_parity_num++;
      }
      local_parity_block_num_per_group.push_back(local_parity_num);
    }
    return local_parity_block_num_per_group;
  }

  std::vector<std::pair<int, int>> get_uniform_lrc_data_like_group_bounds(int k, int r, int z)
  {
    if (k <= 0 || r < 0 || z <= 0 || z > k + r)
      throw std::invalid_argument("UniformLRC requires k > 0, r >= 0, and 0 < z <= k+r");
    return get_group_bounds(get_uneven_group_sizes(k + r, z, "UniformLRC"));
  }

  std::vector<int> get_uniform_lrc_local_group_sizes(int k, int r, int z)
  {
    std::vector<int> local_group_sizes;
    for (const auto &bound : get_uniform_lrc_data_like_group_bounds(k, r, z))
      local_group_sizes.push_back(bound.second - bound.first + 1); // include local parity
    return local_group_sizes;
  }

  int get_uniform_lrc_block_id_to_local_group_id(int k, int r, int z, int block_id)
  {
    if(block_id >= k + r)
      return (block_id - k - r);
    else{
      const auto bounds = get_uniform_lrc_data_like_group_bounds(k, r, z);
      for (int group = 0; group < z; ++group)
        if (block_id >= bounds[group].first && block_id < bounds[group].second)
          return group;
      throw std::runtime_error("block id is out of range");
    }
  }

  std::unordered_map<int, int> get_uniform_lrc_block_id_to_group_id(int k, int r, int z){
    std::unordered_map<int, int> block_id_to_group_id;
    std::unordered_map<int, std::vector<int>> group_id_to_block_ids = get_uniform_lrc_group_id_to_block_ids(k, r, z);
    for (size_t i = 0; i < group_id_to_block_ids.size(); i++){
      for (size_t j = 0; j < group_id_to_block_ids[(int)i].size(); j++)
        block_id_to_group_id[group_id_to_block_ids[(int)i][(int)j]] = (int)i;
    }
    return block_id_to_group_id;
  }

  std::unordered_map<int, std::vector<int>> get_uniform_lrc_group_id_to_block_ids(int k, int r, int z){
    std::unordered_map<int, std::vector<int>> group_id_to_block_ids;
    std::vector<int> group_sizes = get_uniform_lrc_group_sizes(k, r, z);
    std::vector<int> group_num_per_local_group = get_uniform_lrc_group_num_per_local_group(k, r, z);
    int cur_group_id = 0;
    int cur_block_id = 0;
    for (size_t i = 0; i < group_num_per_local_group.size(); i++){
      for (int j = 0; j < group_num_per_local_group[i]; j++){
        std::vector<int> block_ids;
        for (int l = 0; l < group_sizes[cur_group_id]; l++){
          block_ids.push_back(cur_block_id);
          cur_block_id++;
        }
        if(j == group_num_per_local_group[i] - 1){
          block_ids.erase(block_ids.end() - 1);
          block_ids.push_back(k + r + (int)i); // local parity block
          cur_block_id--; // because we erase the last block
        }
        group_id_to_block_ids[cur_group_id] = block_ids;
        cur_group_id++;
      }
    }
    return group_id_to_block_ids;
  }

  std::vector<int> get_uniform_lrc_group_num_per_local_group(int k, int r, int z)
  {
    std::vector<int> group_num_per_local_group;
    std::vector<int> local_group_sizes = get_uniform_lrc_local_group_sizes(k, r, z);
    int max_capacity = r + 1;
    for (size_t i = 0; i < local_group_sizes.size(); i++)
    {
      int group_num = local_group_sizes[i] / max_capacity + bool(local_group_sizes[i] % max_capacity);
      group_num_per_local_group.push_back(group_num);
    }
    return group_num_per_local_group;
  }

  std::vector<int> get_uniform_lrc_group_sizes(int k, int r, int z)
  {
    std::vector<int> group_sizes;
    std::vector<int> local_group_sizes = get_uniform_lrc_local_group_sizes(k, r, z);
    int max_capacity = r + 1;
    for (size_t i = 0; i < local_group_sizes.size(); i++)
    {
      int S = local_group_sizes[i];
      // Split one local group across ceil(S/max_capacity) clusters as evenly as possible,
      // e.g. S=13, max_capacity=4 -> 4 clusters of sizes 4,3,3,3 (not greedy 4,4,4,1).
      int cluster_num = S / max_capacity + bool(S % max_capacity);
      int base = S / cluster_num;
      int larger = S % cluster_num; // the first `larger` clusters hold one extra block
      for (int j = 0; j < larger; j++)
        group_sizes.push_back(base + 1);
      for (int j = 0; j < cluster_num - larger; j++)
        group_sizes.push_back(base);
    }
    return group_sizes;
  }

  /* ----- UniLRC ----- */
  std::vector<int> get_data_block_num_per_group_unilrc(int k, int r, int z)
  {
    std::vector<int> data_block_num_per_group;
    int local_data_num = k / z;
    for (int i = 0; i < z; i++)
      data_block_num_per_group.push_back(local_data_num);
    return data_block_num_per_group;
  }

  std::vector<int> get_global_parity_block_num_per_group_unilrc(int k, int r, int z)
  {
    std::vector<int> global_parity_block_num_per_group;
    int local_global_parity_num = r / z;
    for (int i = 0; i < z; i++)
      global_parity_block_num_per_group.push_back(local_global_parity_num);
    return global_parity_block_num_per_group;
  }

  std::vector<int> get_local_parity_block_num_per_group_unilrc(int k, int r, int z)
  {
    std::vector<int> local_parity_block_num_per_group;
    for (int i = 0; i < z; i++)
      local_parity_block_num_per_group.push_back(1);
    return local_parity_block_num_per_group;
  }

  /* ----- LotusLRC ----- */
  std::vector<int> get_data_block_num_per_group_lotuslrc(int k, int r, int z)
  {
    std::vector<int> data_block_num_per_group;
    std::vector<int> local_group_sizes = get_lotuslrc_local_group_sizes(k, r, z);
    int local_group_num = local_group_sizes.size();
    std::vector<int> data_block_num_per_local_group = get_data_block_num_per_local_group_lotuslrc(k, r, z);
    std::vector<int> global_parity_block_num_per_local_group = get_global_parity_block_num_per_local_group_lotuslrc(k, r, z);
    std::vector<int> local_parity_block_num_per_local_group = get_local_parity_block_num_per_local_group_lotuslrc(k, r, z);
    std::vector<int> group_sizes = get_lotuslrc_group_sizes(k, r, z);
    std::vector<int> group_num_per_local_group = get_lotuslrc_group_num_per_local_group(k, r, z);
    int cur_group_id = 0;
    for (int i = 0; i < local_group_num; i++)
    {
      int parity_num = global_parity_block_num_per_local_group[i] + local_parity_block_num_per_local_group[i];
      for (int j = 0; j < parity_num; j++)
      {
        data_block_num_per_group.push_back(group_sizes[cur_group_id] - 1);
        cur_group_id++;
      }
      for (int j = 0; j < group_num_per_local_group[i] - parity_num; j++)
      {
        data_block_num_per_group.push_back(group_sizes[cur_group_id]);
        cur_group_id++;
      }
    }
    return data_block_num_per_group;
  }

  std::vector<int> get_global_parity_block_num_per_group_lotuslrc(int k, int r, int z)
  {
    std::vector<int> global_parity_block_num_per_group;
    std::vector<int> local_group_sizes = get_lotuslrc_local_group_sizes(k, r, z);
    int local_group_num = local_group_sizes.size();
    std::vector<int> global_parity_block_num_per_local_group = get_global_parity_block_num_per_local_group_lotuslrc(k, r, z);
    std::vector<int> group_num_per_local_group = get_lotuslrc_group_num_per_local_group(k, r, z);
    for (int i = 0; i < local_group_num; i++)
    {
      int global_parity_num = global_parity_block_num_per_local_group[i];
      for (int j = 0; j < global_parity_num; j++)
        global_parity_block_num_per_group.push_back(1);
      for (int j = 0; j < group_num_per_local_group[i] - global_parity_num; j++)
        global_parity_block_num_per_group.push_back(0);
    }
    return global_parity_block_num_per_group;
  }

  std::vector<int> get_local_parity_block_num_per_group_lotuslrc(int k, int r, int z)
  {
    std::vector<int> local_parity_block_num_per_group;
    std::vector<int> local_group_sizes = get_lotuslrc_local_group_sizes(k, r, z);
    int local_group_num = local_group_sizes.size();
    std::vector<int> global_parity_block_num_per_local_group = get_global_parity_block_num_per_local_group_lotuslrc(k, r, z);
    std::vector<int> local_parity_block_num_per_local_group = get_local_parity_block_num_per_local_group_lotuslrc(k, r, z);
    std::vector<int> group_num_per_local_group = get_lotuslrc_group_num_per_local_group(k, r, z);
    for (int i = 0; i < local_group_num; i++)
    {
      int local_parity_num = local_parity_block_num_per_local_group[i];
      int global_parity_num = global_parity_block_num_per_local_group[i];
      for (int j = 0; j < global_parity_num; j++)
        local_parity_block_num_per_group.push_back(0);
      for (int j = 0; j < local_parity_num; j++)
        local_parity_block_num_per_group.push_back(1);
      for (int j = 0; j < group_num_per_local_group[i] - local_parity_num - global_parity_num; j++)
        local_parity_block_num_per_group.push_back(0);
    }
    return local_parity_block_num_per_group;
  }

  std::unordered_map<int, int> get_lotuslrc_block_id_to_group_id(int k, int r, int z)
  {
    std::unordered_map<int, int> block_id_to_group_id;
    std::unordered_map<int, std::vector<int>> group_id_to_block_ids = get_lotuslrc_group_id_to_block_ids(k, r, z);
    for (size_t i = 0; i < group_id_to_block_ids.size(); i++)
    {
      for (size_t j = 0; j < group_id_to_block_ids[(int)i].size(); j++)
        block_id_to_group_id[group_id_to_block_ids[(int)i][(int)j]] = (int)i;
    }
    return block_id_to_group_id;
  }

  std::unordered_map<int, std::vector<int>> get_lotuslrc_group_id_to_block_ids(int k, int r, int z)
  {
    std::vector<int> data_block_num_per_group = get_data_block_num_per_group_lotuslrc(k, r, z);
    std::vector<int> global_parity_block_num_per_group = get_global_parity_block_num_per_group_lotuslrc(k, r, z);
    std::vector<int> local_parity_block_num_per_group = get_local_parity_block_num_per_group_lotuslrc(k, r, z);
    std::unordered_map<int, std::vector<int>> group_id_to_block_ids;
    int cur_data_block_id = 0;
    int cur_global_parity_block_id = k;
    int cur_local_parity_block_id = k + r;
    for (size_t i = 0; i < data_block_num_per_group.size(); i++)
    {
      std::vector<int> block_ids;
      for (int j = 0; j < data_block_num_per_group[i]; j++)
      {
        block_ids.push_back(cur_data_block_id);
        cur_data_block_id++;
      }
      for (int j = 0; j < global_parity_block_num_per_group[i]; j++)
      {
        block_ids.push_back(cur_global_parity_block_id);
        cur_global_parity_block_id++;
      }
      for (int j = 0; j < local_parity_block_num_per_group[i]; j++)
      {
        block_ids.push_back(cur_local_parity_block_id);
        cur_local_parity_block_id++;
      }
      group_id_to_block_ids[(int)i] = block_ids;
    }
    return group_id_to_block_ids;
  }

  std::vector<int> get_data_block_num_per_local_group_lotuslrc(int k, int r, int z)
  {
    // Option B:
    // 1) absorb (k+r) evenly with fewer-first / more-later;
    // 2) data_i = kr_i - global_i;
    // 3) among groups with no fused globals, rearrange those data counts
    //    more-first so earlier unfused groups get more data blocks.
    int local_group_num = z / 2;
    if (local_group_num <= 0)
      throw std::runtime_error("LotusLRC local group num must be positive");
    std::vector<int> global_parity_block_num_per_local_group =
        get_global_parity_block_num_per_local_group_lotuslrc(k, r, z);
    if ((int)global_parity_block_num_per_local_group.size() != local_group_num)
      throw std::runtime_error("global parity per local group size mismatch");

    int kr_base = (k + r) / local_group_num;
    int kr_rem = (k + r) % local_group_num;
    std::vector<int> data_block_num_per_local_group;
    data_block_num_per_local_group.reserve(local_group_num);
    for (int i = 0; i < local_group_num; i++)
    {
      int kr_slots = (i < local_group_num - kr_rem) ? kr_base : (kr_base + 1);
      int data_num = kr_slots - global_parity_block_num_per_local_group[i];
      if (data_num < 0)
        throw std::runtime_error("LotusLRC local group has negative data block count");
      data_block_num_per_local_group.push_back(data_num);
    }

    std::vector<int> unfused_ids;
    std::vector<int> unfused_data;
    for (int i = 0; i < local_group_num; i++)
    {
      if (global_parity_block_num_per_local_group[i] == 0)
      {
        unfused_ids.push_back(i);
        unfused_data.push_back(data_block_num_per_local_group[i]);
      }
    }
    std::sort(unfused_data.begin(), unfused_data.end(), std::greater<int>());
    for (size_t j = 0; j < unfused_ids.size(); j++)
      data_block_num_per_local_group[unfused_ids[j]] = unfused_data[j];

    int data_sum = std::accumulate(data_block_num_per_local_group.begin(),
                                   data_block_num_per_local_group.end(), 0);
    if (data_sum != k)
      throw std::runtime_error("LotusLRC data block count does not sum to k");
    return data_block_num_per_local_group;
  }

  std::vector<int> get_global_parity_block_num_per_local_group_lotuslrc(int k, int r, int z)
  {
    // Globals only go into the latter ceil(group_num/2) local groups,
    // distributed as evenly as possible with fewer-first / more-later.
    (void)k;
    std::vector<int> global_parity_block_num_per_local_group;
    int local_group_num = z / 2;
    if (local_group_num <= 0)
      throw std::runtime_error("LotusLRC local group num must be positive");
    int latter_num = (local_group_num + 1) / 2; // ceil(group_num / 2)
    int start = local_group_num - latter_num;
    int base = r / latter_num;
    int rem = r % latter_num;
    for (int i = 0; i < local_group_num; i++)
    {
      if (i < start)
      {
        global_parity_block_num_per_local_group.push_back(0);
      }
      else
      {
        int j = i - start;
        // first (latter_num - rem) get base; last rem get base + 1
        if (j < latter_num - rem)
          global_parity_block_num_per_local_group.push_back(base);
        else
          global_parity_block_num_per_local_group.push_back(base + 1);
      }
    }
    return global_parity_block_num_per_local_group;
  }

  std::vector<int> get_local_parity_block_num_per_local_group_lotuslrc(int k, int r, int z)
  {
    std::vector<int> local_parity_block_num_per_local_group;
    std::vector<int> local_group_sizes = get_lotuslrc_local_group_sizes(k, r, z);
    int local_group_num = local_group_sizes.size();
    if (local_group_num != z / 2)
      throw std::runtime_error("local group num is not equal to z / 2");
    for (int i = 0; i < local_group_num; i++)
      local_parity_block_num_per_local_group.push_back(2);
    return local_parity_block_num_per_local_group;
  }

  int get_lotuslrc_local_group_id_to_block_id(int k, int r, int z, int local_group_id)
  {
    (void)k;
    (void)r;
    (void)z;
    (void)local_group_id;
    return -1; // TODO
  }

  int get_lotuslrc_block_id_to_local_group_id(int k, int r, int z, int block_id)
  {
    if (block_id < k)
    {
      std::vector<int> data_block_num_per_local_group = get_data_block_num_per_local_group_lotuslrc(k, r, z);
      int cur_sum = 0;
      for (size_t i = 0; i < data_block_num_per_local_group.size(); i++)
      {
        cur_sum += data_block_num_per_local_group[i];
        if (block_id < cur_sum)
          return (int)i;
      }
    }
    else if (block_id < k + r)
    {
      std::vector<int> global_parity_block_num_per_local_group = get_global_parity_block_num_per_local_group_lotuslrc(k, r, z);
      int cur_sum = 0;
      for (size_t i = 0; i < global_parity_block_num_per_local_group.size(); i++)
      {
        cur_sum += global_parity_block_num_per_local_group[i];
        if ((block_id - k) < cur_sum)
          return (int)i;
      }
    }
    else
    {
      return (block_id - k - r) / 2;
    }
    throw std::runtime_error("block id is out of range");
  }

  std::vector<int> get_lotuslrc_group_sizes(int k, int r, int z)
  {
    std::vector<int> local_group_sizes = get_lotuslrc_local_group_sizes(k, r, z);
    std::vector<int> group_num_per_local_group = get_lotuslrc_group_num_per_local_group(k, r, z);
    std::vector<int> group_sizes;
    for (size_t i = 0; i < local_group_sizes.size(); i++)
    {
      int group_num = group_num_per_local_group[i];
      if (group_num <= 0 || group_num > local_group_sizes[i])
        throw std::runtime_error("LotusLRC invalid placement group num for local group");
      int group_size = local_group_sizes[i] / group_num;
      int larger_group_num = local_group_sizes[i] % group_num;
      for (int j = 0; j < larger_group_num; j++)
        group_sizes.push_back(group_size + 1);
      for (int j = 0; j < group_num - larger_group_num; j++)
        group_sizes.push_back(group_size);
    }
    return group_sizes;
  }

  std::vector<int> get_lotuslrc_local_group_sizes(int k, int r, int z)
  {
    // size_i = data_i + global_i + 2 (keeps Option B consistent after unfused
    // data counts are rearranged more-first).
    std::vector<int> data_block_num_per_local_group =
        get_data_block_num_per_local_group_lotuslrc(k, r, z);
    std::vector<int> global_parity_block_num_per_local_group =
        get_global_parity_block_num_per_local_group_lotuslrc(k, r, z);
    std::vector<int> group_sizes;
    group_sizes.reserve(data_block_num_per_local_group.size());
    for (size_t i = 0; i < data_block_num_per_local_group.size(); i++)
      group_sizes.push_back(data_block_num_per_local_group[i] +
                            global_parity_block_num_per_local_group[i] + 2);
    return group_sizes;
  }

  std::vector<int> get_lotuslrc_group_num_per_local_group(int k, int r, int z)
  {
    // Each global/local parity occupies its own placement subgroup in the
    // current layout, so subgroup count must be at least globals + 2 LPs.
    std::vector<int> group_num_per_local_group;
    int max_capacity = r + 2;
    std::vector<int> local_group_sizes = get_lotuslrc_local_group_sizes(k, r, z);
    std::vector<int> global_parity_block_num_per_local_group =
        get_global_parity_block_num_per_local_group_lotuslrc(k, r, z);
    for (size_t i = 0; i < local_group_sizes.size(); i++)
    {
      int min_groups = global_parity_block_num_per_local_group[i] + 2;
      int group_num = local_group_sizes[i] / max_capacity + bool(local_group_sizes[i] % max_capacity);
      if (group_num < min_groups)
        group_num = min_groups;
      if (group_num > local_group_sizes[i])
        throw std::runtime_error("LotusLRC local group too small for its parity blocks");
      group_num_per_local_group.push_back(group_num);
    }
    return group_num_per_local_group;
  }

} // namespace ECProject
