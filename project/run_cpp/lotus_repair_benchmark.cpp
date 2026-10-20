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
};

void append_range(std::vector<int> &ids, int first, int last)
{
  for (int id = first; id <= last; ++id)
    ids.push_back(id);
}

std::vector<RepairCase> make_k24_single_block_cases(int r)
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

std::vector<RepairCase> make_k48_single_block_cases()
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

std::vector<RepairCase> make_k72_single_block_cases(int r)
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

std::vector<RepairCase> make_k96_single_block_cases(int r)
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

std::vector<DoubleRepairCase> make_k24_double_block_cases(int r)
{
  const int first_local_parity_id = 24 + r;
  std::vector<DoubleRepairCase> cases;

  RepairTask l2_l3{{first_local_parity_id + 2, first_local_parity_id + 3}, {}};
  append_range(l2_l3.source_block_ids, 12, 23);
  l2_l3.source_block_ids.push_back(24);
  cases.push_back({"L2+L3", {l2_l3}});

  return cases;
}

std::vector<DoubleRepairCase> make_k48_double_block_cases()
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

std::vector<DoubleRepairCase> make_k72_double_block_cases(int r)
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

std::vector<DoubleRepairCase> make_k96_double_block_cases(int r)
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
  return directory + "/../config/parameterConfiguration.xml";
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

  const bool supported_plan =
      (requested_k == 24 && (failure_type == "single" || failure_type == "double")) ||
      (requested_k == 48 && (failure_type == "single" || failure_type == "double")) ||
      (requested_k == 72 && (failure_type == "single" || failure_type == "double")) ||
      (requested_k == 96 && (failure_type == "single" || failure_type == "double"));
  if (!supported_plan)
  {
    std::cerr << "Error: k=" << requested_k << " with failure type '" << failure_type
              << "' is not implemented yet. Currently available: 24 single, 24 double, "
              << "48 single, 48 double, 72 single, 72 double, 96 single, 96 double."
              << std::endl;
    return 3;
  }
  const int expected_r = requested_k / 24;
  if (config->CodeType != "LotusLRC" || config->r != expected_r || config->z != 8)
  {
    std::cerr << "Error: the selected repair plan requires LotusLRC with k=" << requested_k
              << ", r=" << expected_r << ", z=8; XML has " << config->CodeType
              << " with k=" << config->k << ", r=" << config->r
              << ", z=" << config->z << "." << std::endl;
    return 1;
  }

  const std::string client_ip = "127.0.0.1";
  const int client_port = 44444;
  ECProject::Client client(client_ip, client_port,
      config->CoordinatorIP + ":" + std::to_string(config->CoordinatorPort), config_path);

  std::cout << client.sayHelloToCoordinatorByGrpc(
      "Lotus repair benchmark: " + client_ip + ":" + std::to_string(client_port)) << std::endl;
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

  std::cout << "LotusLRC repair benchmark: k=" << config->k << ", r=" << config->r
            << ", z=" << config->z << ", failure=" << failure_type
            << ", stripe=" << stripe_id << std::endl;
  bool all_succeeded = true;
  if (failure_type == "single")
  {
    std::vector<RepairCase> single_cases;
    if (requested_k == 24)
      single_cases = make_k24_single_block_cases(config->r);
    else if (requested_k == 48)
      single_cases = make_k48_single_block_cases();
    else if (requested_k == 72)
      single_cases = make_k72_single_block_cases(config->r);
    else
      single_cases = make_k96_single_block_cases(config->r);
    for (const RepairCase &test_case : single_cases)
    {
      std::cout << "Case " << test_case.failed_name << " (block " << test_case.failed_block_id
                << "), sources=[";
      print_ids(test_case.source_block_ids);
      std::cout << "]" << std::endl;

      double recovery_time = 0.0;
      const bool succeeded = client.recovery_with_sources(
          stripe_id, test_case.failed_block_id, test_case.source_block_ids, recovery_time);
      if (succeeded)
        std::cout << "Result " << test_case.failed_name << ": " << recovery_time << " s" << std::endl;
      else
      {
        std::cout << "Result " << test_case.failed_name << ": FAILED after "
                  << recovery_time << " s" << std::endl;
        all_succeeded = false;
      }
    }
  }
  else
  {
    std::vector<DoubleRepairCase> double_cases;
    if (requested_k == 24)
      double_cases = make_k24_double_block_cases(config->r);
    else if (requested_k == 48)
      double_cases = make_k48_double_block_cases();
    else if (requested_k == 72)
      double_cases = make_k72_double_block_cases(config->r);
    else
      double_cases = make_k96_double_block_cases(config->r);
    for (const DoubleRepairCase &test_case : double_cases)
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
          stripe_id, failed_ids, source_ids, recovery_time);
      if (succeeded)
        std::cout << "Result " << test_case.failed_name << ": " << recovery_time << " s" << std::endl;
      else
      {
        std::cout << "Result " << test_case.failed_name << ": FAILED after "
                  << recovery_time << " s" << std::endl;
        all_succeeded = false;
      }
    }
  }

  return all_succeeded ? 0 : 1;
}
