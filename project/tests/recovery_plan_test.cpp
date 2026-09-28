/**
 * 单块修复方案单元测试：get_recovery_group_and_block_ids
 */
#include "encoder.h"
#include <iostream>
#include <vector>
#include <cstdlib>

static void print_plan(const std::string &code_type, int failed_block_id,
                       const std::vector<std::pair<int, std::vector<int>>> &plan)
{
  std::cout << "  " << code_type << " failed_block_id=" << failed_block_id << " -> ";
  std::cout << plan.size() << " group(s): ";
  for (size_t i = 0; i < plan.size(); ++i)
  {
    if (i > 0)
      std::cout << "; ";
    std::cout << "g" << plan[i].first << "=[";
    for (size_t j = 0; j < plan[i].second.size(); ++j)
    {
      if (j > 0)
        std::cout << ",";
      std::cout << plan[i].second[j];
    }
    std::cout << "]";
  }
  std::cout << std::endl;
}

static bool test_recovery_plan(const std::string &code_type, int k, int r, int z, int failed_block_id)
{
  auto plan = ECProject::get_recovery_group_and_block_ids(code_type, k, r, z, failed_block_id);
  if (plan.empty())
  {
    std::cerr << "FAIL: " << code_type << " failed_block_id=" << failed_block_id << " returned empty plan" << std::endl;
    return false;
  }
  for (const auto &p : plan)
  {
    for (int bid : p.second)
    {
      if (bid == failed_block_id)
      {
        std::cerr << "FAIL: " << code_type << " failed_block_id=" << failed_block_id
                  << " appears in group " << p.first << " block_ids" << std::endl;
        return false;
      }
    }
  }
  print_plan(code_type, failed_block_id, plan);
  return true;
}

int main()
{
  int k = 12, r = 4, z = 4;
  std::cin >> k >> r >> z;
  int n = k + r + z;

  std::cout << "=== recovery_group_and_block_ids 单元测试 (k=" << k << " r=" << r << " z=" << z << " n=" << n << ") ===" << std::endl;

  const auto require_hlrc_plan = [](int failed, const std::vector<int> &expected) {
    const auto plan = ECProject::get_recovery_group_and_block_ids("HLRC", 24, 1, 4, failed);
    if (plan.size() != 1 || plan[0].first != 0 || plan[0].second != expected)
    {
      std::cerr << "FAIL: unexpected HLRC plan for block " << failed << std::endl;
      std::exit(1);
    }
  };
  require_hlrc_plan(0, {1, 2, 3, 4, 5, 25});
  require_hlrc_plan(25, {0, 1, 2, 3, 4, 5});
  require_hlrc_plan(12, {13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 27});
  require_hlrc_plan(24, {12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 27});
  require_hlrc_plan(27, {12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24});

  const std::vector<std::string> code_types = {"AzureLRC",  "OptimalLRC", "UniformLRC", "LotusLRC"};

  for (const auto &code_type : code_types)
  {
    std::cout << "\n--- " << code_type << " ---" << std::endl;
    int block_num = n;
    if (code_type == "LotusLRC"){
      block_num = k + r - 1 + z * 2;
    }
    for (int failed_block_id = 0; failed_block_id < block_num; ++failed_block_id)
    {
      bool ok = false;
      try
      {
        if(code_type == "LotusLRC"){
          ok = test_recovery_plan(code_type, k, r - 1, z * 2, failed_block_id);
        }
        else
        {
          ok = test_recovery_plan(code_type, k, r, z, failed_block_id);
        }
      }
      catch (const std::exception &e)
      {
        std::cerr << "FAIL: " << code_type << " failed_block_id=" << failed_block_id << " exception: " << e.what() << std::endl;
        return 1;
      }
      if (!ok)
        return 1;
    }
  }

  std::cout << "\n=== All recovery_plan tests passed ===" << std::endl;
  return 0;
}
