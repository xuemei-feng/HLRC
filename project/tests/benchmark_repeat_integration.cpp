#include "coordinator.grpc.pb.h"
#include "encoder.h"
#include <asio.hpp>
#include <grpcpp/grpcpp.h>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <mutex>
#include <spawn.h>
#include <stdexcept>
#include <sys/wait.h>
#include <thread>
#include <vector>
#include <unistd.h>
#include <fcntl.h>

extern char **environ;
namespace {
void require(bool ok, const std::string &message)
{ if (!ok) throw std::runtime_error(message); }

// A controlled RPC fixture exercises the real CLI's stripe lifecycle, rounds,
// warmup exclusion and failure reporting. Real repair bytes are tested by the
// separate proxy/datanode integrations.
class Fixture final : public coordinator_proto::coordinatorService::Service {
public:
    std::atomic<int> set_calls{0}, single_calls{0}, multi_calls{0};
    std::atomic<int> fail_at{0}, warmup_calls{0};
    std::atomic<bool> omit_task_times{false};
    std::atomic<size_t> received{0};
    std::mutex mutex;
    std::vector<int> stripes{7}, repaired;
    int upload_port = 0;
    grpc::Status sayHelloToCoordinator(grpc::ServerContext *,
        const coordinator_proto::RequestToCoordinator *, coordinator_proto::ReplyFromCoordinator *reply) override
    { reply->set_message("repeat benchmark fixture"); return grpc::Status::OK; }
    grpc::Status listStripes(grpc::ServerContext *, const coordinator_proto::RequestToCoordinator *,
                            coordinator_proto::RepStripeIds *reply) override
    {
        std::lock_guard<std::mutex> lock(mutex);
        for (int id : stripes) reply->add_stripe_ids(id);
        return grpc::Status::OK;
    }
    grpc::Status uploadSetValue(grpc::ServerContext *, const coordinator_proto::RequestProxyIPPort *,
                               coordinator_proto::ReplyProxyIPsPorts *reply) override
    {
        ++set_calls;
        {
            std::lock_guard<std::mutex> lock(mutex);
            stripes.push_back(stripes.back() + 1);
        }
        const auto data = ECProject::get_data_block_num_per_group(64, 3, 5, "NLRC");
        const auto global = ECProject::get_global_parity_block_num_per_group(64, 3, 5, "NLRC");
        const auto local = ECProject::get_local_parity_block_num_per_group(64, 3, 5, "NLRC");
        size_t total = 0;
        for (size_t i = 0; i < data.size(); ++i) {
            const size_t size = (data[i] + global[i] + local[i]) * 4096;
            reply->add_append_keys("fixture_" + std::to_string(i));
            reply->add_proxyips("127.0.0.1");
            reply->add_proxyports(upload_port);
            reply->add_node_slice_sizes(size);
            total += size;
        }
        reply->set_sum_append_size(total);
        return grpc::Status::OK;
    }
    grpc::Status checkCommitAbort(grpc::ServerContext *, const coordinator_proto::AskIfSuccess *,
                                 coordinator_proto::RepIfSuccess *reply) override
    { reply->set_ifcommit(true); return grpc::Status::OK; }
    grpc::Status result(int stripe, int call, coordinator_proto::RecoveryReply *reply)
    {
        {
            std::lock_guard<std::mutex> lock(mutex);
            repaired.push_back(stripe);
        }
        if (call == fail_at.load())
            return grpc::Status(grpc::StatusCode::UNAVAILABLE, "injected repair failure");
        reply->set_custom_recovery_time(call <= warmup_calls.load() ? 100.0 : 0.01);
        return grpc::Status::OK;
    }
    grpc::Status customRecovery(grpc::ServerContext *, const coordinator_proto::CustomRecoveryRequest *request,
                                coordinator_proto::RecoveryReply *reply) override
    { return result(request->stripe_id(), ++single_calls, reply); }
    grpc::Status customDoubleRecovery(grpc::ServerContext *,
        const coordinator_proto::CustomDoubleRecoveryRequest *request,
        coordinator_proto::RecoveryReply *reply) override
    {
        const int call = ++multi_calls;
        const auto status = result(request->stripe_id(), call, reply);
        if (status.ok() && !omit_task_times)
            for (int i = 0; i < request->tasks_size(); ++i)
                reply->add_custom_task_recovery_times(call <= warmup_calls.load()
                    ? 100.0 + i : (i == 0 ? 0.012 : 0.023));
        return status;
    }
    void reset(int warmup = 0, int failure = 0)
    {
        single_calls = 0; multi_calls = 0; warmup_calls = warmup; fail_at = failure;
        std::lock_guard<std::mutex> lock(mutex);
        repaired.clear();
    }
    void require_stripe(int stripe)
    {
        std::lock_guard<std::mutex> lock(mutex);
        require(!repaired.empty(), "no repair requests received");
        for (int id : repaired) require(id == stripe, "stripe changed between rounds");
    }
};
std::pair<int, std::string> run(const std::string &binary, const std::filesystem::path &directory,
                              const std::vector<std::string> &options)
{
    const auto output = directory / "output.txt";
    const auto config = (directory / "parameters.xml").string();
    std::vector<std::string> arguments{binary, "64"};
    arguments.insert(arguments.end(), options.begin(), options.end());
    arguments.insert(arguments.end(), {"--config", config});
    std::vector<char *> argv;
    for (auto &argument : arguments) argv.push_back(argument.data());
    argv.push_back(nullptr);
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, output.c_str(),
                                     O_WRONLY | O_CREAT | O_TRUNC, 0600);
    posix_spawn_file_actions_adddup2(&actions, STDOUT_FILENO, STDERR_FILENO);
    pid_t child;
    const int spawned = posix_spawn(&child, binary.c_str(), &actions, nullptr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&actions);
    require(spawned == 0, "cannot launch real repair_benchmark");
    int status;
    require(waitpid(child, &status, 0) == child, "cannot wait for benchmark");
    std::ifstream file(output);
    std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return {WIFEXITED(status) ? WEXITSTATUS(status) : 128, text};
}
} // namespace

int main(int argc, char **argv)
{
    const auto directory = std::filesystem::temp_directory_path() /
        ("hlrc-repeat-benchmark-" + std::to_string(::getpid()));
    try {
        require(argc == 2, "expected repair_benchmark binary path");
        require(std::filesystem::create_directory(directory), "fixture directory exists");
        auto *fixture = new Fixture;
        asio::io_context io;
        auto *acceptor = new asio::ip::tcp::acceptor(io,
            {asio::ip::make_address("127.0.0.1"), 0});
        fixture->upload_port = acceptor->local_endpoint().port();
        std::thread([fixture, acceptor, &io] {
            for (;;) {
                asio::ip::tcp::socket socket(io);
                acceptor->accept(socket);
                char buffer[65536]; asio::error_code error;
                for (;;) {
                    const size_t count = socket.read_some(asio::buffer(buffer), error);
                    fixture->received.fetch_add(count);
                    if (error) break;
                }
            }
        }).detach();
        grpc::ServerBuilder builder;
        int port = 0;
        builder.AddListeningPort("127.0.0.1:0", grpc::InsecureServerCredentials(), &port);
        builder.RegisterService(fixture);
        auto server = builder.BuildAndStart();
        require(server != nullptr, "fixture RPC server failed");
        auto write_config = [&](const std::string &code, int z) {
            std::ofstream config(directory / "parameters.xml");
            config << "<Configuration><AlignedSize>4096</AlignedSize><UnitSize>4096</UnitSize>"
                   << "<BlockSize>4096</BlockSize><CoordinatorIP>127.0.0.1</CoordinatorIP>"
                   << "<CoordinatorPort>" << port << "</CoordinatorPort><ClientIP>127.0.0.1</ClientIP>"
                   << "<AppendMode>UNILRC_MODE</AppendMode><CodeType>" << code << "</CodeType>"
                   << "<k>64</k><r>3</r><z>" << z << "</z><NodeNum>72</NodeNum></Configuration>";
        };
        write_config("NLRC", 5);
        fixture->reset(2);
        auto first = run(argv[1], directory, {"single", "--repeat", "3", "--warmup", "1"});
        require(first.first == 0, first.second);
        require(fixture->set_calls == 1 && fixture->single_calls == 8, "new stripe was not created exactly once");
        fixture->require_stripe(8);
        require(first.second.find("success=3/3 min=0.01 median=0.01 p95=0.01 max=0.01 s") != std::string::npos,
                "warmup contaminated measured statistics: " + first.second);
        std::cout << "PASS: one set, one fixed stripe, 3 measured rounds, warmup excluded" << std::endl;

        fixture->reset();
        auto reused = run(argv[1], directory, {"single", "--stripe-id", "8", "--repeat", "2"});
        require(reused.first == 0, reused.second);
        require(fixture->set_calls == 1 && fixture->single_calls == 4, "existing stripe triggered set or wrong rounds");
        fixture->require_stripe(8);
        std::cout << "PASS: existing stripe reused across CLI processes without another set" << std::endl;

        fixture->reset();
        auto absent = run(argv[1], directory, {"single", "--stripe-id", "999"});
        require(absent.first != 0 && fixture->set_calls == 1 && fixture->single_calls == 0,
                "missing stripe was silently created or repaired");
        fixture->reset(2);
        auto multi = run(argv[1], directory, {"quadruple", "--stripe-id", "8", "--repeat", "2", "--warmup", "1"});
        require(multi.first == 0, multi.second);
        require(fixture->multi_calls == 6 && fixture->set_calls == 1, "multi repair round count changed");
        fixture->require_stripe(8);
        std::cout << "PASS: repeated multi-block cases keep fixed stripe; missing stripe rejected" << std::endl;

        fixture->reset(2, 1);
        auto warmup_failure = run(argv[1], directory, {"single", "--stripe-id", "8", "--repeat", "3", "--warmup", "1"});
        require(warmup_failure.first != 0 && fixture->single_calls == 1 &&
                warmup_failure.second.find("Measured round") == std::string::npos, "warmup failure was ignored");
        fixture->reset(0, 1);
        auto failure = run(argv[1], directory, {"single", "--stripe-id", "8", "--repeat", "3"});
        require(failure.first != 0 && fixture->single_calls == 6 &&
                failure.second.find("success=2/3") != std::string::npos, "failed repair counted as successful sample");
        require(fixture->set_calls == 1, "unexpected additional stripe creation");
        std::cout << "PASS: warmup abort and measured failure counts remain explicit" << std::endl;

        write_config("OptimalLRC", 3);
        fixture->reset(3);
        auto timed = run(argv[1], directory,
            {"double", "--stripe-id", "8", "--repeat", "2", "--warmup", "1"});
        require(timed.first == 0 && fixture->multi_calls == 9, timed.second);
        for (const std::string name : {"L0+L1/L0", "L0+L2/L0"})
            require(timed.second.find("Task result [OptimalLRC] " + name + ": 0.012 s") != std::string::npos,
                    "missing first task timing: " + timed.second);
        for (const std::string name : {"L0+L1/L1", "L0+L2/L2"})
            require(timed.second.find("Task result [OptimalLRC] " + name + ": 0.023 s") != std::string::npos,
                    "missing second task timing: " + timed.second);
        for (const std::string name : {"L0+L1", "L0+L2"}) {
            require(timed.second.find("Result [OptimalLRC] " + name + ": 0.035 s (sum; wall=0.01 s)")
                        != std::string::npos, "case must report sum and wall time: " + timed.second);
            require(timed.second.find("Summary [OptimalLRC] " + name +
                        ": success=2/2 min=0.035 median=0.035 p95=0.035 max=0.035 s")
                        != std::string::npos, "sum statistics include warmup or wall time: " + timed.second);
        }
        require(timed.second.find("Result [OptimalLRC] G0+G1: 0.01 s") != std::string::npos,
                "shared global task must keep end-to-end time: " + timed.second);
        fixture->omit_task_times = true;
        fixture->reset();
        auto stale = run(argv[1], directory, {"double", "--stripe-id", "8"});
        require(stale.first != 0 && stale.second.find("Missing or invalid per-task") != std::string::npos,
                "missing task timings silently produced a zero sum: " + stale.second);
        fixture->omit_task_times = false;
        write_config("UniformLRC", 3);
        fixture->reset(3);
        auto uniform_timed = run(argv[1], directory,
            {"double", "--stripe-id", "8", "--repeat", "2", "--warmup", "1"});
        require(uniform_timed.first == 0 && fixture->multi_calls == 9, uniform_timed.second);
        for (const std::string name : {"L0+L1/L0", "L0+L2/L0"})
            require(uniform_timed.second.find("Task result [UniformLRC] " + name + ": 0.012 s")
                        != std::string::npos,
                    "missing UniformLRC first task timing: " + uniform_timed.second);
        for (const std::string name : {"L0+L1/L1", "L0+L2/L2"})
            require(uniform_timed.second.find("Task result [UniformLRC] " + name + ": 0.023 s")
                        != std::string::npos,
                    "missing UniformLRC second task timing: " + uniform_timed.second);
        for (const std::string name : {"L0+L1", "L0+L2"}) {
            require(uniform_timed.second.find("Result [UniformLRC] " + name +
                        ": 0.035 s (sum; wall=0.01 s)") != std::string::npos,
                    "UniformLRC case must report sum and wall time: " + uniform_timed.second);
            require(uniform_timed.second.find("Summary [UniformLRC] " + name +
                        ": success=2/2 min=0.035 median=0.035 p95=0.035 max=0.035 s")
                        != std::string::npos,
                    "UniformLRC sum statistics are wrong: " + uniform_timed.second);
        }
        require(uniform_timed.second.find("Result [UniformLRC] G0+G1: 0.01 s")
                    != std::string::npos,
                "UniformLRC shared global task must keep end-to-end time: " + uniform_timed.second);
        write_config("NLRC", 5);
        fixture->reset(3);
        auto nlrc_timed = run(argv[1], directory,
            {"triple", "--stripe-id", "8", "--repeat", "2", "--warmup", "1"});
        require(nlrc_timed.first == 0 && fixture->multi_calls == 9, nlrc_timed.second);
        for (const std::string name : {"L0+L1+L2/L0+L1", "L0+L1+L4/L0+L1"})
            require(nlrc_timed.second.find("Task result [NLRC] " + name + ": 0.012 s")
                        != std::string::npos,
                    "missing NLRC shared local task timing: " + nlrc_timed.second);
        for (const std::string name : {"L0+L1+L2/L2", "L0+L1+L4/L4"})
            require(nlrc_timed.second.find("Task result [NLRC] " + name + ": 0.023 s")
                        != std::string::npos,
                    "missing NLRC second local task timing: " + nlrc_timed.second);
        for (const std::string name : {"L0+L1+L2", "L0+L1+L4"}) {
            require(nlrc_timed.second.find("Result [NLRC] " + name +
                        ": 0.035 s (sum; wall=0.01 s)") != std::string::npos,
                    "NLRC case must report sum and wall time: " + nlrc_timed.second);
            require(nlrc_timed.second.find("Summary [NLRC] " + name +
                        ": success=2/2 min=0.035 median=0.035 p95=0.035 max=0.035 s")
                        != std::string::npos,
                    "NLRC sum statistics are wrong: " + nlrc_timed.second);
        }
        require(nlrc_timed.second.find("Result [NLRC] G0+G1+G2: 0.01 s")
                    != std::string::npos,
                "NLRC shared global task must keep end-to-end time: " + nlrc_timed.second);
        std::cout << "PASS: OptimalLRC/UniformLRC k64 task times, sums, shared global timing"
                  << "; NLRC k64 triple task times and sums; stale replies" << std::endl;
        server->Shutdown();
        std::filesystem::remove_all(directory);
        std::_Exit(0);
    } catch (const std::exception &error) {
        std::cerr << "FAIL: " << error.what() << "; fixture: " << directory << std::endl;
        std::_Exit(1);
    }
}
