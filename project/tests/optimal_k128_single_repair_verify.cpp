#include "encoder.h"
#include "repair_benchmark_plan.h"

#include <iostream>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void require(bool condition,const std::string &message){if(!condition)throw std::runtime_error(message);}
std::vector<int> range(int begin,int end){std::vector<int> result(static_cast<size_t>(end-begin));std::iota(result.begin(),result.end(),begin);return result;}
std::vector<int> with_globals(std::vector<int> sources){sources.insert(sources.end(),{128,129,130});return sources;}
std::vector<int> flatten(const std::vector<std::pair<int,std::vector<int>>> &plan){std::vector<int> result;for(const auto &entry:plan)result.insert(result.end(),entry.second.begin(),entry.second.end());return result;}
} // namespace

int main(){
    try{
        constexpr int k=128,r=3,z=5,block_size=4096,l0_id=131,l2_id=133;
        const auto cases=ECProject::RepairBenchmark::make_optimal_uneven_single_block_cases(k,r,z);
        require(cases.size()==6,"unexpected OptimalLRC k128 single case count");
        require(cases[0].failed_name=="L0"&&cases[0].failed_block_id==l0_id&&
                cases[0].source_block_ids==with_globals(range(0,25)),
                "L0 must read D0-D24 and G0-G2");
        require(cases[2].failed_name=="L2"&&cases[2].failed_block_id==l2_id&&
                cases[2].source_block_ids==with_globals(range(50,76)),
                "L2 must read D50-D75 and G0-G2");
        const std::vector<ECProject::RepairBenchmark::RepairCase> selected={cases[0],cases[2]};

        std::vector<std::vector<unsigned char>> blocks(k+r+z,std::vector<unsigned char>(block_size));
        std::mt19937 generator(0x4f503138u);std::uniform_int_distribution<int> distribution(0,255);
        for(int id=0;id<k;++id)for(auto &value:blocks[id])value=static_cast<unsigned char>(distribution(generator));
        std::vector<unsigned char*> data(k),parity(r+z);
        for(int id=0;id<k;++id)data[id]=blocks[id].data();
        for(int id=0;id<r+z;++id)parity[id]=blocks[k+id].data();
        ECProject::encode_optimal_lrc(k,r,z,data.data(),parity.data(),block_size);
        const auto group_by_block=ECProject::get_optimal_lrc_block_id_to_group_id(k,r,z);

        for(const auto &test_case:selected){
            const auto plan=ECProject::get_recovery_group_and_block_ids("OptimalLRC",k,r,z,test_case.failed_block_id);
            const auto sources=flatten(plan);
            require(sources==test_case.source_block_ids,test_case.failed_name+" production read set mismatch");
            for(const auto &entry:plan)for(int source:entry.second)
                require(group_by_block.at(source)==entry.first,test_case.failed_name+" source assigned to wrong group");
            std::vector<unsigned char> coefficients;
            require(ECProject::get_local_fill_plan(k,r,z,"OptimalLRC",{test_case.failed_block_id},sources,coefficients),test_case.failed_name+" coefficient solve failed");
            require(coefficients.size()==sources.size(),"coefficient count mismatch");
            std::vector<unsigned char> direct(block_size,0);
            for(size_t i=0;i<sources.size();++i)for(int byte=0;byte<block_size;++byte)
                direct[byte]^=ECProject::gf_mul(coefficients[i],blocks[sources[i]][byte]);
            require(direct==blocks[test_case.failed_block_id],test_case.failed_name+" direct GF recovery mismatch");
            std::vector<unsigned char*> source_ptrs;for(int source:sources)source_ptrs.push_back(blocks[source].data());
            std::vector<unsigned char> decoded(block_size,0);
            ECProject::decode_optimal_lrc(k,r,z,static_cast<int>(sources.size()),&sources,source_ptrs.data(),decoded.data(),block_size,test_case.failed_block_id);
            require(decoded==blocks[test_case.failed_block_id],test_case.failed_name+" decode mismatch");
        }
        std::cout<<"OptimalLRC k128 L0/L2 single repair verification passed"<<std::endl;return 0;
    }catch(const std::exception &error){std::cerr<<"OptimalLRC k128 single repair verification failed: "<<error.what()<<std::endl;return 1;}
}
