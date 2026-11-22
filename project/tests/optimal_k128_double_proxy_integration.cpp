#include "proxy.h"
#include "datanode.h"
#include "encoder.h"
#include "repair_benchmark_plan.h"

#include <asio.hpp>
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
constexpr int block_size = 4096;
void require(bool condition, const std::string &message)
{
    if (!condition) throw std::runtime_error(message);
}

struct Ports { int proxy, datanode; };

Ports select_ports()
{
    for (int attempt = 0; attempt < 1000; ++attempt) {
        const int offset = ((::getpid() + attempt) % 1000) * 4;
        const Ports ports{28000 + offset, 40000 + offset};
        asio::io_context context;
        std::vector<std::unique_ptr<asio::ip::tcp::acceptor>> reservations;
        try {
            for (int owner = 0; owner < 4; ++owner)
                for (int port : {ports.proxy + owner, ports.proxy + 1000 + owner,
                                 ports.proxy + 2000 + owner, ports.datanode + owner,
                                 ports.datanode + 500 + owner}) {
                    auto acceptor = std::make_unique<asio::ip::tcp::acceptor>(context);
                    acceptor->open(asio::ip::tcp::v4());
                    acceptor->bind({asio::ip::make_address("127.0.0.1"),
                                    static_cast<unsigned short>(port)});
                    reservations.push_back(std::move(acceptor));
                }
            return ports;
        } catch (const std::exception &) {}
    }
    throw std::runtime_error("no free loopback ports for k128 double repair test");
}

std::string endpoint(int port) { return "127.0.0.1:" + std::to_string(port); }
std::string key(int case_index, int id)
{
    return "k128_double_" + std::to_string(case_index) + "_block_" + std::to_string(id);
}
std::filesystem::path stored(const Ports &ports, int owner, int case_index, int id)
{
    return std::filesystem::path("storage") / std::to_string(ports.datanode + owner) /
           key(case_index, id);
}

void write_configuration(const Ports &ports)
{
    std::ofstream nodes("nodes.xml");
    nodes << "<nodes>\n";
    for (int owner = 0; owner < 4; ++owner)
        nodes << "<node id=\"" << owner << "\" proxy=\"" << endpoint(ports.proxy + owner)
              << "\" datanode=\"" << endpoint(ports.datanode + owner) << "\"/>\n";
    nodes << "</nodes>\n";
    std::ofstream parameters("parameters.xml");
    parameters << "<Configuration>\n"
        << "<AlignedSize>4096</AlignedSize><UnitSize>8</UnitSize><BlockSize>" << block_size
        << "</BlockSize><CoordinatorIP>127.0.0.1</CoordinatorIP><CoordinatorPort>55000</CoordinatorPort>\n"
        << "<AppendMode>UNILRC_MODE</AppendMode><CodeType>OptimalLRC</CodeType>\n"
        << "<k>128</k><r>3</r><z>5</z><NodeNum>136</NodeNum>\n</Configuration>\n";
}
} // namespace

int main()
{
    const auto fixture = std::filesystem::temp_directory_path() /
        ("hlrc-optimal-k128-double-" + std::to_string(::getpid()));
    try {
        require(std::filesystem::create_directory(fixture), "fixture directory already exists");
        std::filesystem::current_path(fixture);
        const Ports ports = select_ports();
        write_configuration(ports);
        auto *config = ECProject::Config::getInstance("parameters.xml");

        std::vector<std::vector<unsigned char>> blocks(136,
            std::vector<unsigned char>(block_size));
        std::mt19937 generator(0x128d0u);
        for (int id = 0; id < 128; ++id)
            for (auto &byte : blocks[id]) byte = static_cast<unsigned char>(generator());
        std::vector<unsigned char *> data(128), parity(8);
        for (int id = 0; id < 128; ++id) data[id] = blocks[id].data();
        for (int id = 0; id < 8; ++id) parity[id] = blocks[id + 128].data();
        ECProject::encode_optimal_lrc(128, 3, 5, data.data(), parity.data(), block_size);

        const auto cases = ECProject::RepairBenchmark::make_optimal_uneven_double_block_cases(128);
        require(cases.size() == 4, "wrong k128 double case count");
        for (int owner = 0; owner < 4; ++owner)
            std::filesystem::create_directories(stored(ports, owner, 0, 0).parent_path());
        for (size_t case_index = 0; case_index < cases.size(); ++case_index) {
            require(cases[case_index].tasks.size() == 1, "case must use one shared task");
            require(!cases[case_index].sum_task_times, "case must use shared wall time");
            const auto &task = cases[case_index].tasks[0];
            require(task.failed_block_ids.size() == 2, "shared task must produce two outputs");
            for (int id : task.failed_block_ids)
                for (int owner = 0; owner < 4; ++owner)
                    require(!std::filesystem::exists(stored(ports, owner, case_index, id)),
                            "failed block was unexpectedly preseeded");
            for (int id : task.source_block_ids) {
                std::ofstream file(stored(ports, id % 4, case_index, id), std::ios::binary);
                file.write(reinterpret_cast<const char *>(blocks[id].data()), block_size);
                require(file.good(), "cannot seed source block");
            }
        }

        for (int owner = 0; owner < 4; ++owner) {
            const auto address = endpoint(ports.datanode + owner);
            std::thread([address] { ECProject::DataNode(address, "parameters.xml").Run(); }).detach();
            const auto channel = grpc::CreateChannel(address, grpc::InsecureChannelCredentials());
            require(channel->WaitForConnected(std::chrono::system_clock::now() +
                                              std::chrono::seconds(5)),
                    "datanode did not start");
        }

        std::vector<ECProject::ProxyImpl *> proxies;
        std::vector<std::unique_ptr<grpc::Server>> servers;
        for (int owner = 0; owner < 4; ++owner) {
            auto *proxy = new ECProject::ProxyImpl(endpoint(ports.proxy + owner), "nodes.xml",
                                                  endpoint(55000));
            proxy->m_sys_config = config;
            proxy->m_toolbox = ECProject::ToolBox::getInstance();
            require(proxy->LocalDatanodeStubCountForTest() == 1,
                    "each proxy must access only its paired datanode");
            proxies.push_back(proxy);
            grpc::ServerBuilder builder;
            builder.AddListeningPort(endpoint(ports.proxy + owner),
                                     grpc::InsecureServerCredentials());
            builder.RegisterService(proxy);
            servers.push_back(builder.BuildAndStart());
            require(servers.back() != nullptr, "proxy RPC server did not start");
        }

        for (size_t case_index = 0; case_index < cases.size(); ++case_index) {
            const auto &test_case = cases[case_index];
            const auto &task = test_case.tasks[0];
            const int request_owner = task.failed_block_ids.front() % 4;
            auto channel = grpc::CreateChannel(endpoint(ports.proxy + request_owner),
                                               grpc::InsecureChannelCredentials());
            require(channel->WaitForConnected(std::chrono::system_clock::now() +
                                              std::chrono::seconds(5)),
                    "owner proxy did not become ready");
            auto stub = proxy_proto::proxyService::NewStub(channel);
            proxy_proto::RecoveryRequest request;
            request.set_cross_rack_num(0);
            request.set_failed_block_id(task.failed_block_ids.front());
            request.set_failed_block_key(key(case_index, task.failed_block_ids.front()));
            for (int id : task.failed_block_ids) {
                request.add_failed_block_ids(id);
                request.add_custom_failed_block_keys(key(case_index, id));
                request.add_custom_target_proxy_ips("127.0.0.1");
                request.add_custom_target_proxy_ports(ports.proxy + id % 4);
            }
            for (int id : task.source_block_ids) {
                request.add_custom_source_block_ids(id);
                request.add_blockids(id);
                request.add_blockkeys(key(case_index, id));
                request.add_source_proxy_ips("127.0.0.1");
                request.add_source_proxy_ports(ports.proxy + id % 4);
            }
            grpc::ClientContext context;
            context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(45));
            proxy_proto::RecoveryReply reply;
            const auto status = stub->recovery(&context, request, &reply);
            require(status.ok(), test_case.failed_name + " recovery failed: " +
                                 status.error_message());
            require(reply.per_output_write_back_times_size() == 2,
                    test_case.failed_name + " did not write exactly two generated outputs");

            for (size_t output = 0; output < task.failed_block_ids.size(); ++output) {
                const int id = task.failed_block_ids[output];
                const int owner = id % 4;
                require(output == 0 ? owner == request_owner : owner != request_owner,
                        test_case.failed_name + " output order does not represent local then remote");
                require(std::filesystem::exists(stored(ports, owner, case_index, id)),
                        "repaired block is missing");
                std::ifstream file(stored(ports, owner, case_index, id), std::ios::binary);
                std::vector<unsigned char> actual(block_size);
                file.read(reinterpret_cast<char *>(actual.data()), block_size);
                require(file.gcount() == block_size && actual == blocks[id],
                        "persisted block bytes differ from original: " + std::to_string(id));
                for (int other = 0; other < 4; ++other)
                    if (other != owner)
                        require(!std::filesystem::exists(stored(ports, other, case_index, id)),
                                "repaired block was written to the wrong datanode");
            }
        }

        for (auto &server : servers) server->Shutdown();
        std::filesystem::current_path(fixture.parent_path());
        std::filesystem::remove_all(fixture);
        std::cout << "PASS: OptimalLRC k128 shared double repairs read exact source sets, "
                     "generate two outputs once, persist the first locally, and forward the "
                     "second through its owner proxy" << std::endl;
        std::_Exit(0);
    } catch (const std::exception &error) {
        std::cerr << "FAIL: " << error.what() << "; fixture: " << fixture << std::endl;
        std::_Exit(1);
    }
}
