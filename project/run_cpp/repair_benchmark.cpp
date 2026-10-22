#include "client.h"
#include "config.h"

#include <algorithm>
#include <chrono>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <unistd.h>

namespace {

struct RepairCase
{
  std::string failed_name;
  int failed_block_id;
  std::vector<int> source_block_ids;
};

struct RepairTask
{
  std::vector<int> failed_block_ids;
  std::vector<int> source_block_ids;
};

struct DoubleRepairCase
{
  std::string failed_name;
  std::vector<RepairTask> tasks;
  bool optimal_g0_d0_chain = false;
};

struct RepairPlan
{
  std::string code_type;
  int k = 0;
  int required_r = 0;
  int required_z = 0;
  std::vector<RepairCase> single_cases;
  std::vector<DoubleRepairCase> double_cases;
};

void append_range(std::vector<int> &ids, int first, int last)
{
  for (int id = first; id <= last; ++id)
    ids.push_back(id);
}

std::vector<DoubleRepairCase> make_uniform_k96_double_block_cases()
{
  std::vector<DoubleRepairCase> cases;

  RepairTask g0_g1{{96, 97}, {}};
  append_range(g0_g1.source_block_ids, 0, 95);
  cases.push_back({"G0+G1", {g0_g1}, false});

  return cases;
}

std::vector<RepairCase> make_uniform_k96_single_block_cases(int r)
{
  const int first_global_parity_id = 96;
  const int first_local_parity_id = 96 + r;
  std::vector<RepairCase> cases;

  RepairCase l0{"L0", first_local_parity_id, {}};
  append_range(l0.source_block_ids, 0, 24);
  cases.push_back(l0);

  RepairCase l3{"L3", first_local_parity_id + 3, {}};
  append_range(l3.source_block_ids, 75, 95);
  append_range(l3.source_block_ids, first_global_parity_id,
               first_global_parity_id + r - 1);
  cases.push_back(l3);

  return cases;
}

std::vector<DoubleRepairCase> make_uniform_k72_double_block_cases()
{
  std::vector<DoubleRepairCase> cases;

  RepairTask g0_g1{{72, 73}, {}};
  append_range(g0_g1.source_block_ids, 0, 71);
  cases.push_back({"G0+G1", {g0_g1}, false});

  return cases;
}

std::vector<RepairCase> make_uniform_k72_single_block_cases(int r)
{
  const int first_local_parity_id = 72 + r;
  std::vector<RepairCase> cases;

  RepairCase l0{"L0", first_local_parity_id, {}};
  append_range(l0.source_block_ids, 0, 18);
  cases.push_back(l0);

  return cases;
}

std::vector<DoubleRepairCase> make_uniform_k48_double_block_cases()
{
  std::vector<DoubleRepairCase> cases;

  RepairTask g0_g1{{48, 49}, {}};
  append_range(g0_g1.source_block_ids, 0, 47);
  cases.push_back({"G0+G1", {g0_g1}, false});

  return cases;
}

std::vector<RepairCase> make_uniform_k48_single_block_cases(int r)
{
  const int first_local_parity_id = 48 + r;
  std::vector<RepairCase> cases;

  RepairCase l0{"L0", first_local_parity_id, {}};
  append_range(l0.source_block_ids, 0, 11);
  cases.push_back(l0);

  RepairCase l1{"L1", first_local_parity_id + 1, {}};
  append_range(l1.source_block_ids, 12, 24);
  cases.push_back(l1);

  return cases;
}

std::vector<DoubleRepairCase> make_uniform_k24_double_block_cases()
{
  std::vector<DoubleRepairCase> cases;

  RepairTask g0_g1{{24, 25}, {}};
  append_range(g0_g1.source_block_ids, 0, 23);
  cases.push_back({"G0+G1", {g0_g1}, false});

  return cases;
}

std::vector<RepairCase> make_uniform_k24_single_block_cases(int r)
{
  const int first_local_parity_id = 24 + r;
  std::vector<RepairCase> cases;

  RepairCase l0{"L0", first_local_parity_id, {}};
  append_range(l0.source_block_ids, 0, 12);
  cases.push_back(l0);

  return cases;
}

std::vector<DoubleRepairCase> make_optimal_k48_double_block_cases()
{
  std::vector<DoubleRepairCase> cases;

  RepairTask recover_g0{{48}, {}};
  append_range(recover_g0.source_block_ids, 12, 23);
  recover_g0.source_block_ids.push_back(49);
  recover_g0.source_block_ids.push_back(50);
  recover_g0.source_block_ids.push_back(52);
  RepairTask recover_d0{{0}, {}};
  append_range(recover_d0.source_block_ids, 1, 11);
  recover_d0.source_block_ids.push_back(51);
  cases.push_back({"G0+D0", {recover_g0, recover_d0}, true});

  RepairTask g0_g1{{48, 49}, {}};
  append_range(g0_g1.source_block_ids, 0, 47);
  cases.push_back({"G0+G1", {g0_g1}, false});

  return cases;
}

std::vector<DoubleRepairCase> make_optimal_k24_double_block_cases()
{
  std::vector<DoubleRepairCase> cases;

  RepairTask g0_g1{{24, 25}, {}};
  append_range(g0_g1.source_block_ids, 0, 23);
  cases.push_back({"G0+G1", {g0_g1}, false});

  RepairTask recover_g0{{24}, {}};
  append_range(recover_g0.source_block_ids, 12, 23);
  recover_g0.source_block_ids.push_back(25);
  recover_g0.source_block_ids.push_back(27);
  RepairTask recover_d0{{0}, {}};
  append_range(recover_d0.source_block_ids, 1, 11);
  recover_d0.source_block_ids.push_back(26);
  cases.push_back({"G0+D0", {recover_g0, recover_d0}, true});

  return cases;
}

std::vector<DoubleRepairCase> make_optimal_k72_double_block_cases()
{
  std::vector<DoubleRepairCase> cases;

  RepairTask recover_g0{{72}, {}};
  append_range(recover_g0.source_block_ids, 18, 35);
  recover_g0.source_block_ids.push_back(73);
  recover_g0.source_block_ids.push_back(74);
  recover_g0.source_block_ids.push_back(75);
  recover_g0.source_block_ids.push_back(77);
  RepairTask recover_d0{{0}, {}};
  append_range(recover_d0.source_block_ids, 1, 17);
  recover_d0.source_block_ids.push_back(76);
  cases.push_back({"G0+D0", {recover_g0, recover_d0}, true});

  RepairTask g0_g1{{72, 73}, {}};
  append_range(g0_g1.source_block_ids, 0, 71);
  cases.push_back({"G0+G1", {g0_g1}, false});

  return cases;
}

std::vector<DoubleRepairCase> make_optimal_k96_double_block_cases()
{
  std::vector<DoubleRepairCase> cases;

  RepairTask recover_g0{{96}, {}};
  append_range(recover_g0.source_block_ids, 24, 47);
  recover_g0.source_block_ids.push_back(97);
  recover_g0.source_block_ids.push_back(98);
  recover_g0.source_block_ids.push_back(99);
  recover_g0.source_block_ids.push_back(100);
  recover_g0.source_block_ids.push_back(102);
  RepairTask recover_d0{{0}, {}};
  append_range(recover_d0.source_block_ids, 1, 23);
  recover_d0.source_block_ids.push_back(101);
  cases.push_back({"G0+D0", {recover_g0, recover_d0}, true});

  RepairTask g0_g1{{96, 97}, {}};
  append_range(g0_g1.source_block_ids, 0, 95);
  cases.push_back({"G0+G1", {g0_g1}, false});

  return cases;
}

std::vector<RepairCase> make_optimal_k96_single_block_cases(int r)
{
  const int first_local_parity_id = 96 + r;
  std::vector<RepairCase> cases;

  RepairCase l0{"L0", first_local_parity_id, {}};
  append_range(l0.source_block_ids, 0, 23);
  for (int global_id = 96; global_id < 96 + r; ++global_id)
    l0.source_block_ids.push_back(global_id);
  cases.push_back(l0);

  return cases;
}

std::vector<RepairCase> make_optimal_k72_single_block_cases(int r)
{
  const int first_local_parity_id = 72 + r;
  std::vector<RepairCase> cases;

  RepairCase l0{"L0", first_local_parity_id, {}};
  append_range(l0.source_block_ids, 0, 17);
  for (int global_id = 72; global_id < 72 + r; ++global_id)
    l0.source_block_ids.push_back(global_id);
  cases.push_back(l0);

  return cases;
}

std::vector<RepairCase> make_optimal_k48_single_block_cases(int r)
{
  const int first_local_parity_id = 48 + r;
  std::vector<RepairCase> cases;

  RepairCase l0{"L0", first_local_parity_id, {}};
  append_range(l0.source_block_ids, 0, 11);
  l0.source_block_ids.push_back(48);
  l0.source_block_ids.push_back(49);
  l0.source_block_ids.push_back(50);
  cases.push_back(l0);

  return cases;
}

std::vector<RepairCase> make_optimal_k24_single_block_cases(int r)
{
  const int first_local_parity_id = 24 + r;
  std::vector<RepairCase> cases;

  RepairCase l0{"L0", first_local_parity_id, {}};
  append_range(l0.source_block_ids, 0, 11);
  l0.source_block_ids.push_back(24);
  l0.source_block_ids.push_back(25);
  cases.push_back(l0);

  return cases;
}

std::vector<RepairCase> make_azure_k24_single_block_cases(int r)
{
  const int first_local_parity_id = 24 + r;
  std::vector<RepairCase> cases;

  RepairCase l0{"L0", first_local_parity_id, {}};
  append_range(l0.source_block_ids, 0, 11);
  cases.push_back(l0);

  RepairCase g0{"G0", 24, {}};
  append_range(g0.source_block_ids, 0, 23);
  cases.push_back(g0);

  return cases;
}

std::vector<RepairCase> make_azure_k48_single_block_cases(int r)
{
  const int first_local_parity_id = 48 + r;
  std::vector<RepairCase> cases;

  RepairCase l0{"L0", first_local_parity_id, {}};
  append_range(l0.source_block_ids, 0, 11);
  cases.push_back(l0);

  RepairCase g0{"G0", 48, {}};
  append_range(g0.source_block_ids, 0, 47);
  cases.push_back(g0);

  return cases;
}

std::vector<RepairCase> make_azure_k72_single_block_cases(int r)
{
  const int first_local_parity_id = 72 + r;
  std::vector<RepairCase> cases;

  RepairCase l0{"L0", first_local_parity_id, {}};
  append_range(l0.source_block_ids, 0, 17);
  cases.push_back(l0);

  RepairCase g0{"G0", 72, {}};
  append_range(g0.source_block_ids, 0, 71);
  cases.push_back(g0);

  return cases;
}

std::vector<RepairCase> make_azure_k96_single_block_cases(int r)
{
  const int first_local_parity_id = 96 + r;
  std::vector<RepairCase> cases;

  RepairCase l0{"L0", first_local_parity_id, {}};
  append_range(l0.source_block_ids, 0, 23);
  cases.push_back(l0);

  RepairCase g0{"G0", 96, {}};
  append_range(g0.source_block_ids, 0, 95);
  cases.push_back(g0);

  return cases;
}

std::vector<DoubleRepairCase> make_azure_k96_double_block_cases()
{
  std::vector<DoubleRepairCase> cases;

  RepairTask g0_g1{{96, 97}, {}};
  append_range(g0_g1.source_block_ids, 0, 95);
  cases.push_back({"G0+G1", {g0_g1}});

  return cases;
}

std::vector<DoubleRepairCase> make_azure_k72_double_block_cases()
{
  std::vector<DoubleRepairCase> cases;

  RepairTask g0_g1{{72, 73}, {}};
  append_range(g0_g1.source_block_ids, 0, 71);
  cases.push_back({"G0+G1", {g0_g1}});

  return cases;
}

std::vector<DoubleRepairCase> make_azure_k48_double_block_cases()
{
  std::vector<DoubleRepairCase> cases;

  RepairTask g0_g1{{48, 49}, {}};
  append_range(g0_g1.source_block_ids, 0, 47);
  cases.push_back({"G0+G1", {g0_g1}});

  return cases;
}

std::vector<DoubleRepairCase> make_azure_k24_double_block_cases()
{
  std::vector<DoubleRepairCase> cases;

  RepairTask g0_g1{{24, 25}, {}};
  append_range(g0_g1.source_block_ids, 0, 23);
  cases.push_back({"G0+G1", {g0_g1}});

  return cases;
}

std::vector<RepairCase> make_lotus_k24_single_block_cases(int r)
{
  const int first_local_parity_id = 24 + r;
  std::vector<RepairCase> cases;

  RepairCase l0{"L0", first_local_parity_id, {}};
  append_range(l0.source_block_ids, 0, 11);
  cases.push_back(l0);

  RepairCase l2{"L2", first_local_parity_id + 2, {}};
  append_range(l2.source_block_ids, 12, 23);
  l2.source_block_ids.push_back(24);
  cases.push_back(l2);

  return cases;
}

std::vector<RepairCase> make_lotus_k48_single_block_cases()
{
  std::vector<RepairCase> cases;

  RepairCase d0{"D0", 0, {1, 2, 3, 4, 5, 50}};
  cases.push_back(d0);

  RepairCase l2{"L2", 52, {}};
  append_range(l2.source_block_ids, 12, 23);
  cases.push_back(l2);

  RepairCase l5{"L5", 55, {}};
  append_range(l5.source_block_ids, 24, 35);
  l5.source_block_ids.push_back(48);
  cases.push_back(l5);

  RepairCase l3{"L3", 53, {}};
  append_range(l3.source_block_ids, 12, 23);
  append_range(l3.source_block_ids, 0, 5);
  cases.push_back(l3);

  RepairCase l4{"L4", 54, {}};
  append_range(l4.source_block_ids, 24, 35);
  append_range(l4.source_block_ids, 6, 11);
  l4.source_block_ids.push_back(48);
  cases.push_back(l4);

  return cases;
}

std::vector<RepairCase> make_lotus_k72_single_block_cases(int r)
{
  const int first_local_parity_id = 72 + r;
  std::vector<RepairCase> cases;

  RepairCase l1{"L1", first_local_parity_id + 1, {}};
  append_range(l1.source_block_ids, 10, 18);
  cases.push_back(l1);

  RepairCase l0{"L0", first_local_parity_id, {}};
  append_range(l0.source_block_ids, 0, 9);
  cases.push_back(l0);

  RepairCase l2{"L2", first_local_parity_id + 2, {}};
  append_range(l2.source_block_ids, 19, 36);
  cases.push_back(l2);

  RepairCase l4{"L4", first_local_parity_id + 4, {}};
  append_range(l4.source_block_ids, 37, 54);
  l4.source_block_ids.push_back(72);
  cases.push_back(l4);

  RepairCase l3{"L3", first_local_parity_id + 3, {}};
  append_range(l3.source_block_ids, 19, 36);
  append_range(l3.source_block_ids, 0, 9);
  cases.push_back(l3);

  return cases;
}

std::vector<RepairCase> make_lotus_k96_single_block_cases(int r)
{
  const int first_local_parity_id = 96 + r;
  std::vector<RepairCase> cases;

  RepairCase l1{"L1", first_local_parity_id + 1, {}};
  append_range(l1.source_block_ids, 13, 24);
  cases.push_back(l1);

  RepairCase l0{"L0", first_local_parity_id, {}};
  append_range(l0.source_block_ids, 0, 12);
  cases.push_back(l0);

  RepairCase l2{"L2", first_local_parity_id + 2, {}};
  append_range(l2.source_block_ids, 25, 49);
  cases.push_back(l2);

  RepairCase l4{"L4", first_local_parity_id + 4, {}};
  append_range(l4.source_block_ids, 50, 72);
  l4.source_block_ids.push_back(96);
  l4.source_block_ids.push_back(97);
  cases.push_back(l4);

  RepairCase l3{"L3", first_local_parity_id + 3, {}};
  append_range(l3.source_block_ids, 25, 49);
  append_range(l3.source_block_ids, 0, 12);
  cases.push_back(l3);

  return cases;
}

std::vector<DoubleRepairCase> make_lotus_k24_double_block_cases(int r)
{
  const int first_local_parity_id = 24 + r;
  std::vector<DoubleRepairCase> cases;

  RepairTask l2_l3{{first_local_parity_id + 2, first_local_parity_id + 3}, {}};
  append_range(l2_l3.source_block_ids, 12, 23);
  l2_l3.source_block_ids.push_back(24);
  cases.push_back({"L2+L3", {l2_l3}});

  return cases;
}

std::vector<DoubleRepairCase> make_lotus_k48_double_block_cases()
{
  std::vector<DoubleRepairCase> cases;

  RepairTask l0{{50}, {}};
  append_range(l0.source_block_ids, 0, 5);
  RepairTask l1{{51}, {}};
  append_range(l1.source_block_ids, 6, 11);
  cases.push_back({"L0+L1", {l0, l1}});

  RepairTask l6_l7{{56, 57}, {}};
  append_range(l6_l7.source_block_ids, 36, 47);
  l6_l7.source_block_ids.push_back(49);
  cases.push_back({"L6+L7", {l6_l7}});

  RepairTask l1_again{{51}, {}};
  append_range(l1_again.source_block_ids, 6, 11);
  RepairTask l4{{54}, {}};
  append_range(l4.source_block_ids, 6, 11);
  append_range(l4.source_block_ids, 24, 35);
  l4.source_block_ids.push_back(48);
  cases.push_back({"L1+L4", {l1_again, l4}});

  return cases;
}

std::vector<DoubleRepairCase> make_lotus_k72_double_block_cases(int r)
{
  const int first_local_parity_id = 72 + r;
  std::vector<DoubleRepairCase> cases;

  RepairTask l6_l7{{first_local_parity_id + 6, first_local_parity_id + 7}, {}};
  append_range(l6_l7.source_block_ids, 55, 71);
  l6_l7.source_block_ids.push_back(73);
  l6_l7.source_block_ids.push_back(74);
  cases.push_back({"L6+L7", {l6_l7}});

  RepairTask l0{{first_local_parity_id}, {}};
  append_range(l0.source_block_ids, 0, 9);
  RepairTask l3{{first_local_parity_id + 3}, {}};
  append_range(l3.source_block_ids, 0, 9);
  append_range(l3.source_block_ids, 19, 36);
  cases.push_back({"L0+L3", {l0, l3}});

  return cases;
}

std::vector<DoubleRepairCase> make_lotus_k96_double_block_cases(int r)
{
  const int first_local_parity_id = 96 + r;
  std::vector<DoubleRepairCase> cases;

  RepairTask l6_l7{{first_local_parity_id + 6, first_local_parity_id + 7}, {}};
  append_range(l6_l7.source_block_ids, 73, 95);
  l6_l7.source_block_ids.push_back(99);
  l6_l7.source_block_ids.push_back(98);
  cases.push_back({"L6+L7", {l6_l7}});

  RepairTask l0{{first_local_parity_id}, {}};
  append_range(l0.source_block_ids, 0, 12);
  RepairTask l3{{first_local_parity_id + 3}, {}};
  append_range(l3.source_block_ids, 25, 49);
  append_range(l3.source_block_ids, 0, 12);
  cases.push_back({"L0+L3", {l0, l3}});

  return cases;
}

bool make_repair_plan(const std::string &code_type, int k,
                      const std::string &failure_type, int configured_r,
                      RepairPlan &plan, std::string &error)
{
  plan = RepairPlan{};
  plan.code_type = code_type;
  plan.k = k;

  if (code_type == "AzureLRC")
  {
    if (k == 24)
    {
      if (failure_type != "single" && failure_type != "double")
      {
        error = "no repair plan is registered for CodeType=AzureLRC, k=24, failure=" +
                failure_type;
        return false;
      }
      plan.required_r = 2;
      plan.required_z = 2;
      if (configured_r != plan.required_r)
      {
        error = "AzureLRC k=24 requires r=2";
        return false;
      }
      if (failure_type == "single")
        plan.single_cases = make_azure_k24_single_block_cases(configured_r);
      else
        plan.double_cases = make_azure_k24_double_block_cases();
      return true;
    }

    if (k == 48 && (failure_type == "single" || failure_type == "double"))
    {
      plan.required_r = 3;
      plan.required_z = 4;
      if (configured_r != plan.required_r)
      {
        error = "AzureLRC k=48 requires r=3";
        return false;
      }
      if (failure_type == "single")
        plan.single_cases = make_azure_k48_single_block_cases(configured_r);
      else
        plan.double_cases = make_azure_k48_double_block_cases();
      return true;
    }

    if (k == 72 && (failure_type == "single" || failure_type == "double"))
    {
      plan.required_r = 4;
      plan.required_z = 4;
      if (configured_r != plan.required_r)
      {
        error = "AzureLRC k=72 requires r=4";
        return false;
      }
      if (failure_type == "single")
        plan.single_cases = make_azure_k72_single_block_cases(configured_r);
      else
        plan.double_cases = make_azure_k72_double_block_cases();
      return true;
    }

    if (k == 96 && (failure_type == "single" || failure_type == "double"))
    {
      plan.required_r = 5;
      plan.required_z = 4;
      if (configured_r != plan.required_r)
      {
        error = "AzureLRC k=96 requires r=5";
        return false;
      }
      if (failure_type == "single")
        plan.single_cases = make_azure_k96_single_block_cases(configured_r);
      else
        plan.double_cases = make_azure_k96_double_block_cases();
      return true;
    }

    error = "no repair plan is registered for CodeType=AzureLRC, k=" +
            std::to_string(k) + ", failure=" + failure_type;
    return false;
  }

  if (code_type == "OptimalLRC")
  {
    if (k == 24 && (failure_type == "single" || failure_type == "double"))
    {
      plan.required_r = 2;
      plan.required_z = 2;
      if (configured_r != plan.required_r)
      {
        error = "OptimalLRC k=24 requires r=2";
        return false;
      }
      if (failure_type == "single")
        plan.single_cases = make_optimal_k24_single_block_cases(configured_r);
      else
        plan.double_cases = make_optimal_k24_double_block_cases();
      return true;
    }

    if (k == 48 && (failure_type == "single" || failure_type == "double"))
    {
      plan.required_r = 3;
      plan.required_z = 4;
      if (configured_r != plan.required_r)
      {
        error = "OptimalLRC k=48 requires r=3";
        return false;
      }
      if (failure_type == "single")
        plan.single_cases = make_optimal_k48_single_block_cases(configured_r);
      else
        plan.double_cases = make_optimal_k48_double_block_cases();
      return true;
    }

    if (k == 72 && (failure_type == "single" || failure_type == "double"))
    {
      plan.required_r = 4;
      plan.required_z = 4;
      if (configured_r != plan.required_r)
      {
        error = "OptimalLRC k=72 requires r=4";
        return false;
      }
      if (failure_type == "single")
        plan.single_cases = make_optimal_k72_single_block_cases(configured_r);
      else
        plan.double_cases = make_optimal_k72_double_block_cases();
      return true;
    }

    if (k == 96 && (failure_type == "single" || failure_type == "double"))
    {
      plan.required_r = 5;
      plan.required_z = 4;
      if (configured_r != plan.required_r)
      {
        error = "OptimalLRC k=96 requires r=5";
        return false;
      }
      if (failure_type == "single")
        plan.single_cases = make_optimal_k96_single_block_cases(configured_r);
      else
        plan.double_cases = make_optimal_k96_double_block_cases();
      return true;
    }

    error = "no repair plan is registered for CodeType=OptimalLRC, k=" +
            std::to_string(k) + ", failure=" + failure_type;
    return false;
  }

  if (code_type == "UniformLRC")
  {
    if (k == 24 && (failure_type == "single" || failure_type == "double"))
    {
      plan.required_r = 2;
      plan.required_z = 2;
      if (configured_r != plan.required_r)
      {
        error = "UniformLRC k=24 requires r=2";
        return false;
      }
      if (failure_type == "single")
        plan.single_cases = make_uniform_k24_single_block_cases(configured_r);
      else
        plan.double_cases = make_uniform_k24_double_block_cases();
      return true;
    }

    if (k == 48 && (failure_type == "single" || failure_type == "double"))
    {
      plan.required_r = 3;
      plan.required_z = 4;
      if (configured_r != plan.required_r)
      {
        error = "UniformLRC k=48 requires r=3";
        return false;
      }
      if (failure_type == "single")
        plan.single_cases = make_uniform_k48_single_block_cases(configured_r);
      else
        plan.double_cases = make_uniform_k48_double_block_cases();
      return true;
    }

    if (k == 72 && (failure_type == "single" || failure_type == "double"))
    {
      plan.required_r = 4;
      plan.required_z = 4;
      if (configured_r != plan.required_r)
      {
        error = "UniformLRC k=72 requires r=4";
        return false;
      }
      if (failure_type == "single")
        plan.single_cases = make_uniform_k72_single_block_cases(configured_r);
      else
        plan.double_cases = make_uniform_k72_double_block_cases();
      return true;
    }

    if (k == 96 && (failure_type == "single" || failure_type == "double"))
    {
      plan.required_r = 5;
      plan.required_z = 4;
      if (configured_r != plan.required_r)
      {
        error = "UniformLRC k=96 requires r=5";
        return false;
      }
      if (failure_type == "single")
        plan.single_cases = make_uniform_k96_single_block_cases(configured_r);
      else
        plan.double_cases = make_uniform_k96_double_block_cases();
      return true;
    }

    error = "no repair plan is registered for CodeType=UniformLRC, k=" +
            std::to_string(k) + ", failure=" + failure_type;
    return false;
  }

  if (code_type != "LotusLRC")
  {
    error = "unsupported CodeType=" + code_type;
    return false;
  }

  plan.required_r = k / 24;
  plan.required_z = (k == 24) ? 4 : 8;
  if (configured_r != plan.required_r)
  {
    error = "LotusLRC k=" + std::to_string(k) + " requires r=" +
            std::to_string(plan.required_r);
    return false;
  }

  if (failure_type == "single")
  {
    if (k == 24)
      plan.single_cases = make_lotus_k24_single_block_cases(configured_r);
    else if (k == 48)
      plan.single_cases = make_lotus_k48_single_block_cases();
    else if (k == 72)
      plan.single_cases = make_lotus_k72_single_block_cases(configured_r);
    else if (k == 96)
      plan.single_cases = make_lotus_k96_single_block_cases(configured_r);
  }
  else if (failure_type == "double")
  {
    if (k == 24)
      plan.double_cases = make_lotus_k24_double_block_cases(configured_r);
    else if (k == 48)
      plan.double_cases = make_lotus_k48_double_block_cases();
    else if (k == 72)
      plan.double_cases = make_lotus_k72_double_block_cases(configured_r);
    else if (k == 96)
      plan.double_cases = make_lotus_k96_double_block_cases(configured_r);
  }

  if (plan.single_cases.empty() && plan.double_cases.empty())
  {
    error = "no repair plan is registered for CodeType=" + code_type +
            ", k=" + std::to_string(k) + ", failure=" + failure_type;
    return false;
  }
  return true;
}

bool is_supported_k(int k)
{
  return k == 24 || k == 48 || k == 72 || k == 96;
}

bool is_supported_failure_type(const std::string &failure_type)
{
  return failure_type == "single" || failure_type == "double" || failure_type == "multi";
}

std::string config_path_from_executable(const char *argv0)
{
  char cwd[4096];
  if (getcwd(cwd, sizeof(cwd)) == nullptr)
    throw std::runtime_error("cannot determine current working directory");

  std::string executable(argv0);
  const size_t slash = executable.rfind('/');
  if (slash == std::string::npos)
    return std::string(cwd) + "/project/config/parameterConfiguration.xml";

  std::string directory = executable.substr(0, slash);
  if (directory.empty() || directory.front() != '/')
    directory = std::string(cwd) + "/" + directory;
  const std::string direct_build_path = directory + "/../config/parameterConfiguration.xml";
  if (access(direct_build_path.c_str(), R_OK) == 0)
    return direct_build_path;
  return directory + "/../../config/parameterConfiguration.xml";
}

void print_ids(const std::vector<int> &ids)
{
  for (size_t i = 0; i < ids.size(); ++i)
    std::cout << (i == 0 ? "" : ",") << ids[i];
}

} // namespace

int main(int argc, char **argv)
{
  if (argc != 3)
  {
    std::cerr << "Usage: " << argv[0] << " <24|48|72|96> <single|double|multi>" << std::endl;
    return 2;
  }

  int requested_k = 0;
  try
  {
    size_t parsed = 0;
    requested_k = std::stoi(argv[1], &parsed);
    if (parsed != std::string(argv[1]).size())
      throw std::invalid_argument("trailing characters");
  }
  catch (const std::exception &)
  {
    std::cerr << "Error: encoding parameter must be one of 24, 48, 72, or 96." << std::endl;
    return 2;
  }

  const std::string failure_type = argv[2];
  if (!is_supported_k(requested_k))
  {
    std::cerr << "Error: unsupported encoding parameter " << requested_k
              << "; expected 24, 48, 72, or 96." << std::endl;
    return 2;
  }
  if (!is_supported_failure_type(failure_type))
  {
    std::cerr << "Error: unsupported failure type '" << failure_type
              << "'; expected single, double, or multi." << std::endl;
    return 2;
  }

  const std::string config_path = config_path_from_executable(argv[0]);
  const ECProject::Config *config = nullptr;
  try
  {
    config = ECProject::Config::getInstance(config_path);
  }
  catch (const std::exception &error)
  {
    std::cerr << "Error: failed to load " << config_path << ": " << error.what() << std::endl;
    return 1;
  }

  if (requested_k != config->k)
  {
    std::cerr << "Error: terminal parameter k=" << requested_k
              << " does not match XML parameter k=" << config->k << "." << std::endl;
    return 1;
  }

  std::cout << "Current code type: " << config->CodeType << std::endl;

  RepairPlan repair_plan;
  std::string plan_error;
  if (!make_repair_plan(config->CodeType, requested_k, failure_type,
                        config->r, repair_plan, plan_error))
  {
    std::cerr << "Error: " << plan_error << "." << std::endl;
    return 3;
  }
  if (config->z != repair_plan.required_z)
  {
    std::cerr << "Error: repair plan for " << repair_plan.code_type
              << " with k=" << requested_k << " requires r="
              << repair_plan.required_r << ", z=" << repair_plan.required_z
              << "; XML has r=" << config->r << ", z=" << config->z << "."
              << std::endl;
    return 1;
  }

  const std::string client_ip = config->ClientIP;
  const int client_port = 44444;
  ECProject::Client client(client_ip, client_port,
      config->CoordinatorIP + ":" + std::to_string(config->CoordinatorPort), config_path);

  std::cout << client.sayHelloToCoordinatorByGrpc(
      "Repair benchmark (" + config->CodeType + "): " + client_ip + ":" +
      std::to_string(client_port)) << std::endl;
  if (!client.set())
  {
    std::cerr << "Error: failed to create the benchmark stripe." << std::endl;
    return 1;
  }

  std::vector<int> stripe_ids = client.list_stripes();
  if (stripe_ids.empty())
  {
    std::cerr << "Error: no stripe found after benchmark data was written." << std::endl;
    return 1;
  }
  const int stripe_id = *std::max_element(stripe_ids.begin(), stripe_ids.end());

  std::cout << "Repair benchmark: code=" << repair_plan.code_type
            << ", k=" << config->k << ", r=" << config->r
            << ", z=" << config->z << ", failure=" << failure_type
            << ", stripe=" << stripe_id << std::endl;
  bool all_succeeded = true;
  if (failure_type == "single")
  {
    for (const RepairCase &test_case : repair_plan.single_cases)
    {
      std::cout << "Case " << test_case.failed_name << " (block " << test_case.failed_block_id
                << "), sources=[";
      print_ids(test_case.source_block_ids);
      std::cout << "]" << std::endl;

      double recovery_time = 0.0;
      const bool succeeded = client.recovery_with_sources(
          stripe_id, test_case.failed_block_id, test_case.source_block_ids, recovery_time);
      if (succeeded)
        std::cout << "Result [" << repair_plan.code_type << "] " << test_case.failed_name << ": " << recovery_time << " s" << std::endl;
      else
      {
        std::cout << "Result [" << repair_plan.code_type << "] " << test_case.failed_name << ": FAILED after "
                  << recovery_time << " s" << std::endl;
        all_succeeded = false;
      }
    }
  }
  else
  {
    for (const DoubleRepairCase &test_case : repair_plan.double_cases)
    {
      std::cout << "Case " << test_case.failed_name << std::endl;
      std::vector<std::vector<int>> failed_ids;
      std::vector<std::vector<int>> source_ids;
      for (const RepairTask &task : test_case.tasks)
      {
        std::cout << "  task failed=[";
        print_ids(task.failed_block_ids);
        std::cout << "], sources=[";
        print_ids(task.source_block_ids);
        std::cout << "]" << std::endl;
        failed_ids.push_back(task.failed_block_ids);
        source_ids.push_back(task.source_block_ids);
      }

      double recovery_time = 0.0;
      const bool succeeded = client.double_recovery_with_sources(
          stripe_id, failed_ids, source_ids, recovery_time,
          test_case.optimal_g0_d0_chain);
      if (succeeded)
        std::cout << "Result [" << repair_plan.code_type << "] " << test_case.failed_name << ": " << recovery_time << " s" << std::endl;
      else
      {
        std::cout << "Result [" << repair_plan.code_type << "] " << test_case.failed_name << ": FAILED after "
                  << recovery_time << " s" << std::endl;
        all_succeeded = false;
      }
    }
  }

  std::cout << "Benchmark completed: code=" << repair_plan.code_type
            << ", result=" << (all_succeeded ? "SUCCESS" : "FAILED") << std::endl;
  return all_succeeded ? 0 : 1;
}
