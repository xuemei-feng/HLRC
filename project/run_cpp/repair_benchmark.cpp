#include "client.h"
#include "config.h"
#include "encoder.h"
#include "repair_benchmark_plan.h"

#include <algorithm>
#include <chrono>
#include <exception>
#include <cmath>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include <unistd.h>

namespace {

using ECProject::RepairBenchmark::DoubleRepairCase;
using ECProject::RepairBenchmark::TripleRepairCase;
using ECProject::RepairBenchmark::QuadrupleRepairCase;
using ECProject::RepairBenchmark::RepairCase;
using ECProject::RepairBenchmark::RepairGroupCase;
using ECProject::RepairBenchmark::RepairTask;

struct RepairPlan
{
  std::string code_type;
  int k = 0;
  int required_r = 0;
  int required_z = 0;
  std::vector<RepairCase> single_cases;
  std::vector<DoubleRepairCase> double_cases;
  std::vector<TripleRepairCase> triple_cases;
  std::vector<QuadrupleRepairCase> quadruple_cases;
  std::vector<RepairGroupCase> muti_cases;
};

void append_range(std::vector<int> &ids, int first, int last)
{
  for (int id = first; id <= last; ++id)
    ids.push_back(id);
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

  // These shared branches own only the three standard LRC registries.
  // NLRC/HLRC use their own parameter validation and registries below.
  const bool standard_lrc = code_type == "AzureLRC" || code_type == "OptimalLRC" ||
                            code_type == "UniformLRC";
  if (standard_lrc && failure_type == "triple")
  {
    const bool azure_large = code_type == "AzureLRC" && (k == 96 || k == 128);
    const bool optimal_large = code_type == "OptimalLRC" && (k == 96 || k == 128);
    const bool uniform_large = code_type == "UniformLRC" && (k == 96 || k == 128);
    const bool supported_k64 =
        (code_type == "AzureLRC" || code_type == "OptimalLRC" ||
         code_type == "UniformLRC") && k == 64;
    if (!azure_large && !optimal_large && !uniform_large && !supported_k64)
    {
      error = "no repair plan is registered for CodeType=" + code_type +
              ", k=" + std::to_string(k) + ", failure=triple";
      return false;
    }
    plan.required_r = 3;
    plan.required_z = (azure_large || optimal_large || uniform_large) ? (k == 96 ? 4 : 5) : 3;
    if (configured_r != plan.required_r)
    {
      error = code_type + " k=" + std::to_string(k) + " triple requires r=3";
      return false;
    }
    if (code_type == "AzureLRC")
      plan.triple_cases = ECProject::RepairBenchmark::make_azure_triple_block_cases(k);
    else if (code_type == "OptimalLRC")
      plan.triple_cases = ECProject::RepairBenchmark::make_optimal_triple_block_cases(k);
    else if (k == 96)
      plan.triple_cases = ECProject::RepairBenchmark::make_uniform_k96_triple_block_cases();
    else if (k == 128)
      plan.triple_cases = ECProject::RepairBenchmark::make_uniform_k128_triple_block_cases();
    else
      plan.triple_cases = ECProject::RepairBenchmark::make_uniform_k64_triple_block_cases();
    return true;
  }

  if (standard_lrc && failure_type == "quadruple")
  {
    const bool azure_large = code_type == "AzureLRC" && (k == 96 || k == 128);
    const bool optimal_large = code_type == "OptimalLRC" && (k == 96 || k == 128);
    const bool uniform_large = code_type == "UniformLRC" && (k == 96 || k == 128);
    const bool supported_k64 =
        (code_type == "AzureLRC" || code_type == "OptimalLRC" || code_type == "UniformLRC") && k == 64;
    if (!azure_large && !optimal_large && !uniform_large && !supported_k64)
    {
      error = "no repair plan is registered for CodeType=" + code_type +
              ", k=" + std::to_string(k) + ", failure=quadruple";
      return false;
    }
    plan.required_r = 3;
    plan.required_z = (azure_large || optimal_large || uniform_large) ? (k == 96 ? 4 : 5) : 3;
    if (configured_r != plan.required_r)
    {
      error = code_type + " k=" + std::to_string(k) + " quadruple requires r=3";
      return false;
    }
    if (code_type == "AzureLRC")
      plan.quadruple_cases = ECProject::RepairBenchmark::make_azure_quadruple_block_cases(k);
    else if (code_type == "OptimalLRC")
      plan.quadruple_cases = ECProject::RepairBenchmark::make_optimal_quadruple_block_cases(k);
    else if (k == 96)
      plan.quadruple_cases = ECProject::RepairBenchmark::make_uniform_k96_quadruple_block_cases();
    else if (k == 128)
      plan.quadruple_cases = ECProject::RepairBenchmark::make_uniform_k128_quadruple_block_cases();
    else
      plan.quadruple_cases = ECProject::RepairBenchmark::make_uniform_k64_quadruple_block_cases();
    return true;
  }

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

    if ((k == 64 || k == 128) && (failure_type == "single" || failure_type == "double"))
    {
      plan.required_r = 3;
      plan.required_z = (k == 64 ? 3 : 5);
      if (configured_r != plan.required_r)
      {
        error = "AzureLRC k=" + std::to_string(k) + " requires r=3";
        return false;
      }
      if (failure_type == "single")
        plan.single_cases = ECProject::RepairBenchmark::make_azure_uneven_single_block_cases(
            k, configured_r, plan.required_z);
      else
        plan.double_cases = ECProject::RepairBenchmark::make_azure_double_block_cases(k);
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
      plan.required_r = 3;
      plan.required_z = 4;
      if (configured_r != plan.required_r)
      {
        error = "AzureLRC k=96 requires r=3";
        return false;
      }
      if (failure_type == "single")
        plan.single_cases = ECProject::RepairBenchmark::make_azure_uneven_single_block_cases(
            k, configured_r, plan.required_z);
      else
        plan.double_cases = ECProject::RepairBenchmark::make_azure_double_block_cases(k);
      return true;
    }

    error = "no repair plan is registered for CodeType=AzureLRC, k=" +
            std::to_string(k) + ", failure=" + failure_type;
    return false;
  }

  if (code_type == "OptimalLRC")
  {
    if ((k == 64 || k == 128) && (failure_type == "single" || failure_type == "double"))
    {
      plan.required_r = 3;
      plan.required_z = k == 64 ? 3 : 5;
      if (configured_r != plan.required_r)
      {
        error = "OptimalLRC k=" + std::to_string(k) + " requires r=3";
        return false;
      }
      if (failure_type == "single") {
        if (k == 64)
          plan.single_cases = ECProject::RepairBenchmark::make_optimal_k64_single_block_cases();
        else
          plan.single_cases = ECProject::RepairBenchmark::make_optimal_uneven_single_block_cases(
              k, configured_r, plan.required_z);
      } else {
        if (k == 64)
          plan.double_cases = ECProject::RepairBenchmark::make_optimal_k64_double_block_cases();
        else
          plan.double_cases = ECProject::RepairBenchmark::make_optimal_uneven_double_block_cases(k);
      }
      return true;
    }

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
      plan.required_r = 3;
      plan.required_z = 4;
      if (configured_r != plan.required_r)
      {
        error = "OptimalLRC k=96 requires r=3";
        return false;
      }
      if (failure_type == "single")
        plan.single_cases = ECProject::RepairBenchmark::make_optimal_uneven_single_block_cases(
            k, configured_r, plan.required_z);
      else
        plan.double_cases = ECProject::RepairBenchmark::make_optimal_uneven_double_block_cases(k);
      return true;
    }

    error = "no repair plan is registered for CodeType=OptimalLRC, k=" +
            std::to_string(k) + ", failure=" + failure_type;
    return false;
  }

  if (code_type == "UniformLRC")
  {
    if ((k == 64 || k == 96 || k == 128) && (failure_type == "single" || failure_type == "double"))
    {
      plan.required_r = 3;
      plan.required_z = (k == 64) ? 3 : (k == 96 ? 4 : 5);
      if (configured_r != plan.required_r)
      {
        error = "UniformLRC k=" + std::to_string(k) + " requires r=3";
        return false;
      }
      if (failure_type == "single") {
        if (k == 64)
          plan.single_cases = ECProject::RepairBenchmark::make_uniform_k64_single_block_cases();
        else if (k == 96)
          plan.single_cases = ECProject::RepairBenchmark::make_uniform_k96_single_block_cases();
        else if (k == 128)
          plan.single_cases = ECProject::RepairBenchmark::make_uniform_k128_single_block_cases();
        else
          plan.single_cases = ECProject::RepairBenchmark::make_uniform_uneven_single_block_cases(
              k, configured_r, plan.required_z);
      } else {
        if (k == 64)
          plan.double_cases = ECProject::RepairBenchmark::make_uniform_k64_double_block_cases();
        else if (k == 96)
          plan.double_cases = ECProject::RepairBenchmark::make_uniform_k96_double_block_cases();
        else if (k == 128)
          plan.double_cases = ECProject::RepairBenchmark::make_uniform_k128_double_block_cases();
        else
          plan.double_cases = ECProject::RepairBenchmark::make_uniform_uneven_double_block_cases(k);
      }
      return true;
    }

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


    error = "no repair plan is registered for CodeType=UniformLRC, k=" +
            std::to_string(k) + ", failure=" + failure_type;
    return false;
  }

  if (failure_type == "muti")
  {
    if (code_type != "NLRC" || k != 128 || configured_r != 3)
    {
      error = "muti requires CodeType=NLRC, k=128, r=3, z=9";
      return false;
    }
    plan.required_r = 3;
    plan.required_z = 9;
    plan.muti_cases = ECProject::RepairBenchmark::make_nlrc_k128_muti_block_cases();
    return true;
  }

  if (code_type == "NLRC")
  {
    const bool k64_supported = k == 64 && (failure_type == "single" ||
        failure_type == "double" || failure_type == "triple" ||
        failure_type == "quadruple");
    const bool k96_supported = k == 96 &&
        (failure_type == "single" || failure_type == "double" ||
         failure_type == "triple" || failure_type == "quadruple");
    const bool k128_supported = k == 128 &&
        (failure_type == "single" || failure_type == "double" ||
         failure_type == "triple" || failure_type == "quadruple");
    if (!k64_supported && !k96_supported && !k128_supported)
    {
      error = "no repair plan is registered for CodeType=NLRC, k=" +
              std::to_string(k) + ", failure=" + failure_type;
      return false;
    }
    plan.required_r = 3;
    plan.required_z = k == 128 ? 9 : (k == 96 ? 7 : 5);
    if (configured_r != plan.required_r)
    {
      error = "NLRC k=" + std::to_string(k) + " requires r=3";
      return false;
    }
    if (failure_type == "single")
      plan.single_cases = k == 128
          ? ECProject::RepairBenchmark::make_nlrc_k128_single_block_cases()
          : (k == 96
                 ? ECProject::RepairBenchmark::make_nlrc_k96_single_block_cases()
                 : ECProject::RepairBenchmark::make_nlrc_k64_single_block_cases());
    else if (failure_type == "double")
      plan.double_cases = k == 128
          ? ECProject::RepairBenchmark::make_nlrc_k128_double_block_cases()
          : (k == 96
                 ? ECProject::RepairBenchmark::make_nlrc_k96_double_block_cases()
                 : ECProject::RepairBenchmark::make_nlrc_k64_double_block_cases());
    else if (failure_type == "triple")
      plan.triple_cases = k == 128
          ? ECProject::RepairBenchmark::make_nlrc_k128_triple_block_cases()
          : (k == 96
                 ? ECProject::RepairBenchmark::make_nlrc_k96_triple_block_cases()
                 : ECProject::RepairBenchmark::make_nlrc_k64_triple_block_cases());
    else
      plan.quadruple_cases = k == 128
          ? ECProject::RepairBenchmark::make_nlrc_k128_quadruple_block_cases()
          : (k == 96
                 ? ECProject::RepairBenchmark::make_nlrc_k96_quadruple_block_cases()
                 : ECProject::RepairBenchmark::make_nlrc_k64_quadruple_block_cases());
    return true;
  }

  if (code_type == "HLRC")
  {
    const bool k64_supported = k == 64 && (failure_type == "single" ||
        failure_type == "double" || failure_type == "triple" ||
        failure_type == "quadruple");
    const bool k96_supported = k == 96 &&
        (failure_type == "single" || failure_type == "double" ||
         failure_type == "triple" || failure_type == "quadruple");
    const bool k128_supported = k == 128 &&
        (failure_type == "single" || failure_type == "double" ||
         failure_type == "triple" || failure_type == "quadruple");
    if (!k64_supported && !k96_supported && !k128_supported)
    {
      error = "no repair plan is registered for CodeType=HLRC, k=" +
              std::to_string(k) + ", failure=" + failure_type;
      return false;
    }
    plan.required_r = 2;
    plan.required_z = k == 128 ? 10 : (k == 96 ? 8 : 6);
    if (configured_r != plan.required_r)
    {
      error = "HLRC k=" + std::to_string(k) + " requires r=2";
      return false;
    }
    if (failure_type == "single")
      plan.single_cases = k == 128
          ? ECProject::RepairBenchmark::make_hlrc_k128_single_block_cases()
          : (k == 96
              ? ECProject::RepairBenchmark::make_hlrc_k96_single_block_cases()
              : ECProject::RepairBenchmark::make_hlrc_k64_single_block_cases());
    else if (failure_type == "double")
      plan.double_cases = k == 128
          ? ECProject::RepairBenchmark::make_hlrc_k128_double_block_cases()
          : (k == 96
              ? ECProject::RepairBenchmark::make_hlrc_k96_double_block_cases()
              : ECProject::RepairBenchmark::make_hlrc_k64_double_block_cases());
    else if (failure_type == "triple")
      plan.triple_cases = k == 128
          ? ECProject::RepairBenchmark::make_hlrc_k128_triple_block_cases()
          : (k == 96
              ? ECProject::RepairBenchmark::make_hlrc_k96_triple_block_cases()
              : ECProject::RepairBenchmark::make_hlrc_k64_triple_block_cases());
    else
      plan.quadruple_cases = k == 128
          ? ECProject::RepairBenchmark::make_hlrc_k128_quadruple_block_cases()
          : (k == 96
              ? ECProject::RepairBenchmark::make_hlrc_k96_quadruple_block_cases()
              : ECProject::RepairBenchmark::make_hlrc_k64_quadruple_block_cases());
    return true;
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
  return k == 24 || k == 48 || k == 64 || k == 72 || k == 96 || k == 128;
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

std::string repair_task_name(const RepairTask &task, int k, int r, int z,
                             const std::string &code_type)
{
  std::string name;
  for (int id : task.failed_block_ids)
  {
    if (!name.empty()) name += "+";
    if (id < k) name += "D" + std::to_string(id);
    else if (id < k + r) name += "G" + std::to_string(id - k);
    else
    {
      const int local_id = id - k - r;
      if (code_type == "HLRC" && local_id >= z - 2)
        name += "U" + std::to_string(local_id - (z - 2));
      else
        name += "L" + std::to_string(local_id);
    }
  }
  return name;
}

void print_repair_plan(const RepairPlan &plan, const std::string &failure_type)
{
  std::cout << "Repair plan: code=" << plan.code_type << ", k=" << plan.k
            << ", r=" << plan.required_r << ", z=" << plan.required_z
            << ", failure=" << failure_type << std::endl;
  auto print_task = [](const RepairTask &task) {
    std::cout << "  task owner=" << task.failed_block_ids.front() << " failed=[";
    print_ids(task.failed_block_ids);
    std::cout << "], sources=[";
    print_ids(task.source_block_ids);
    std::cout << "]" << std::endl;
  };
  for (const auto &test_case : plan.single_cases)
  {
    std::cout << "Case " << test_case.failed_name << std::endl;
    print_task({{test_case.failed_block_id}, test_case.source_block_ids});
  }
  for (const auto *cases : {&plan.double_cases, &plan.triple_cases, &plan.quadruple_cases, &plan.muti_cases})
    for (const auto &test_case : *cases)
    {
      std::cout << "Case " << test_case.failed_name << std::endl;
      for (const auto &task : test_case.tasks)
        print_task(task);
    }
}

struct BenchmarkOptions
{
  int repeat = 1;
  int warmup = 0;
  int stripe_id = -1;
  bool check_plan = false;
  std::string config_path;
};

BenchmarkOptions parse_options(int argc, char **argv)
{
  BenchmarkOptions options;
  std::vector<std::string> seen;
  for (int i = 3; i < argc; ++i)
  {
    const std::string flag = argv[i];
    if (std::find(seen.begin(), seen.end(), flag) != seen.end())
      throw std::invalid_argument("duplicate option " + flag);
    seen.push_back(flag);
    if (flag == "--check-plan")
    {
      options.check_plan = true;
      if (i + 1 < argc && std::string(argv[i + 1]).rfind("--", 0) != 0)
      {
        if (!options.config_path.empty()) throw std::invalid_argument("duplicate config path");
        options.config_path = argv[++i];
      }
      continue;
    }
    if (flag != "--repeat" && flag != "--warmup" && flag != "--stripe-id" && flag != "--config")
      throw std::invalid_argument("unknown option " + flag);
    if (++i == argc) throw std::invalid_argument("missing value for " + flag);
    const std::string value = argv[i];
    if (flag == "--config")
    {
      if (!options.config_path.empty()) throw std::invalid_argument("duplicate config path");
      if (value.empty() || value.rfind("--", 0) == 0) throw std::invalid_argument("missing config path");
      options.config_path = value;
      continue;
    }
    if (value.empty() || value.find_first_not_of("0123456789") != std::string::npos)
      throw std::invalid_argument("invalid value for " + flag);
    size_t parsed = 0;
    const long long number = std::stoll(value, &parsed);
    const long long maximum = flag == "--stripe-id" ? std::numeric_limits<int>::max() : 1000000;
    const int minimum = flag == "--repeat" ? 1 : 0;
    if (parsed != value.size() || number < minimum || number > maximum)
      throw std::invalid_argument("value out of range for " + flag);
    if (flag == "--repeat") options.repeat = static_cast<int>(number);
    else if (flag == "--warmup") options.warmup = static_cast<int>(number);
    else options.stripe_id = static_cast<int>(number);
  }
  return options;
}

} // namespace

int main(int argc, char **argv)
{
  if (argc < 3)
  {
    std::cerr << "Usage: " << argv[0]
              << " <24|48|64|72|96|128> <single|double|triple|quadruple|muti>"
              << " [--repeat N] [--warmup N] [--stripe-id ID]"
              << " [--config config.xml] [--check-plan [config.xml]]" << std::endl;
    return 2;
  }

  BenchmarkOptions options;
  try { options = parse_options(argc, argv); }
  catch (const std::exception &error)
  {
    std::cerr << "Error: " << error.what() << std::endl;
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
    std::cerr << "Error: encoding parameter must be one of 24, 48, 64, 72, 96, or 128." << std::endl;
    return 2;
  }

  const std::string failure_type = argv[2];
  if (!is_supported_k(requested_k))
  {
    std::cerr << "Error: unsupported encoding parameter " << requested_k
              << "; expected 24, 48, 64, 72, 96, or 128." << std::endl;
    return 2;
  }
  if (!ECProject::RepairBenchmark::is_supported_failure_type(failure_type))
  {
    std::cerr << "Error: unsupported failure type '" << failure_type
              << "'; expected single, double, triple, quadruple, or muti." << std::endl;
    return 2;
  }

  const std::string config_path = options.config_path.empty()
      ? config_path_from_executable(argv[0]) : options.config_path;
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

  if (options.check_plan)
  {
    print_repair_plan(repair_plan, failure_type);
    std::cout << "Benchmark options: repeat=" << options.repeat << ", warmup=" << options.warmup
              << ", stripe=" << (options.stripe_id < 0 ? "new" : std::to_string(options.stripe_id))
              << std::endl;
    std::cout << "Repair plan check passed." << std::endl;
    return 0;
  }

  const std::string client_ip = config->ClientIP;
  const int client_port = 44444;
  ECProject::Client client(client_ip, client_port,
      config->CoordinatorIP + ":" + std::to_string(config->CoordinatorPort), config_path,
      options.stripe_id < 0);

  std::cout << client.sayHelloToCoordinatorByGrpc(
      "Repair benchmark (" + config->CodeType + "): " + client_ip + ":" +
      std::to_string(client_port)) << std::endl;
  int stripe_id = options.stripe_id;
  const std::vector<int> previous_stripes = client.list_stripes();
  if (stripe_id >= 0)
  {
    if (std::find(previous_stripes.begin(), previous_stripes.end(), stripe_id) == previous_stripes.end())
    {
      std::cerr << "Error: requested stripe " << stripe_id << " does not exist; no stripe was created."
                << std::endl;
      return 1;
    }
    std::cout << "Reusing existing stripe " << stripe_id << "; skipping set." << std::endl;
  }
  else
  {
    if (!client.set())
    {
      std::cerr << "Error: failed to create the benchmark stripe." << std::endl;
      return 1;
    }
    std::vector<int> created;
    for (int id : client.list_stripes())
      if (std::find(previous_stripes.begin(), previous_stripes.end(), id) == previous_stripes.end())
        created.push_back(id);
    if (created.size() != 1)
    {
      std::cerr << "Error: cannot uniquely identify the newly created stripe; stop concurrent stripe creation."
                << std::endl;
      return 1;
    }
    stripe_id = created.front();
  }

  std::cout << "Repair benchmark: code=" << repair_plan.code_type
            << ", k=" << config->k << ", r=" << config->r
            << ", z=" << config->z << ", failure=" << failure_type
            << ", stripe=" << stripe_id << ", repeat=" << options.repeat
            << ", warmup=" << options.warmup << std::endl;
  std::vector<ECProject::RepairBenchmark::RepairGroupCase> cases;
  if (failure_type == "single")
    for (const auto &test : repair_plan.single_cases)
      cases.push_back({test.failed_name, {{{test.failed_block_id}, test.source_block_ids}}, false});
  else if (failure_type == "double") cases = repair_plan.double_cases;
  else if (failure_type == "triple") cases = repair_plan.triple_cases;
  else if (failure_type == "quadruple") cases = repair_plan.quadruple_cases;
  else cases = repair_plan.muti_cases;

  bool all_succeeded = true;
  std::map<std::string, std::vector<double>> samples;
  std::vector<std::string> summary_names;
  for (const auto &test_case : cases)
  {
    if (test_case.sum_task_times)
      for (const auto &task : test_case.tasks)
        summary_names.push_back(test_case.failed_name + "/" +
                                repair_task_name(task, config->k, config->r, config->z, repair_plan.code_type));
    summary_names.push_back(test_case.failed_name);
  }
  for (int iteration = 0; iteration < options.warmup + options.repeat; ++iteration)
  {
    const bool warming_up = iteration < options.warmup;
    const int round = warming_up ? iteration + 1 : iteration - options.warmup + 1;
    std::cout << (warming_up ? "Warmup round " : "Measured round ") << round
              << "/" << (warming_up ? options.warmup : options.repeat)
              << ", stripe=" << stripe_id << std::endl;
    for (const auto &test_case : cases)
    {
      std::cout << "Case " << test_case.failed_name << std::endl;
      std::vector<std::vector<int>> failed_ids, source_ids;
      for (const auto &task : test_case.tasks)
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
      std::vector<double> task_times;
      ECProject::MultiRecoveryBreakdown breakdown;
      bool succeeded = failure_type == "single"
          ? client.recovery_with_sources(stripe_id, failed_ids[0][0], source_ids[0], recovery_time)
          : client.multi_recovery_with_sources(stripe_id, failed_ids, source_ids, recovery_time,
                                               test_case.optimal_g0_d0_chain,
                                               test_case.sum_task_times ? &task_times : nullptr,
                                               failure_type == "muti" ? &breakdown : nullptr);
      const double wall_time = recovery_time;
      if (succeeded && failure_type == "muti")
      {
        if (breakdown.per_output_write_back_times.size() != failed_ids[0].size())
        {
          std::cerr << "Missing per-output write-back breakdown; rebuild and restart proxies and coordinator."
                    << std::endl;
          succeeded = false;
        }
        else
        {
          std::cout << "  source read: " << breakdown.source_read_time << " s" << std::endl;
          std::cout << "  coefficient solve: " << breakdown.coefficient_solve_time << " s" << std::endl;
          std::cout << "  AVX2 encode: " << breakdown.avx2_encode_time << " s" << std::endl;
          for (size_t i = 0; i < breakdown.per_output_write_back_times.size(); ++i)
            std::cout << "  per-output write-back G" << (failed_ids[0][i] - config->k)
                      << ": " << breakdown.per_output_write_back_times[i] << " s" << std::endl;
        }
      }
      if (succeeded && test_case.sum_task_times)
      {
        if (task_times.size() != test_case.tasks.size() ||
            std::any_of(task_times.begin(), task_times.end(),
                        [](double value) { return !std::isfinite(value) || value < 0.0; }))
        {
          std::cerr << "Missing or invalid per-task recovery times; rebuild and restart the coordinator."
                    << std::endl;
          succeeded = false;
        }
        else
        {
          recovery_time = 0.0;
          for (size_t i = 0; i < task_times.size(); ++i)
          {
            const std::string name = test_case.failed_name + "/" +
                repair_task_name(test_case.tasks[i], config->k, config->r, config->z,
                                 repair_plan.code_type);
            std::cout << (warming_up ? "Warmup task" : "Task result")
                      << " [" << repair_plan.code_type << "] " << name << ": "
                      << task_times[i] << " s" << std::endl;
            recovery_time += task_times[i];
            if (!warming_up) samples[name].push_back(task_times[i]);
          }
        }
      }
      std::cout << (warming_up ? "Warmup" : "Result") << " [" << repair_plan.code_type
                << "] " << test_case.failed_name << ": ";
      if (succeeded)
      {
        std::cout << recovery_time << " s";
        if (test_case.sum_task_times)
          std::cout << " (sum; wall=" << wall_time << " s)";
        std::cout << std::endl;
        if (!warming_up) samples[test_case.failed_name].push_back(recovery_time);
      }
      else
      {
        std::cout << "FAILED after " << recovery_time << " s" << std::endl;
        all_succeeded = false;
        if (warming_up)
        {
          std::cerr << "Warmup failed; measured rounds were not started." << std::endl;
          return 1;
        }
      }
    }
  }
  if (options.repeat > 1 || options.warmup > 0)
    for (const auto &name : summary_names)
    {
      auto values = samples[name];
      std::sort(values.begin(), values.end());
      std::cout << "Summary [" << repair_plan.code_type << "] " << name
                << ": success=" << values.size() << "/" << options.repeat;
      if (!values.empty())
      {
        const size_t count = values.size();
        const double median = (values[(count - 1) / 2] + values[count / 2]) / 2.0;
        const size_t p95_index = static_cast<size_t>(std::ceil(count * 0.95)) - 1;
        std::cout << " min=" << values.front() << " median=" << median
                  << " p95=" << values[p95_index] << " max=" << values.back() << " s";
      }
      std::cout << std::endl;
    }

  std::cout << "Benchmark completed: code=" << repair_plan.code_type
            << ", result=" << (all_succeeded ? "SUCCESS" : "FAILED") << std::endl;
  return all_succeeded ? 0 : 1;
}
