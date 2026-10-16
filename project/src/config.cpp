#include "config.h"
#include "tinyxml2.h"

#include <limits>
#include <set>
#include <stdexcept>

namespace ECProject
{
  namespace
  {
    const tinyxml2::XMLElement *requiredElement(const tinyxml2::XMLElement *root,
                                                const char *name,
                                                const std::string &path)
    {
      const auto *element = root->FirstChildElement(name);
      if (element == nullptr || element->GetText() == nullptr || std::string(element->GetText()).empty())
        throw std::runtime_error("Missing required configuration field '" + std::string(name) + "' in " + path);
      return element;
    }

    int requiredPositiveInt(const tinyxml2::XMLElement *root, const char *name,
                            const std::string &path)
    {
      const auto *element = requiredElement(root, name, path);
      int value = 0;
      if (element->QueryIntText(&value) != tinyxml2::XML_SUCCESS || value <= 0)
        throw std::runtime_error("Invalid configuration field '" + std::string(name) +
                                 "': actual=" + element->GetText() + ", required=positive integer");
      return value;
    }
  }

  Config *Config::instance = nullptr;

  Config::Config(const std::string &configPath)
  {
    loadConfig(configPath);
    validateConfig();
    printConfigs();
  }

  void Config::validateConfig() const
  {
    static const std::set<std::string> supported = {
        "AzureLRC", "CP-ALRC", "CP-ULRC", "OptimalLRC", "UniformLRC", "LotusLRC", "UniLRC"};
    if (supported.count(CodeType) == 0)
      throw std::runtime_error("Unsupported CodeType: actual=" + CodeType);
    if (AlignedSize <= 0 || UnitSize <= 0 || BlockSize == 0 || CoordinatorPort <= 0 ||
        CoordinatorPort > 65535 || CoordinatorIP.empty() || AppendMode.empty())
      throw std::runtime_error("Invalid non-positive or empty runtime configuration value");
    if (k <= 0 || r <= 0 || z <= 0)
      throw std::runtime_error("Invalid coding parameters for " + CodeType +
                               ": k=" + std::to_string(k) + ", r=" + std::to_string(r) +
                               ", z=" + std::to_string(z) + ", required=positive values");
    if (CodeType == "UniLRC" && alpha <= 0)
      throw std::runtime_error("Invalid alpha for UniLRC: actual=" + std::to_string(alpha) +
                               ", required=positive integer");
    if (CodeType == "CP-ULRC" && z > k + r - 1)
      throw std::runtime_error("Invalid coding parameters for CP-ULRC: k=" + std::to_string(k) +
                               ", z=" + std::to_string(z) + ", required=z <= k + r - 1");
    if (CodeType == "CP-ALRC" && k % z != 0)
      throw std::runtime_error("Invalid coding parameters for CP-ALRC: k=" + std::to_string(k) +
                               ", z=" + std::to_string(z) + ", required=k divisible by z");
    if (NodeNum < n)
      throw std::runtime_error("Insufficient NodeNum for code type " + CodeType +
                               ": actual=" + std::to_string(NodeNum) +
                               ", required=" + std::to_string(n));
  }

  Config *Config::getInstance(const std::string &configPath)
  {
    if (instance == nullptr)
      instance = new Config(configPath);
    return instance;
  }

  void Config::loadConfig(const std::string &configPath)
  {
    tinyxml2::XMLDocument doc;
    const auto status = doc.LoadFile(configPath.c_str());
    if (status != tinyxml2::XML_SUCCESS)
      throw std::runtime_error("Failed to load configuration file '" + configPath +
                               "': " + doc.ErrorStr());

    const tinyxml2::XMLElement *root = doc.RootElement();
    if (root == nullptr || std::string(root->Name()) != "Configuration")
      throw std::runtime_error("Invalid configuration root in " + configPath +
                               ": required=Configuration");

    AlignedSize = requiredPositiveInt(root, "AlignedSize", configPath);
    UnitSize = requiredPositiveInt(root, "UnitSize", configPath);
    const int blockSize = requiredPositiveInt(root, "BlockSize", configPath);
    BlockSize = static_cast<unsigned int>(blockSize);
    z = requiredPositiveInt(root, "z", configPath);
    CodeType = requiredElement(root, "CodeType", configPath)->GetText();

    if (CodeType == "UniLRC")
    {
      alpha = requiredPositiveInt(root, "alpha", configPath);
      const long long derivedK = 1LL * alpha * z * z - 1LL * alpha * z;
      const long long derivedR = 1LL * alpha * z;
      if (derivedK <= 0 || derivedK > std::numeric_limits<int>::max() ||
          derivedR > std::numeric_limits<int>::max())
        throw std::runtime_error("Invalid derived UniLRC parameters: k=" +
                                 std::to_string(derivedK) + ", r=" + std::to_string(derivedR));
      k = static_cast<int>(derivedK);
      r = static_cast<int>(derivedR);
    }
    else
    {
      k = requiredPositiveInt(root, "k", configPath);
      r = requiredPositiveInt(root, "r", configPath);
    }
    const long long width = 1LL * k + r + z;
    if (width > std::numeric_limits<int>::max())
      throw std::runtime_error("Stripe width exceeds integer range");
    n = static_cast<int>(width);

    NodeNum = requiredPositiveInt(root, "NodeNum", configPath);
    CoordinatorIP = requiredElement(root, "CoordinatorIP", configPath)->GetText();
    CoordinatorPort = requiredPositiveInt(root, "CoordinatorPort", configPath);
    AppendMode = requiredElement(root, "AppendMode", configPath)->GetText();
  }

  void Config::printConfigs() const
  {
    std::cout << "Configuration Parameters:" << std::endl;
    std::cout << "  AlignedSize: " << AlignedSize << " bytes" << std::endl;
    std::cout << "  UnitSize: " << UnitSize << " bytes" << std::endl;
    std::cout << "  BlockSize: " << BlockSize << " bytes" << std::endl;
    std::cout << "  alpha: " << alpha << std::endl;
    std::cout << "  z: " << z << std::endl;
    std::cout << "  n: " << n << std::endl;
    std::cout << "  k: " << k << std::endl;
    std::cout << "  r: " << r << std::endl;
    std::cout << "  (n, k, r, z): (" << n << ", " << k << ", " << r << ", " << z << ")" << std::endl;
    std::cout << "  NodeNum: " << NodeNum << " nodes" << std::endl;
    std::cout << "  CoordinatorIP: " << CoordinatorIP << std::endl;
    std::cout << "  CoordinatorPort: " << CoordinatorPort << std::endl;
    std::cout << "  AppendMode: " << AppendMode << std::endl;
    std::cout << "  CodeType: " << CodeType << std::endl;
  }
}
