#ifndef CONFIG_H
#define CONFIG_H

#include "devcommon.h"

namespace ECProject
{
  const int DATANODE_PORT_SHIFT = 500;
  const int PROXY_PORT_SHIFT = 1000;
  const int PROXY_PEER_PORT_SHIFT = 2000;

  class Config
  {
  private:
    static Config *instance;
    Config(const std::string &configPath);

  public:
    static Config *getInstance(const std::string &configPath);
    void loadConfig(const std::string &configPath);
    void printConfigs() const;
    void validateConfig() const;

    int AlignedSize;
    int UnitSize;
    unsigned int BlockSize;
    int alpha = 0;
    int z;
    int n;
    int k;
    int r;
    int NodeNum;
    std::string CoordinatorIP;
    std::string ClientIP;
    int CoordinatorPort;
    std::string AppendMode;
    std::string CodeType;
  };
}

#endif // CONFIG_H