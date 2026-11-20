#include "proxy.h"
#include "datanode.h"
#include "encoder.h"
#include "repair_benchmark_plan.h"

#include <asio.hpp>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <random>
#include <stdexcept>
#include <thread>
#include <unistd.h>
#include <vector>

namespace {
constexpr int block_size = 1048576;
constexpr int node_count = 6;

void require(bool condition, const std::string &message)
{
    if (!condition) throw std::runtime_error(message);
}

struct Ports { int proxy, datanode; };

Ports select_ports()
{
    for (int attempt = 0; attempt < 1000; ++attempt) {
        const int offset = ((::getpid() + attempt) % 1000) * 6;
        const Ports ports{22000 + offset, 40000 + offset};
        asio::io_context context;
        std::vector<std::unique_ptr<asio::ip::tcp::acceptor>> reservations;
        try {
            for (int index = 0; index < node_count; ++index)
                for (int port : {ports.proxy + index, ports.proxy + 1000 + index,
                                 ports.proxy + 2000 + index, ports.datanode + index,
                                 ports.datanode + 500 + index}) {
                    auto acceptor = std::make_unique<asio::ip::tcp::acceptor>(context);
                    acceptor->open(asio::ip::tcp::v4());
                    acceptor->bind({asio::ip::make_address("127.0.0.1"),
                                    static_cast<unsigned short>(port)});
                    reservations.push_back(std::move(acceptor));
                }
            return ports;
        } catch (const std::exception &) {}
    }
    throw std::runtime_error("no free loopback ports for repair test");
}

std::string endpoint(int port) { return "127.0.0.1:" + std::to_string(port); }
std::string key(size_t scenario, int id)
{
    return "repair_" + std::to_string(scenario) + "_block_" + std::to_string(id);
}
std::filesystem::path stored(const Ports &ports, int owner, size_t scenario, int id)
{
    return std::filesystem::path("storage") / std::to_string(ports.datanode + owner) /
        key(scenario, id);
}
int owner_for(int id) { return id < 64 ? id % node_count : id - 64; }

void write_fixtures(const Ports &ports, bool uniform)
{
    std::ofstream nodes("nodes.xml");
    nodes << "<nodes>\n";
    for (int index = 0; index < node_count; ++index)
        nodes << "<node id=\"" << index << "\" proxy=\"" << endpoint(ports.proxy + index)
              << "\" datanode=\"" << endpoint(ports.datanode + index) << "\"/>\n";
    nodes << "</nodes>\n";
    std::ofstream parameters("parameters.xml");
    parameters << "<Configuration>\n"
        << "<AlignedSize>4096</AlignedSize><UnitSize>8</UnitSize><BlockSize>" << block_size
        << "</BlockSize><CoordinatorIP>127.0.0.1</CoordinatorIP><CoordinatorPort>55000</CoordinatorPort>\n"
        << "<AppendMode>UNILRC_MODE</AppendMode><CodeType>"
        << (uniform ? "UniformLRC" : "OptimalLRC") << "</CodeType>\n"
        << "<k>64</k><r>3</r><z>3</z><NodeNum>72</NodeNum>\n</Configuration>\n";
}

void verify_output(const Ports &ports, const std::vector<std::vector<unsigned char>> &blocks,
                   size_t scenario, int id)
{
    const int owner = owner_for(id);
    const auto path = stored(ports, owner, scenario, id);
    require(std::filesystem::exists(path), "repaired block is missing: " + std::to_string(id));
    require(std::filesystem::file_size(path) == block_size,
            "repaired block has the wrong length: " + std::to_string(id));
    std::ifstream file(path, std::ios::binary);
    std::vector<unsigned char> actual(block_size);
    file.read(reinterpret_cast<char *>(actual.data()), block_size);
    require(file.gcount() == block_size && actual == blocks[id],
            "persisted block bytes differ from original: " + std::to_string(id));
    for (int other = 0; other < node_count; ++other)
        if (other != owner)
            require(!std::filesystem::exists(stored(ports, other, scenario, id)),
                    "repaired block was written to the wrong datanode");
}
} // namespace

int main(int argc, char **argv)
{
    const std::string mode = argc == 2 ? argv[1] : "quadruple";
    const bool uniform = mode == "uniform-double";
    const bool optimal_double = mode == "double";
    const bool double_repair = optimal_double || uniform;
    const auto fixture = std::filesystem::temp_directory_path() /
        (std::string("hlrc-") + (uniform ? "uniform-" : "optimal-") + "k64-" +
         (double_repair ? "double-" : "quadruple-") + std::to_string(::getpid()));
    try {
        require(std::filesystem::create_directory(fixture), "fixture directory already exists");
        std::filesystem::current_path(fixture);
        const Ports ports = select_ports();
        write_fixtures(ports, uniform);
        auto *config = ECProject::Config::getInstance("parameters.xml");

        std::vector<std::vector<unsigned char>> blocks(70,
            std::vector<unsigned char>(block_size));
        std::mt19937 generator(0x643456u);
        for (int id = 0; id < 64; ++id)
            for (auto &byte : blocks[id]) byte = static_cast<unsigned char>(generator());
        std::vector<unsigned char *> data(64), parity(6);
        for (int id = 0; id < 64; ++id) data[id] = blocks[id].data();
        for (int id = 0; id < 6; ++id) parity[id] = blocks[id + 64].data();
        if (uniform)
            ECProject::encode_uniform_lrc(64, 3, 3, data.data(), parity.data(), block_size);
        else
            ECProject::encode_optimal_lrc(64, 3, 3, data.data(), parity.data(), block_size);

        for (int owner = 0; owner < node_count; ++owner) {
            std::filesystem::create_directories(stored(ports, owner, 0, 0).parent_path());
            const auto address = endpoint(ports.datanode + owner);
            std::thread([address] { ECProject::DataNode(address, "parameters.xml").Run(); }).detach();
            const auto channel = grpc::CreateChannel(address, grpc::InsecureChannelCredentials());
            require(channel->WaitForConnected(std::chrono::system_clock::now() +
                                              std::chrono::seconds(5)),
                    "datanode did not start");
        }

        std::vector<ECProject::ProxyImpl *> proxies;
        std::vector<std::unique_ptr<grpc::Server>> servers;
        for (int owner = 0; owner < node_count; ++owner) {
            auto *proxy = new ECProject::ProxyImpl(endpoint(ports.proxy + owner), "nodes.xml",
                                                  endpoint(55000));
            proxy->m_sys_config = config;
            proxy->m_toolbox = ECProject::ToolBox::getInstance();
            require(proxy->LocalDatanodeStubCountForTest() == 1,
                    "each proxy must access only its paired datanode");
            proxies.push_back(proxy);
            grpc::ServerBuilder builder;
            builder.AddListeningPort(endpoint(ports.proxy + owner), grpc::InsecureServerCredentials());
            builder.RegisterService(proxy);
            auto server = builder.BuildAndStart();
            require(server != nullptr, "proxy RPC server did not start");
            servers.push_back(std::move(server));
        }

        const auto cases = uniform
            ? ECProject::RepairBenchmark::make_uniform_k64_double_block_cases()
            : (double_repair
                   ? ECProject::RepairBenchmark::make_optimal_k64_double_block_cases()
                   : ECProject::RepairBenchmark::make_optimal_quadruple_block_cases(64));
        std::vector<size_t> scenario_indexes;
        if (optimal_double)
            scenario_indexes = {0, 1, 2};
        else
            scenario_indexes = {cases.size() - 1};

        for (size_t scenario = 0; scenario < scenario_indexes.size(); ++scenario) {
            const auto &test_case = cases[scenario_indexes[scenario]];
            require(test_case.tasks.size() == 1, "expected one shared task");
            const auto &task = test_case.tasks[0];
            require(task.failed_block_ids.size() >= 2, "expected multi-output task");

            for (int id : task.failed_block_ids)
                for (int owner = 0; owner < node_count; ++owner)
                    require(!std::filesystem::exists(stored(ports, owner, scenario, id)),
                            "failed output must not be pre-seeded");
            for (int id : task.source_block_ids) {
                require(std::find(task.failed_block_ids.begin(), task.failed_block_ids.end(), id) ==
                            task.failed_block_ids.end(),
                        "source list contains a failed output");
                std::ofstream file(stored(ports, owner_for(id), scenario, id), std::ios::binary);
                file.write(reinterpret_cast<const char *>(blocks[id].data()), block_size);
                require(file.good(), "cannot seed source block");
            }

            proxy_proto::RecoveryRequest request;
            request.set_cross_rack_num(0);
            request.set_failed_block_id(task.failed_block_ids.front());
            request.set_failed_block_key(key(scenario, task.failed_block_ids.front()));
            for (int id : task.failed_block_ids) {
                request.add_failed_block_ids(id);
                request.add_custom_failed_block_keys(key(scenario, id));
                request.add_custom_target_proxy_ips("127.0.0.1");
                request.add_custom_target_proxy_ports(ports.proxy + owner_for(id));
            }
            for (int id : task.source_block_ids) {
                request.add_custom_source_block_ids(id);
                request.add_blockids(id);
                request.add_blockkeys(key(scenario, id));
                request.add_source_proxy_ips("127.0.0.1");
                request.add_source_proxy_ports(ports.proxy + owner_for(id));
            }

            const int leader_owner = owner_for(task.failed_block_ids.front());
            auto channel = grpc::CreateChannel(endpoint(ports.proxy + leader_owner),
                                               grpc::InsecureChannelCredentials());
            require(channel->WaitForConnected(std::chrono::system_clock::now() +
                                              std::chrono::seconds(5)),
                    "leader proxy did not become ready");
            auto stub = proxy_proto::proxyService::NewStub(channel);
            grpc::ClientContext context;
            context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(45));
            proxy_proto::RecoveryReply reply;
            const auto status = stub->recovery(&context, request, &reply);
            require(status.ok(), test_case.failed_name + " recovery failed: " +
                                 status.error_message());

            for (int id : task.failed_block_ids) verify_output(ports, blocks, scenario, id);
            require(owner_for(task.failed_block_ids.front()) == leader_owner,
                    "first output was not assigned to the leader proxy");
            require(owner_for(task.failed_block_ids[1]) != leader_owner,
                    "second output must exercise forwarding to its owner proxy");
            std::cout << "PASS: " << (uniform ? "UniformLRC" : "OptimalLRC") << " k64 "
                      << test_case.failed_name << " reads " << task.source_block_ids.size()
                      << " sources once, writes the first output locally, and forwards the second"
                         " output to its owner proxy" << std::endl;
        }

        for (auto &server : servers) server->Shutdown();
        std::filesystem::current_path(fixture.parent_path());
        std::filesystem::remove_all(fixture);
        std::_Exit(0);
    } catch (const std::exception &error) {
        std::cerr << "FAIL: " << error.what() << "; fixture: " << fixture << std::endl;
        std::_Exit(1);
    }
}
