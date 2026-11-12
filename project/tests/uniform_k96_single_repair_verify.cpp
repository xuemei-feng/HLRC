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
std::vector<int> flatten(const std::vector<std::pair<int,std::vector<int>>> &plan){std::vector<int> result;for(const auto &entry:plan)result.insert(result.end(),entry.second.begin(),entry.second.end());return result;}
} // namespace

int main(){try{
    constexpr int k=96,r=3,z=4,block_size=4096,l0_id=99,l1_id=100;
    const auto cases=ECProject::RepairBenchmark::make_uniform_k96_single_block_cases();
    require(cases.size()==2,"unexpected UniformLRC k96 single case count");
    require(cases[0].failed_name=="L0"&&cases[0].failed_block_id==l0_id&&cases[0].source_block_ids==range(0,24),"L0 must read D0-D23");
    require(cases[1].failed_name=="L1"&&cases[1].failed_block_id==l1_id&&cases[1].source_block_ids==range(24,49),"L1 must read D24-D48");

    std::vector<std::vector<unsigned char>> blocks(k+r+z,std::vector<unsigned char>(block_size));
    std::mt19937 generator(0x554e4936u);std::uniform_int_distribution<int> distribution(0,255);
    for(int id=0;id<k;++id)for(auto &value:blocks[id])value=static_cast<unsigned char>(distribution(generator));
    std::vector<unsigned char*> data(k),parity(r+z);for(int id=0;id<k;++id)data[id]=blocks[id].data();for(int id=0;id<r+z;++id)parity[id]=blocks[k+id].data();
    ECProject::encode_uniform_lrc(k,r,z,data.data(),parity.data(),block_size);
    const auto group_by_block=ECProject::get_uniform_lrc_block_id_to_group_id(k,r,z);

    for(const auto &test_case:cases){
        const auto recovery_plan=ECProject::get_recovery_group_and_block_ids("UniformLRC",k,r,z,test_case.failed_block_id);
        const auto sources=flatten(recovery_plan);require(sources==test_case.source_block_ids,test_case.failed_name+" production read set mismatch");
        for(const auto &entry:recovery_plan)for(int source:entry.second)require(group_by_block.at(source)==entry.first,test_case.failed_name+" source assigned to wrong group");
        std::vector<unsigned char> coefficients;require(ECProject::get_local_fill_plan(k,r,z,"UniformLRC",{test_case.failed_block_id},sources,coefficients),test_case.failed_name+" coefficient solve failed");
        require(coefficients.size()==sources.size(),test_case.failed_name+" coefficient count mismatch");
        std::vector<unsigned char> direct(block_size);for(size_t i=0;i<sources.size();++i)for(int byte=0;byte<block_size;++byte)direct[byte]^=ECProject::gf_mul(coefficients[i],blocks[sources[i]][byte]);
        require(direct==blocks[test_case.failed_block_id],test_case.failed_name+" direct recovery mismatch");
        std::vector<unsigned char*> source_ptrs;for(int source:sources)source_ptrs.push_back(blocks[source].data());
        std::vector<unsigned char> decoded(block_size);ECProject::decode_uniform_lrc(k,r,z,static_cast<int>(sources.size()),&sources,source_ptrs.data(),decoded.data(),block_size,test_case.failed_block_id);
        require(decoded==blocks[test_case.failed_block_id],test_case.failed_name+" decode mismatch");
    }
    std::cout<<"UniformLRC k96 L0/L1 single repair verification passed"<<std::endl;return 0;
}catch(const std::exception &error){std::cerr<<"UniformLRC k96 single repair verification failed: "<<error.what()<<std::endl;return 1;}}
