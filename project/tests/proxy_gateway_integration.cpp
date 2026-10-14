#include "proxy.h"
#include "datanode.h"
#include <asio.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <thread>
#include <unistd.h>
#include <vector>

namespace
{
struct TestPorts
{
  int proxy_base;
  int datanode_base;
  int coordinator;
};

bool reserve_ports(const std::vector<int> &ports)
{
  asio::io_context context;
  std::vector<std::unique_ptr<asio::ip::tcp::acceptor>> acceptors;
  try
  {
    for (int port : ports)
    {
      auto acceptor = std::make_unique<asio::ip::tcp::acceptor>(context);
      acceptor->open(asio::ip::tcp::v4());
      acceptor->set_option(asio::socket_base::reuse_address(false));
      acceptor->bind({asio::ip::make_address("127.0.0.1"), static_cast<unsigned short>(port)});
      acceptors.push_back(std::move(acceptor));
    }
    return true;
  }
  catch (const std::exception &)
  {
    return false;
  }
}

TestPorts select_ports()
{
  const int seed = static_cast<int>(::getpid() % 1000);
  for (int attempt = 0; attempt < 1000; ++attempt)
  {
    const int proxy_base = 28000 + ((seed + attempt) % 1000) * 3;
    const int datanode_base = 40000 + ((seed + attempt) % 1000) * 3;
    const int coordinator = 55000 + ((seed + attempt) % 1000);
    std::vector<int> ports{coordinator};
    for (int i = 0; i < 3; ++i)
    {
      ports.push_back(proxy_base + i);
      ports.push_back(proxy_base + 1000 + i);
      ports.push_back(proxy_base + 2000 + i);
      ports.push_back(datanode_base + i);
      ports.push_back(datanode_base + 500 + i);
    }
    if (reserve_ports(ports)) return {proxy_base, datanode_base, coordinator};
  }
  throw std::runtime_error("unable to find free integration-test ports");
}

void write_fixtures(const std::filesystem::path &directory, const TestPorts &ports,
                    std::filesystem::path &node_xml, std::filesystem::path &parameter_xml)
{
  node_xml = directory / "nodeInformation.xml";
  parameter_xml = directory / "parameterConfiguration.xml";
  std::ofstream nodes(node_xml);
  nodes << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<nodes>\n";
  for (int i = 0; i < 3; ++i)
    nodes << "  <node id=\"" << i << "\" proxy=\"127.0.0.1:" << ports.proxy_base + i
          << "\" datanode=\"127.0.0.1:" << ports.datanode_base + i << "\"/>\n";
  nodes << "</nodes>\n";
  nodes.close();

  std::ofstream parameters(parameter_xml);
  parameters << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<Configuration>\n"
             << "  <AlignedSize>4096</AlignedSize><UnitSize>8</UnitSize><BlockSize>25</BlockSize>\n"
             << "  <CoordinatorIP>127.0.0.1</CoordinatorIP><CoordinatorPort>" << ports.coordinator << "</CoordinatorPort>\n"
             << "  <AppendMode>UNILRC_MODE</AppendMode><CodeType>AzureLRC</CodeType>\n"
             << "  <k>1</k><r>1</r><z>1</z><NodeNum>3</NodeNum>\n</Configuration>\n";
}
} // namespace

int main()
{
  try
  {
    const TestPorts ports = select_ports();
    const auto fixture_dir = std::filesystem::temp_directory_path() /
        ("hlrc-proxy-gateway-" + std::to_string(::getpid()));
    std::filesystem::create_directories(fixture_dir);
    std::filesystem::path node_xml;
    std::filesystem::path parameter_xml;
    write_fixtures(fixture_dir, ports, node_xml, parameter_xml);

    const std::string payload = "owner-proxy-route-payload";
    const std::string key = "gateway_test_block_" + std::to_string(::getpid());
    const std::string datanode_endpoint = "127.0.0.1:" + std::to_string(ports.datanode_base);
    const std::string owner_endpoint = "127.0.0.1:" + std::to_string(ports.proxy_base);
    const auto storage_dir = std::filesystem::path("storage") / std::to_string(ports.datanode_base);
    std::filesystem::remove_all(storage_dir);
    std::filesystem::create_directories(storage_dir);

    std::thread datanode_thread([&] { ECProject::DataNode(datanode_endpoint, parameter_xml.string()).Run(); });
    datanode_thread.detach();
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    ECProject::ProxyImpl owner(owner_endpoint, node_xml.string(), "127.0.0.1:" + std::to_string(ports.coordinator));
    ECProject::ProxyImpl caller("127.0.0.1:" + std::to_string(ports.proxy_base + 1), node_xml.string(),
                                "127.0.0.1:" + std::to_string(ports.coordinator));
    ECProject::ProxyImpl third("127.0.0.1:" + std::to_string(ports.proxy_base + 2), node_xml.string(),
                               "127.0.0.1:" + std::to_string(ports.coordinator));
    auto *config = ECProject::Config::getInstance(parameter_xml.string());
    owner.m_sys_config = caller.m_sys_config = third.m_sys_config = config;
    owner.m_toolbox = caller.m_toolbox = third.m_toolbox = ECProject::ToolBox::getInstance();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    if (owner.LocalDatanodeStubCountForTest() != 1 || caller.LocalDatanodeStubCountForTest() != 1 ||
        third.LocalDatanodeStubCountForTest() != 1)
      throw std::runtime_error("a proxy created more than its paired datanode stub");
    if (!caller.PeerPutForTest("127.0.0.1", ports.proxy_base, key, payload))
      throw std::runtime_error("non-owner peer put failed");
    const auto stored_block = storage_dir / key;
    const auto write_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while ((!std::filesystem::exists(stored_block) || std::filesystem::file_size(stored_block) != payload.size()) &&
           std::chrono::steady_clock::now() < write_deadline)
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    if (!std::filesystem::exists(stored_block) || std::filesystem::file_size(stored_block) != payload.size())
      throw std::runtime_error("owner datanode did not finish the asynchronous write");
    std::string received(payload.size(), '\0');
    if (!third.PeerGetForTest("127.0.0.1", ports.proxy_base, key, received) || received != payload)
      throw std::runtime_error("non-owner peer get failed");
    if (!caller.PeerDeleteForTest("127.0.0.1", ports.proxy_base, key))
      throw std::runtime_error("non-owner peer delete failed");
    if (std::filesystem::exists(storage_dir / key))
      throw std::runtime_error("owner datanode file remains after delete");

    std::filesystem::remove_all(storage_dir);
    std::filesystem::remove_all(fixture_dir);
    std::cout << "PASS: self-contained three-proxy owner gateway put/get/delete" << std::endl;
    std::_Exit(0);
  }
  catch (const std::exception &e)
  {
    std::cerr << "FAIL: " << e.what() << std::endl;
    std::_Exit(1);
  }
}
