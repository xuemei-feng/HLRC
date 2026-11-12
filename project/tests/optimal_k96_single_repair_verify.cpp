#include "encoder.h"
#include "repair_benchmark_plan.h"

#include <iostream>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void require(bool condition,const std::string &message)
{
    if(!condition) throw std::runtime_error(message);
}
std::vector<int> range(int begin,int end)
{
    std::vector<int> result(static_cast<size_t>(end-begin));
    std::iota(result.begin(),result.end(),begin);
    return result;
}
std::vector<int> flatten(const std::vector<std::pair<int,std::vector<int>>> &plan)
{
    std::vector<int> result;
    for(const auto &entry:plan) result.insert(result.end(),entry.second.begin(),entry.second.end());
    return result;
}
} // namespace

int main()
{
    try{
        constexpr int k=96,r=3,z=4,block_size=4096,l0_id=99;
        auto expected_sources=range(0,24);
        const auto globals=range(96,99);
        expected_sources.insert(expected_sources.end(),globals.begin(),globals.end());

        const auto cases=ECProject::RepairBenchmark::make_optimal_uneven_single_block_cases(k,r,z);
        require(cases.size()==5,"unexpected OptimalLRC k96 single case count");
        require(cases[0].failed_name=="L0"&&cases[0].failed_block_id==l0_id,
                "unexpected L0 case identity");
        require(cases[0].source_block_ids==expected_sources,
                "L0 must read D0-D23 and G0-G2");

        const auto plan=ECProject::get_recovery_group_and_block_ids("OptimalLRC",k,r,z,l0_id);
        const auto sources=flatten(plan);
        require(sources==expected_sources,"production L0 recovery read set mismatch");
        const auto group_by_block=ECProject::get_optimal_lrc_block_id_to_group_id(k,r,z);
        for(const auto &entry:plan)
            for(int source:entry.second)
                require(group_by_block.at(source)==entry.first,
                        "source assigned to wrong physical group");

        std::vector<std::vector<unsigned char>> blocks(k+r+z,
            std::vector<unsigned char>(block_size));
        std::mt19937 generator(0x4f503936u);
        std::uniform_int_distribution<int> distribution(0,255);
        for(int id=0;id<k;++id)
            for(auto &value:blocks[id]) value=static_cast<unsigned char>(distribution(generator));
        std::vector<unsigned char*> data(k),parity(r+z);
        for(int id=0;id<k;++id)data[id]=blocks[id].data();
        for(int id=0;id<r+z;++id)parity[id]=blocks[k+id].data();
        ECProject::encode_optimal_lrc(k,r,z,data.data(),parity.data(),block_size);

        std::vector<unsigned char> coefficients;
        require(ECProject::get_local_fill_plan(k,r,z,"OptimalLRC",{l0_id},sources,coefficients),
                "L0 coefficient solve failed");
        require(coefficients.size()==sources.size(),"L0 coefficient count mismatch");
        std::vector<unsigned char> direct(block_size,0);
        for(size_t i=0;i<sources.size();++i)
            for(int byte=0;byte<block_size;++byte)
                direct[byte]^=ECProject::gf_mul(coefficients[i],blocks[sources[i]][byte]);
        require(direct==blocks[l0_id],"L0 direct GF recovery mismatch");

        std::vector<unsigned char*> source_ptrs;
        for(int source:sources)source_ptrs.push_back(blocks[source].data());
        std::vector<unsigned char> decoded(block_size,0);
        ECProject::decode_optimal_lrc(k,r,z,static_cast<int>(sources.size()),&sources,
                                      source_ptrs.data(),decoded.data(),block_size,l0_id);
        require(decoded==blocks[l0_id],"decode_optimal_lrc mismatch");

        std::cout<<"OptimalLRC k96 L0 single repair verification passed"<<std::endl;
        return 0;
    }catch(const std::exception &error){
        std::cerr<<"OptimalLRC k96 L0 single repair verification failed: "<<error.what()<<std::endl;
        return 1;
    }
}
