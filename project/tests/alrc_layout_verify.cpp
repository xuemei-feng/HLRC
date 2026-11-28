#include "encoder.h"
#include <algorithm>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <unordered_set>
#include <vector>

namespace {
void require(bool condition, const char *message) { if (!condition) throw std::runtime_error(message); }
void verify(int k, int r, int z)
{
  const int n = k + r + z;
  const auto groups = ECProject::get_alrc_group_id_to_block_ids(k, r, z);
  const auto mapping = ECProject::get_alrc_block_id_to_group_id(k, r, z);
  const auto data = ECProject::get_data_block_num_per_group(k, r, z, "ALRC");
  const auto global = ECProject::get_global_parity_block_num_per_group(k, r, z, "ALRC");
  const auto local = ECProject::get_local_parity_block_num_per_group(k, r, z, "ALRC");
  require(groups.size() == data.size() && data.size() == global.size() && global.size() == local.size(), "count vector size mismatch");
  std::vector<int> seen(n, 0);
  int total = 0;
  for (size_t gid = 0; gid < groups.size(); ++gid) {
    const auto &members = groups.at(static_cast<int>(gid));
    require(!members.empty() && members.size() <= static_cast<size_t>(r + 1), "invalid physical group capacity");
    require(static_cast<int>(members.size()) == data[gid] + global[gid] + local[gid], "group count mismatch");
    for (int id : members) {
      require(id >= 0 && id < n, "block id out of range");
      require(++seen[id] == 1, "duplicate block id");
      require(mapping.at(id) == static_cast<int>(gid), "block mapping mismatch");
      ++total;
    }
  }
  require(total == n && std::all_of(seen.begin(), seen.end(), [](int count) { return count == 1; }), "layout is not a partition");
  require(std::accumulate(data.begin(), data.end(), 0) == k, "data count mismatch");
  require(std::accumulate(global.begin(), global.end(), 0) == r, "global count mismatch");
  require(std::accumulate(local.begin(), local.end(), 0) == z, "local count mismatch");
}
}

int main()
{
  try {
    verify(64, 3, 5); verify(96, 3, 7); verify(128, 3, 9);
    std::cout << "ALRC layout verification passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "ALRC layout verification failed: " << error.what() << '\n';
    return 1;
  }
}
