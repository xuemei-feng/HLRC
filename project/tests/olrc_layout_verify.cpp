#include "encoder.h"
#include <algorithm>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <vector>

namespace {
void require(bool condition, const char *message)
{
  if (!condition) throw std::runtime_error(message);
}

void verify(int k, int r, int z)
{
  const int n = k + r + z;
  const auto logical = ECProject::get_olrc_local_group_block_ids(k, r, z);
  const auto groups = ECProject::get_olrc_group_id_to_block_ids(k, r, z);
  const auto mapping = ECProject::get_olrc_block_id_to_group_id(k, r, z);
  const auto data = ECProject::get_data_block_num_per_group(k, r, z, "OLRC");
  const auto global = ECProject::get_global_parity_block_num_per_group(k, r, z, "OLRC");
  const auto local = ECProject::get_local_parity_block_num_per_group(k, r, z, "OLRC");
  require(groups.size() == data.size() && data.size() == global.size() && global.size() == local.size(),
          "count vector size mismatch");
  require(logical.size() == static_cast<size_t>((z + 1) / 2), "logical group count mismatch");

  std::vector<int> seen(n, 0);
  for (size_t gid = 0; gid < groups.size(); ++gid) {
    const auto &members = groups.at(static_cast<int>(gid));
    require(!members.empty() && members.size() <= static_cast<size_t>(r + 1),
            "invalid physical group capacity");
    require(static_cast<int>(members.size()) == data[gid] + global[gid] + local[gid],
            "group count mismatch");
    for (int id : members) {
      require(id >= 0 && id < n, "block id out of range");
      require(++seen[id] == 1, "duplicate block id");
      require(mapping.at(id) == static_cast<int>(gid), "block mapping mismatch");
    }
  }
  require(std::all_of(seen.begin(), seen.end(), [](int count) { return count == 1; }),
          "layout is not a strict partition");
  require(std::accumulate(data.begin(), data.end(), 0) == k, "data count mismatch");
  require(std::accumulate(global.begin(), global.end(), 0) == r, "global count mismatch");
  require(std::accumulate(local.begin(), local.end(), 0) == z, "local count mismatch");

  for (int id = 0; id < n; ++id) {
    const int local_group = ECProject::get_olrc_block_id_to_local_group_id(k, r, z, id);
    if (id >= k && id < k + r) {
      require(local_group == -1, "global parity has a local group");
      for (const auto &members : logical)
        require(std::find(members.begin(), members.end(), id) != members.end(),
                "global parity missing from logical source group");
    } else {
      require(local_group >= 0 && local_group < static_cast<int>(logical.size()),
              "invalid local group mapping");
      require(std::find(logical[local_group].begin(), logical[local_group].end(), id) !=
              logical[local_group].end(), "logical reverse mapping mismatch");
    }
  }
}
}

int main()
{
  try {
    verify(64, 3, 5);
    verify(96, 3, 7);
    verify(128, 3, 9);
    std::cout << "OLRC layout verification passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "OLRC layout verification failed: " << error.what() << '\n';
    return 1;
  }
}
