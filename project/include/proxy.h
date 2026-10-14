#ifndef PROXY_H
#define PROXY_H
#include "coordinator.grpc.pb.h"
#include "proxy.grpc.pb.h"
#include "datanode.grpc.pb.h"
#include "devcommon.h"
#include "meta_definition.h"
#include "lrc.h"
#include <asio.hpp>
#include <grpc++/health_check_service_interface.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <thread>
#include <semaphore.h>
#include <config.h>
#include <toolbox.h>
#include <queue>
// #define IF_DEBUG true
#define IF_DEBUG false
namespace ECProject
{
  class ProxyImpl final
      : public proxy_proto::proxyService::Service,
        public std::enable_shared_from_this<ECProject::ProxyImpl>
  {

  public:
    ProxyImpl(std::string proxy_ip_port, std::string config_path, std::string coordinator_address) : config_path(config_path), proxy_ip_port(proxy_ip_port), acceptor(io_context, asio::ip::tcp::endpoint(asio::ip::address::from_string(proxy_ip_port.substr(0, proxy_ip_port.find(':')).c_str()), ECProject::PROXY_PORT_SHIFT + std::stoi(proxy_ip_port.substr(proxy_ip_port.find(':') + 1, proxy_ip_port.size())))), m_coordinator_address(coordinator_address)
    {
      m_ip = proxy_ip_port.substr(0, proxy_ip_port.find(':'));
      m_port = std::stoi(proxy_ip_port.substr(proxy_ip_port.find(':') + 1, proxy_ip_port.size()));
      init_coordinator();
      init_datanodes(config_path);
      std::cout << "Node id:" << m_self_node_id << std::endl;
    }
    ~ProxyImpl()
    {
      if (m_peer_acceptor) { asio::error_code ec; m_peer_acceptor->close(ec); }
      if (m_peer_thread.joinable()) m_peer_thread.detach();
    };
    grpc::Status checkalive(
        grpc::ServerContext *context,
        const proxy_proto::CheckaliveCMD *request,
        proxy_proto::RequestResult *response) override;
    // encode and set
    grpc::Status encodeAndSetObject(
        grpc::ServerContext *context,
        const proxy_proto::ObjectAndPlacement *object_and_placement,
        proxy_proto::SetReply *response) override;
    // append
    grpc::Status scheduleAppend2Datanode(
        grpc::ServerContext *context,
        const proxy_proto::AppendStripeDataPlacement *append_stripe_data_placement,
        proxy_proto::SetReply *response) override;
    // decode and get
    grpc::Status decodeAndGetObject(
        grpc::ServerContext *context,
        const proxy_proto::ObjectAndPlacement *object_and_placement,
        proxy_proto::GetReply *response) override;
    // degraded read
    grpc::Status degradedRead(
        grpc::ServerContext *context,
        const proxy_proto::DegradedReadRequest *degraded_read_request,
        proxy_proto::DegradedReadReply *response) override;
    grpc::Status degradedRead2Client(
      grpc::ServerContext *context,
      const proxy_proto::RecoveryRequest *recovery_request,
      proxy_proto::DegradedReadReply *response) override;
    grpc::Status degradedReadBreakdown(
      grpc::ServerContext *context,
      const proxy_proto::DegradedReadRequest *degraded_read_request,
      proxy_proto::DegradedReadReply *response) override;
    grpc::Status degradedRead2ClientBreakdown(
      grpc::ServerContext *context,
      const proxy_proto::RecoveryRequest *recovery_request,
      proxy_proto::DegradedReadReply *response) override;
    grpc::Status degradedReadWithBlockStripeID(
        grpc::ServerContext *context,
        const proxy_proto::DegradedReadRequest *degraded_read_request,
        proxy_proto::GetReply *response) override;
    grpc::Status partialDecoding(
        grpc::ServerContext *context,
        const proxy_proto::PartialDecodingRequest *partial_decoding_request,
        proxy_proto::DegradedReadReply *response) override;
    // recovery
    grpc::Status recovery(
        grpc::ServerContext *context,
        const proxy_proto::RecoveryRequest *recovery_request,
        proxy_proto::RecoveryReply *response) override;
    grpc::Status recoveryBreakdown(
        grpc::ServerContext *context,
        const proxy_proto::RecoveryRequest *recovery_request,
        proxy_proto::RecoveryReply *response) override;

    grpc::Status multipleRecovery(
        grpc::ServerContext *context,
        const proxy_proto::MultipleRecoveryRequest *multiple_recovery_request,
        proxy_proto::GetReply *response) override;
    // delete
    grpc::Status deleteBlock(
        grpc::ServerContext *context,
        const proxy_proto::NodeAndBlock *node_and_block,
        proxy_proto::DelReply *response) override;
    // get stripe
    grpc::Status getBlocks(
        grpc::ServerContext *context,
        const proxy_proto::StripeAndBlockIDs *request,
        proxy_proto::GetReply *response) override;

    bool SetToDatanode(const char *key, size_t key_length, const char *value, size_t value_length, const char *ip, int port, int offset);
    bool GetFromDatanode(const char *key, size_t key_length, char *value, size_t value_length, const char *ip, int port, int offset);
    bool DelInDatanode(std::string key, std::string node_ip_port);
    bool PutToOwnerProxy(const std::string &proxy_ip, int proxy_port, const std::string &key,
                         int block_id, const char *data, size_t size, bool recovery);
    bool GetFromOwnerProxy(const std::string &proxy_ip, int proxy_port, const std::string &key,
                           char *data, size_t size);
    bool DeleteFromOwnerProxy(const std::string &proxy_ip, int proxy_port, const std::string &key);

    ECProject::Config *m_sys_config;
    ECProject::ToolBox *m_toolbox;
    std::queue<std::shared_ptr<char[]>> m_pre_allocated_buffer_queue;
    bool AppendToDatanode(const char *block_key, int block_id, size_t append_size, const char *append_buf, int append_offset, const char *ip, int port, bool is_serialized);
    bool MergeParityOnDatanode(const char *block_key, int block_id, const char *ip, int port, const std::string &append_mode);
    void printAppendStripeDataPlacement(const proxy_proto::AppendStripeDataPlacement *append_stripe_data_placement);
    std::vector<unsigned char *> convertToUnsignedCharArray(std::vector<char*> &input);
    bool GetFromDatanode(const std::string &key, char *value, const size_t value_length, const char *ip, const int port);
    bool GetFromDatanode(const std::string &key, char *value, const size_t value_length, const char *ip, const int port, 
      double *disk_io_start_time, double *disk_io_end_time, double *network_start_time, double *network_end_time, double *grpc_notify_time, double *grpc_start_time);
    bool RecoveryToDatanode(const char *block_key, int block_id, const char *buf, const char *ip, int port);
    bool RecoveryToDatanodeBreakdown(const char *block_key, int block_id, const char *buf, const char *ip, int port, double *network_time, double *disk_io_time);
    void get_from_node(const std::string &block_key, char *block_value, const size_t block_size, const char *datanode_ip, const int datanode_port, bool *status, int index);
    void get_from_node_breakdown(const std::string &block_key, char *block_value, const size_t block_size, const char *datanode_ip, const int datanode_port, bool *status, int index, 
      double *disk_io_start_time, double *disk_io_end_time, double *network_start_time, double *network_end_time, double *grpc_notify_time, double *grpc_start_time);

    bool PeerPutForTest(const std::string &proxy_ip, int proxy_port, const std::string &key, const std::string &data)
    { return PutToOwnerProxy(proxy_ip, proxy_port, key, 0, data.data(), data.size(), false); }
    bool PeerGetForTest(const std::string &proxy_ip, int proxy_port, const std::string &key, std::string &data)
    { return GetFromOwnerProxy(proxy_ip, proxy_port, key, data.data(), data.size()); }
    bool PeerDeleteForTest(const std::string &proxy_ip, int proxy_port, const std::string &key)
    { return DeleteFromOwnerProxy(proxy_ip, proxy_port, key); }
    size_t LocalDatanodeStubCountForTest() const { return m_datanode_ptrs.size(); }

  private:
    std::mutex m_mutex;
    std::condition_variable cv;
    bool init_coordinator();
    bool init_datanodes(std::string datanodeinfo_path);
    void run_peer_gateway();
    bool handle_peer_connection(asio::ip::tcp::socket socket);
    std::pair<std::string, int> paired_datanode_endpoint() const;
    std::unique_ptr<coordinator_proto::coordinatorService::Stub> m_coordinator_ptr;
    std::map<std::string, std::unique_ptr<datanode_proto::datanodeService::Stub>> m_datanode_ptrs;
    std::string config_path;
    std::string proxy_ip_port;
    std::string m_ip;
    int m_port;
    int m_self_node_id = -1;
    std::string m_paired_datanode;
    std::map<int, std::string> m_node_owner_proxies;
    std::map<std::string, std::string> m_datanode_owner_proxies;
    std::map<std::string, std::string> m_proxy_owned_datanodes;
    std::unique_ptr<asio::ip::tcp::acceptor> m_peer_acceptor;
    std::thread m_peer_thread;
    std::mutex m_datanode_io_mutex;
    asio::io_context io_context;
    asio::ip::tcp::acceptor acceptor;
    sem_t sem;
    std::string m_coordinator_address;
  };

  class Proxy
  {
  public:
    Proxy(std::string proxy_ip_port, std::string config_path, std::string coordinator_address) : proxy_ip_port(proxy_ip_port), m_proxyImpl_ptr(proxy_ip_port, config_path, coordinator_address)
    {
    }
    Proxy(std::string proxy_ip_port, std::string config_path, std::string coordinator_address, std::string sys_config_path) : proxy_ip_port(proxy_ip_port), m_proxyImpl_ptr(proxy_ip_port, config_path, coordinator_address)
    {
      m_proxyImpl_ptr.m_sys_config = ECProject::Config::getInstance(sys_config_path);
      m_proxyImpl_ptr.m_toolbox = ECProject::ToolBox::getInstance();
    }
    void Run()
    {
      grpc::EnableDefaultHealthCheckService(true);
      grpc::reflection::InitProtoReflectionServerBuilderPlugin();
      grpc::ServerBuilder builder;
      std::cout << "proxy_ip_port:" << proxy_ip_port << std::endl;
      builder.AddListeningPort(proxy_ip_port, grpc::InsecureServerCredentials());
      builder.RegisterService(&m_proxyImpl_ptr);
      std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
      server->Wait();
    }

  private:
    std::string proxy_ip_port;
    ECProject::ProxyImpl m_proxyImpl_ptr;
  };
} // namespace ECProject
#endif