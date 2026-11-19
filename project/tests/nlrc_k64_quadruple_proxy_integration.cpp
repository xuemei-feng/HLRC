#include "proxy.h"
#include "datanode.h"
#include "encoder.h"
#include "repair_benchmark_plan.h"

#include <asio.hpp>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <memory>
#include <random>
#include <set>
#include <stdexcept>
#include <thread>
#include <unistd.h>
#include <vector>

namespace {
constexpr int block_size = 1048576, node_count = 8;

class FailingDatanode final : public datanode_proto::datanodeService::Service {
    grpc::Status handleGet(grpc::ServerContext *, const datanode_proto::GetInfo *,
                           datanode_proto::RequestResult *) override
    {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "injected datanode read failure");
    }
};
void require(bool condition, const std::string &message)
{
    if (!condition) throw std::runtime_error(message);
}

struct Ports { int proxy, datanode; };
Ports select_ports()
{
    for (int attempt = 0; attempt < 1000; ++attempt) {
        const int offset = ((::getpid() + attempt) % 1000) * node_count;
        const Ports ports{27000 + offset, 41000 + offset};
        asio::io_context context;
        std::vector<std::unique_ptr<asio::ip::tcp::acceptor>> reservations;
        try {
            for (int index = 0; index <= node_count; ++index)
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
    throw std::runtime_error("no free loopback ports for NLRC quadruple repair test");
}

std::string endpoint(int port) { return "127.0.0.1:" + std::to_string(port); }
std::string key(size_t case_index, int id)
{
    return "nlrc_quadruple_" + std::to_string(case_index) + "_" + std::to_string(id);
}
std::filesystem::path stored(const Ports &ports, int owner, size_t case_index, int id)
{
    return std::filesystem::path("storage") / std::to_string(ports.datanode + owner) /
        key(case_index, id);
}

void write_fixtures(const Ports &ports)
{
    std::ofstream nodes("nodes.xml");
    nodes << "<nodes>\n";
    for (int index = 0; index <= node_count; ++index)
        nodes << "<node id=\"" << index << "\" proxy=\"" << endpoint(ports.proxy + index)
              << "\" datanode=\"" << endpoint(ports.datanode + index) << "\"/>\n";
    nodes << "</nodes>\n";
    std::ofstream parameters("parameters.xml");
    parameters << "<Configuration>\n"
        << "<AlignedSize>4096</AlignedSize><UnitSize>8192</UnitSize><BlockSize>" << block_size
        << "</BlockSize><CoordinatorIP>127.0.0.1</CoordinatorIP><CoordinatorPort>55000</CoordinatorPort>\n"
        << "<AppendMode>UNILRC_MODE</AppendMode><CodeType>NLRC</CodeType>\n"
        << "<k>64</k><r>3</r><z>5</z><NodeNum>72</NodeNum>\n</Configuration>\n";
}
} // namespace

int main(int argc, char **argv)
{
    const bool triple = argc == 2 && std::string(argv[1]) == "triple";
    // Keep real network servers alive until process exit; isolate all storage.
    const auto fixture = std::filesystem::temp_directory_path() /
        (std::string("hlrc-nlrc-k64-") + (triple ? "triple-" : "quadruple-") +
         std::to_string(::getpid()));
    try {
        require(std::filesystem::create_directory(fixture), "fixture directory already exists");
        std::filesystem::current_path(fixture);
        require(::setenv("HLRC_REPAIR_WORKERS", "2", 1) == 0, "cannot configure test workers");
        const Ports ports = select_ports();
        write_fixtures(ports);
        auto *config = ECProject::Config::getInstance("parameters.xml");

        std::vector<std::vector<unsigned char>> blocks(72,
            std::vector<unsigned char>(block_size));
        std::mt19937 generator(0x4e4c6434u);
        for (int id = 0; id < 64; ++id)
            for (auto &byte : blocks[id]) byte = static_cast<unsigned char>(generator());
        std::vector<unsigned char *> data(64), parity(8);
        for (int id = 0; id < 64; ++id) data[id] = blocks[id].data();
        for (int id = 0; id < 8; ++id) parity[id] = blocks[id + 64].data();
        ECProject::encode_nlrc(64, 3, 5, data.data(), parity.data(), block_size);

        for (int owner = 0; owner < node_count; ++owner) {
            std::filesystem::create_directories(stored(ports, owner, 0, 0).parent_path());
            const auto address = endpoint(ports.datanode + owner);
            std::thread([address] { ECProject::DataNode(address, "parameters.xml").Run(); }).detach();
            const auto channel = grpc::CreateChannel(address, grpc::InsecureChannelCredentials());
            require(channel->WaitForConnected(std::chrono::system_clock::now() + std::chrono::seconds(5)),
                    "datanode did not start");
        }

        // Distinct owners for G0/G1/G2/L0-L4 and one extra failure fixture.
        std::vector<ECProject::ProxyImpl *> proxies;
        for (int owner = 0; owner <= node_count; ++owner) {
            auto *proxy = new ECProject::ProxyImpl(endpoint(ports.proxy + owner), "nodes.xml",
                                                  endpoint(55000));
            proxy->m_sys_config = config;
            proxy->m_toolbox = ECProject::ToolBox::getInstance();
            require(proxy->LocalDatanodeStubCountForTest() == 1,
                    "each proxy must access only its paired datanode");
            proxies.push_back(proxy);
        }
        // Only the specified task leaders expose recovery RPCs. Other owners
        // must receive reconstructed blocks through the production peer gateway.
        std::vector<std::unique_ptr<grpc::Server>> servers;
        for (int owner : {0, 3, 5, 7}) {
            grpc::ServerBuilder builder;
            const auto address = endpoint(ports.proxy + owner);
            builder.AddListeningPort(address, grpc::InsecureServerCredentials());
            builder.RegisterService(proxies[owner]);
            auto server = builder.BuildAndStart();
            require(server != nullptr, "leader proxy RPC server did not start");
            servers.push_back(std::move(server));
            const auto channel = grpc::CreateChannel(address, grpc::InsecureChannelCredentials());
            require(channel->WaitForConnected(std::chrono::system_clock::now() + std::chrono::seconds(5)),
                    "leader proxy did not become ready");
        }

        const auto cases = triple
            ? ECProject::RepairBenchmark::make_nlrc_k64_triple_block_cases()
            : ECProject::RepairBenchmark::make_nlrc_k64_quadruple_block_cases();
        require(cases.size() == (triple ? 3u : 2u), "unexpected NLRC case count");
        using ECProject::RepairBenchmark::inclusive_range;
        auto l4_sources = inclusive_range(44, 63);
        l4_sources.insert(l4_sources.end(), {64, 65, 66});
        const std::vector<std::vector<std::vector<int>>> wanted_failed = triple
            ? std::vector<std::vector<std::vector<int>>>{
                  {{67, 68}, {69}}, {{67, 68}, {71}}, {{64, 65, 66}}}
            : std::vector<std::vector<std::vector<int>>>{
                  {{67, 68}, {69, 70}}, {{64, 65, 66, 67}}};
        const std::vector<std::vector<std::vector<int>>> wanted_sources = triple
            ? std::vector<std::vector<std::vector<int>>>{
                  {inclusive_range(0, 21), inclusive_range(22, 43)},
                  {inclusive_range(0, 21), l4_sources},
                  {inclusive_range(0, 63)}}
            : std::vector<std::vector<std::vector<int>>>{
                  {inclusive_range(0, 21), inclusive_range(22, 43)},
                  {inclusive_range(0, 63)}};
        for (size_t case_index = 0; case_index < cases.size(); ++case_index) {
            const auto &test_case = cases[case_index];
            require(test_case.tasks.size() == wanted_failed[case_index].size(), "wrong task count");
            std::set<int> failed, sources;
            for (size_t index = 0; index < test_case.tasks.size(); ++index) {
                const auto &task = test_case.tasks[index];
                require(task.failed_block_ids == wanted_failed[case_index][index], "wrong output order");
                require(task.source_block_ids == wanted_sources[case_index][index], "wrong read set");
                failed.insert(task.failed_block_ids.begin(), task.failed_block_ids.end());
                for (int id : task.source_block_ids)
                    require(sources.insert(id).second, "source read repeated across tasks");
            }
            require(failed.size() == (triple ? 3u : 4u), "wrong distinct failed block count");
            // No parity exists. In the local case even D44-D63 are absent,
            // so a fallback to reading all data would fail this test.
            for (int id : sources) {
                std::ofstream file(stored(ports, id % node_count, case_index, id), std::ios::binary);
                file.write(reinterpret_cast<const char *>(blocks[id].data()), block_size);
                require(file.good(), "cannot seed data block");
            }

            for (int round = 0; round < 3; ++round) {
                std::vector<std::future<grpc::Status>> repairs;
                for (const auto &task : test_case.tasks) {
                    proxy_proto::RecoveryRequest request;
                    request.set_cross_rack_num(0);
                    request.set_failed_block_id(task.failed_block_ids.front());
                    request.set_failed_block_key(key(case_index, task.failed_block_ids.front()));
                    for (int id : task.failed_block_ids) {
                        request.add_failed_block_ids(id);
                        request.add_custom_failed_block_keys(key(case_index, id));
                        request.add_custom_target_proxy_ips("127.0.0.1");
                        request.add_custom_target_proxy_ports(ports.proxy + id - 64);
                    }
                    for (int id : task.source_block_ids) {
                        request.add_custom_source_block_ids(id);
                        request.add_blockids(id);
                        request.add_blockkeys(key(case_index, id));
                        request.add_source_proxy_ips("127.0.0.1");
                        request.add_source_proxy_ports(ports.proxy + id % node_count);
                    }
                    const auto leader = endpoint(ports.proxy + task.failed_block_ids.front() - 64);
                    repairs.push_back(std::async(std::launch::async, [request, leader] {
                        auto channel = grpc::CreateChannel(leader, grpc::InsecureChannelCredentials());
                        auto stub = proxy_proto::proxyService::NewStub(channel);
                        grpc::ClientContext context;
                        context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(40));
                        proxy_proto::RecoveryReply reply;
                        return stub->recovery(&context, request, &reply);
                    }));
                }
                for (auto &repair : repairs) {
                    const auto status = repair.get();
                    require(status.ok(), "shared recovery failed: " + status.error_message());
                }

                for (int id : failed) {
                    const int owner = id - 64;
                    const auto path = stored(ports, owner, case_index, id);
                    require(std::filesystem::exists(path), "repaired block is missing");
                    require(std::filesystem::file_size(path) == block_size, "wrong repaired block size");
                    std::ifstream file(path, std::ios::binary);
                    std::vector<unsigned char> actual(block_size);
                    file.read(reinterpret_cast<char *>(actual.data()), block_size);
                    require(file.gcount() == block_size && actual == blocks[id],
                            "persisted bytes differ from original: " + std::to_string(id));
                    for (int other = 0; other < node_count; ++other)
                        if (other != owner)
                            require(!std::filesystem::exists(stored(ports, other, case_index, id)),
                                    "repaired block written to the wrong datanode");
                }
            }
            for (const auto &task : test_case.tasks) {
                auto *leader = proxies[task.failed_block_ids.front() - 64];
                require(leader->RecoveryPoolStartsForTest() == 1, "repair pool was recreated");
                require(leader->RecoveryWorkerCountForTest() == 2, "worker limit was ignored");
            }
            std::cout << "PASS: NLRC k64 " << test_case.failed_name << ", "
                      << test_case.tasks.size() << " shared tasks, " << sources.size()
                      << " sources, three rounds reuse a two-worker pool; "
                      << failed.size() << " 1 MiB blocks verified each round"
                      << std::endl;
        }
        // A failed read RPC must propagate through the peer gateway without
        // connecting to the data socket and waiting forever for missing bytes.
        FailingDatanode failing_datanode;
        grpc::ServerBuilder failure_builder;
        failure_builder.AddListeningPort(endpoint(ports.datanode + node_count),
                                         grpc::InsecureServerCredentials());
        failure_builder.RegisterService(&failing_datanode);
        auto failure_server = failure_builder.BuildAndStart();
        require(failure_server != nullptr, "read-failure RPC server did not start");
        asio::io_context failure_io;
        asio::ip::tcp::acceptor unused_data_socket(failure_io,
            {asio::ip::make_address("127.0.0.1"),
             static_cast<unsigned short>(ports.datanode + node_count + 500)});
        std::string failed_read(block_size, '\0');
        require(!proxies[0]->PeerGetForTest("127.0.0.1", ports.proxy + node_count,
                                          "injected_failure", failed_read),
                "failed datanode read RPC was reported as success");
        unused_data_socket.non_blocking(true);
        asio::ip::tcp::socket unexpected(failure_io);
        asio::error_code error;
        unused_data_socket.accept(unexpected, error);
        require(error == asio::error::would_block || error == asio::error::try_again,
                "failed read RPC must not connect to the datanode data socket");
        std::cout << "PASS: failed datanode read RPC returns failure without waiting for TCP data"
                  << std::endl;
        failure_server->Shutdown();
        for (auto &server : servers) server->Shutdown();
        std::filesystem::current_path(fixture.parent_path());
        std::filesystem::remove_all(fixture);
        std::_Exit(0);
    } catch (const std::exception &error) {
        std::cerr << "FAIL: " << error.what() << "; fixture: " << fixture << std::endl;
        std::_Exit(1);
    }
}
