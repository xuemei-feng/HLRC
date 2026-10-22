#include "client.h"
#include "toolbox.h"
#include <fstream>
#include <sys/time.h>
#include <unistd.h>
#include <sys/stat.h>
#include "config.h"
#include <iomanip>
#include <iostream>
#include <chrono>
#include <algorithm>
#include <random>
#include "encoder.h"
#include <unordered_map>
#include <unordered_set>
#include <stdexcept>



int main(int argc, char **argv)
{
    char buff[256];
    getcwd(buff, 256);
    std::string exe_path(argv[0]);
    std::string exe_dir;
    size_t last_slash = exe_path.rfind('/');
    if (last_slash != std::string::npos) {
        exe_dir = exe_path.substr(0, last_slash);
        if (exe_dir.empty() || exe_dir[0] != '/')
            exe_dir = std::string(buff) + "/" + exe_dir;
    } else {
        exe_dir = buff;
    }
    std::string sys_config_path = exe_dir + "/../../config/parameterConfiguration.xml";
    //std::string sys_config_path = "/home/GuanTian/lql/UniLRC/project/config/parameterConfiguration.xml";
    std::cout << "Current working directory: " << sys_config_path << std::endl;

    const ECProject::Config *config = ECProject::Config::getInstance(sys_config_path);
    std::string client_ip = config->ClientIP;
    int client_port = 44444;
    ECProject::Client client(client_ip, client_port, config->CoordinatorIP + ":" + std::to_string(config->CoordinatorPort), sys_config_path);
    std::cout << client.sayHelloToCoordinatorByGrpc("Client ID: " + client_ip + ":" + std::to_string(client_port)) << std::endl;

    std::vector<int> parameters = client.get_parameters();
    int k = parameters[0];
    int r = parameters[1];
    int z = parameters[2];
    std::string code_type;
    if(parameters[4] == 0){
        code_type = "AzureLRC";
    }
    else if(parameters[4] == 1){
        code_type = "OptimalLRC";
    }
    else if(parameters[4] == 2){
        code_type = "UniformLRC";
    }
    else if(parameters[4] == 3){
        code_type = "UniLRC";
    }
    else if(parameters[4] == 4){
        code_type = "LotusLRC";
    }
    else{
        std::cout << "Code type error" << std::endl;
        return -1;
    }
    double block_size = static_cast<double> (parameters[3]) / 1024 / 1024; //MB
    int n = k + r + z;
    
    int stripe_num = 5;

    size_t total_write_size = static_cast<size_t>(stripe_num * block_size * k); // MB
    std::cout << "Starting set stripe operation" << std::endl;
    std::chrono::high_resolution_clock::time_point set_start = std::chrono::high_resolution_clock::now();
    for(int i = 0; i < stripe_num; i++){
        client.set();
    }
    std::chrono::high_resolution_clock::time_point set_end = std::chrono::high_resolution_clock::now();
    std::cout << "Set stripe operation finished" << std::endl;
    std::cout << "Conducting experiments, please wait..." << std::endl;
    std::chrono::duration<double> set_time = std::chrono::duration_cast<std::chrono::duration<double>>(set_end - set_start);
    std::cout << "write throughput: " << (static_cast<double>(total_write_size) / set_time.count()) << " MB/s" << std::endl;
    std::mt19937 rng(std::random_device{}());
    sleep(5);

    std::uniform_int_distribution<int> dist_500(0, k*stripe_num - 500);
    std::uniform_real_distribution<double> dist_double(0.0, 1.0);

 
    // 读性能测试：Normal read -> Degraded read -> Maintenance-robust read（共用上方预写的 stripe）
    std::cout << "Normal read test start" << std::endl;
    std::vector<std::chrono::duration<double>> read_time_spans;
    for(int i = 0; i < 5; i++){
        size_t data_size;
        int id = i;
        std::string key = std::to_string(id);
        std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();
        std::shared_ptr<char[]> data = client.get(key, data_size);
        if(!data){
            std::cout << "Get operation failed" << std::endl;
            continue;
        }
        std::chrono::high_resolution_clock::time_point t2 = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);
        read_time_spans.push_back(time_span);
        //std::cout << "get time: " << time_span.count() << std::endl;
    }
    std::chrono::duration<double> read_total_time_span = std::accumulate(read_time_spans.begin(), read_time_spans.end(), std::chrono::duration<double>(0));
    std::cout << "Total time: " << read_total_time_span.count() << std::endl;
    std::cout << "Average time: " << read_total_time_span.count() / read_time_spans.size() << std::endl;
    std::cout << "Throughput (stripes/s): " << read_time_spans.size() / read_total_time_span.count() << std::endl;
    std::cout << "Speed: " << static_cast<size_t>(block_size) * k / (read_total_time_span.count() / read_time_spans.size()) << " MB/s" << std::endl;
    std::chrono::duration<double> read_max_time_span = *std::max_element(read_time_spans.begin(), read_time_spans.end());
    std::chrono::duration<double> read_min_time_span = *std::min_element(read_time_spans.begin(), read_time_spans.end());
    std::cout << "Max speed: " << static_cast<size_t>(block_size) * k / read_min_time_span.count() << " MB/s" << std::endl;
    std::cout << "Min speed: " << static_cast<size_t>(block_size) * k / read_max_time_span.count() << " MB/s" << std::endl;
    std::cout << "Normal read test end" << std::endl;
    std::cout << std::endl;



    //for degraded read test 
    std::vector<std::chrono::duration<double>> degraded_read_time_spans;
    std::cout << "Degraded read test start" << std::endl;
    for(int i = 0; i < k; i++){
        size_t data_size;
        int id = i;
        std::string key = std::to_string(id);
        std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();
        std::shared_ptr<char[]> data = client.get_degraded_read_block(0, i);
        if(!data){
            std::cout << "Degraded read operation failed" << std::endl;
            continue;
        }
        std::chrono::high_resolution_clock::time_point t2 = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);
        degraded_read_time_spans.push_back(time_span);
        //std::cout << "get time: " << time_span.count() << std::endl;
    }
    std::chrono::duration<double> degraded_read_total_time_span = std::accumulate(degraded_read_time_spans.begin(), degraded_read_time_spans.end(), std::chrono::duration<double>(0));
    std::cout << "Average time: " << degraded_read_total_time_span.count() / degraded_read_time_spans.size() << std::endl;
    std::chrono::duration<double> degraded_read_max_time_span = *std::max_element(degraded_read_time_spans.begin(), degraded_read_time_spans.end());
    std::chrono::duration<double> degraded_read_min_time_span = *std::min_element(degraded_read_time_spans.begin(), degraded_read_time_spans.end());
    std::cout << "Max time: "<< degraded_read_max_time_span.count() << std::endl;
    std::cout << "Min time: "<< degraded_read_min_time_span.count() << std::endl;
    std::cout << "Throughput (blocks/s): " << degraded_read_time_spans.size() / degraded_read_total_time_span.count() << std::endl;
    std::cout << "Speed: " << static_cast<size_t>(block_size) / (degraded_read_total_time_span.count() / degraded_read_time_spans.size()) << " MB/s" << std::endl;
    std::cout << "Max speed: " << static_cast<size_t>(block_size) / degraded_read_min_time_span.count() << " MB/s" << std::endl;
    std::cout << "Min speed: " << static_cast<size_t>(block_size) / degraded_read_max_time_span.count() << " MB/s" << std::endl;
    std::cout << "Degraded read test end" << std::endl;
    std::cout << std::endl;


/*
    // Maintenance-robust normal read（与 Normal/Degraded read 共用预写后的 stripe 0）
    {
        const int test_stripe_id = 0;
        const int failed_node_id = 0;
        std::vector<int> first_rack_failed;
        try
        {
            first_rack_failed = blocks_on_node(
                code_type, k, r, z, n, test_stripe_id, failed_node_id, config->NodeNum);
        }
        catch (const std::exception &e)
        {
            std::cout << "Layout lookup failed: " << e.what() << std::endl;
            return -1;
        }
        std::vector<int> failed_data;
        for (int bid : first_rack_failed)
            if (bid < k)
                failed_data.push_back(bid);

        if (failed_data.empty())
        {
            std::cout << "No data blocks on node " << failed_node_id << " for stripe "
                      << test_stripe_id << ", skip maintenance-robust read test" << std::endl;
        }
        else
        {
            auto parity_survives = [&](int pid) {
                int gid = block_id_to_group(code_type, k, r, z, pid);
                int cl = (test_stripe_id + gid) % config->NodeNum;
                return cl != failed_node_id;
            };
            int g_last = ECProject::get_block_id_to_local_group_id(code_type, k, r, z, failed_data.back());
            std::vector<int> grp_failed_data;
            for (int bid : failed_data)
                if (ECProject::get_block_id_to_local_group_id(code_type, k, r, z, bid) == g_last)
                    grp_failed_data.push_back(bid);
            int surviving_parities = 0;
            for (int pid : local_parity_ids_of_group(code_type, k, r, z, g_last))
                if (parity_survives(pid))
                    surviving_parities++;
            int leftover_cnt = std::min<int>(surviving_parities, static_cast<int>(grp_failed_data.size()));
            std::unordered_set<int> leftover_set(
                grp_failed_data.end() - leftover_cnt, grp_failed_data.end());
            std::vector<int> global_batch;
            std::vector<int> local_fill;
            for (int bid : failed_data)
            {
                if (leftover_set.count(bid))
                    local_fill.push_back(bid);
                else
                    global_batch.push_back(bid);
            }

            std::cout << "Maintenance-robust normal read test start (one rack, node "
                      << failed_node_id << ", stripe " << test_stripe_id << ", code "
                      << code_type << ")" << std::endl;
            print_block_ids("  Failed blocks on rack (layout+placement):", first_rack_failed);
            print_block_ids("  Failed DATA blocks to reconstruct:", failed_data);
            print_block_ids("  Phase-1 DATA blocks via cross-group global batch:", global_batch);
            print_block_ids("  Phase-2 DATA blocks via in-memory local fill:", local_fill);

            const double recovered_mb = static_cast<double>(k) * block_size;
            std::vector<std::chrono::duration<double>> maintenance_read_time_spans;
            for (int i = 0; i < 5; i++)
            {
                size_t data_size = 0;
                std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();
                std::shared_ptr<char[]> data =
                    client.maintenance_read(test_stripe_id, failed_data, global_batch, data_size);
                std::chrono::high_resolution_clock::time_point t2 = std::chrono::high_resolution_clock::now();
                if (!data)
                {
                    std::cout << "[" << i << "th] Maintenance-robust read operation failed" << std::endl;
                    continue;
                }
                std::chrono::duration<double> time_span =
                    std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);
                maintenance_read_time_spans.push_back(time_span);
                if (time_span.count() > 0)
                    std::cout << "[" << i << "th] Maintenance-robust read throughput: "
                              << (recovered_mb / time_span.count()) << " MB/s" << std::endl;
            }
            print_throughput_summary("Maintenance-robust normal read",
                                     maintenance_read_time_spans, recovered_mb);
            std::cout << "Maintenance-robust normal read test end" << std::endl;
            std::cout << std::endl;
        }
    }
*/

    // Recovery benchmarks are intentionally kept out of main_client.
    // Use repair_benchmark so that every repair uses an explicit source-block plan.


/*
    // 写性能测试，Normal write
    // 使用说明：测写性能时，把文件前面那段“写条带”的循环（client.set() 预写）注释掉，
    // 只跑这里即可——此处会重新写入 stripe_num 条 stripe 并统计写时延 / 吞吐。
    // 每次 client.set() 写入一整条 stripe：coordinator 调度 + client 端 EC 编码 + 并行 append 到各 proxy。
    {
        const int warmup_num = 1; // 预热条数，不计入统计（预热 gRPC 建连 + 各 proxy/datanode 首次连接 + 缓存）
        const double written_mb_per_stripe = block_size * static_cast<double>(k); // 单条 stripe 的有效数据量(MB)
        std::vector<std::chrono::duration<double>> write_time_spans;
        write_time_spans.reserve(stripe_num);

        std::cout << "Normal write test start" << std::endl;
        for (int i = 0; i < warmup_num + stripe_num; i++)
        {
            std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();
            bool ok = client.set();
            std::chrono::high_resolution_clock::time_point t2 = std::chrono::high_resolution_clock::now();
            if (!ok)
            {
                std::cout << "[" << i << "th] Normal write failed" << std::endl;
                continue;
            }
            if (i < warmup_num)
            {
                std::cout << "[warmup " << i << "] Normal write done (excluded from stats)" << std::endl;
                continue;
            }
            std::chrono::duration<double> time_span =
                std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);
            write_time_spans.push_back(time_span);
            if (time_span.count() > 0)
                std::cout << "[" << (i - warmup_num) << "th] Normal write throughput: "
                          << (written_mb_per_stripe / time_span.count()) << " MB/s" << std::endl;
        }

        if (write_time_spans.empty())
        {
            std::cout << "Normal write test: no successful samples" << std::endl;
        }
        else
        {
            std::chrono::duration<double> write_total_time_span =
                std::accumulate(write_time_spans.begin(), write_time_spans.end(),
                                std::chrono::duration<double>(0));
            std::chrono::duration<double> write_max_time_span =
                *std::max_element(write_time_spans.begin(), write_time_spans.end());
            std::chrono::duration<double> write_min_time_span =
                *std::min_element(write_time_spans.begin(), write_time_spans.end());
            const size_t sample_num = write_time_spans.size();

            std::cout << "Total write time: " << write_total_time_span.count() << std::endl;
            std::cout << "Average write time: "
                      << write_total_time_span.count() / static_cast<double>(sample_num) << std::endl;
            std::cout << "Max write time: " << write_max_time_span.count() << std::endl;
            std::cout << "Min write time: " << write_min_time_span.count() << std::endl;
            std::cout << "Throughput: "
                      << static_cast<double>(sample_num) / write_total_time_span.count()
                      << " stripes/s" << std::endl;
            // 聚合写带宽：所有成功 stripe 的有效数据量 / 总耗时
            std::cout << "Aggregate speed: "
                      << (static_cast<double>(sample_num) * written_mb_per_stripe /
                          write_total_time_span.count())
                      << " MB/s" << std::endl;
            // 单条 stripe 视角的 平均/最大/最小 写带宽
            print_throughput_summary("Normal write", write_time_spans, written_mb_per_stripe);
        }
        std::cout << "Normal write test end" << std::endl;
        std::cout << std::endl;
    }
*/

    return 0;
}
