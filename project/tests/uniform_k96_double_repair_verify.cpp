#include "encoder.h"
#include "repair_benchmark_plan.h"

#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace {
using ECProject::RepairBenchmark::DoubleRepairCase;
using ECProject::RepairBenchmark::RepairTask;
void require(bool condition,const std::string &message){if(!condition)throw std::runtime_error(message);}
std::vector<int> range(int first,int last){return ECProject::RepairBenchmark::inclusive_range(first,last);}
void require_task(const RepairTask &task,const std::vector<int> &failed,const std::vector<int> &sources,const std::string &name){require(task.failed_block_ids==failed,name+" failed IDs");require(task.source_block_ids==sources,name+" sources");}
void verify_cases(const std::vector<DoubleRepairCase> &cases){
    require(cases.size()==3,"case count");
    require(cases[0].failed_name=="L0+L1"&&cases[0].tasks.size()==2,"L0+L1");require_task(cases[0].tasks[0],{99},range(0,23),"L0");require_task(cases[0].tasks[1],{100},range(24,48),"L1");
    require(cases[1].failed_name=="L1+L2"&&cases[1].tasks.size()==2,"L1+L2");require_task(cases[1].tasks[0],{100},range(24,48),"L1");require_task(cases[1].tasks[1],{101},range(49,73),"L2");
    require(cases[2].failed_name=="G0+G1"&&cases[2].tasks.size()==1,"G0+G1");require_task(cases[2].tasks[0],{96,97},range(0,95),"G0+G1");require(cases[2].tasks[0].failed_block_ids.front()==96,"G0 owner");
}
} // namespace

int main(){try{
    constexpr int k=96,r=3,z=4,block_size=4096;const auto cases=ECProject::RepairBenchmark::make_uniform_k96_double_block_cases();verify_cases(cases);
    std::vector<std::vector<unsigned char>> blocks(k+r+z,std::vector<unsigned char>(block_size));std::mt19937 generator(0x554e4436u);std::uniform_int_distribution<int> distribution(0,255);
    for(int id=0;id<k;++id)for(auto &value:blocks[id])value=static_cast<unsigned char>(distribution(generator));
    std::vector<unsigned char*> data(k),parity(r+z);for(int id=0;id<k;++id)data[id]=blocks[id].data();for(int id=0;id<r+z;++id)parity[id]=blocks[k+id].data();ECProject::encode_uniform_lrc(k,r,z,data.data(),parity.data(),block_size);
    for(const auto &test_case:cases){
        std::unordered_set<int> failed;for(const auto &task:test_case.tasks)failed.insert(task.failed_block_ids.begin(),task.failed_block_ids.end());require(failed.size()==2,test_case.failed_name+" failures");
        for(const auto &task:test_case.tasks){
            for(int source:task.source_block_ids)require(!failed.count(source),test_case.failed_name+" reads failure");
            std::vector<unsigned char> coefficients;require(ECProject::get_local_fill_plan(k,r,z,"UniformLRC",task.failed_block_ids,task.source_block_ids,coefficients),test_case.failed_name+" solve");
            const size_t rows=task.failed_block_ids.size(),columns=task.source_block_ids.size();require(coefficients.size()==rows*columns,"matrix size");
            std::vector<std::vector<unsigned char>> recovered(rows,std::vector<unsigned char>(block_size));
            for(size_t row=0;row<rows;++row){for(size_t column=0;column<columns;++column)for(int byte=0;byte<block_size;++byte)recovered[row][byte]^=ECProject::gf_mul(coefficients[row*columns+column],blocks[task.source_block_ids[column]][byte]);require(recovered[row]==blocks[task.failed_block_ids[row]],test_case.failed_name+" bytes");}
            if(test_case.failed_name=="G0+G1"){require(recovered[0]==blocks[96],"G0 local");require(recovered[1]==blocks[97],"G1 forward");}
        }
    }
    std::cout<<"UniformLRC k96 double repair verification passed"<<std::endl;return 0;
}catch(const std::exception &error){std::cerr<<"UniformLRC k96 double repair verification failed: "<<error.what()<<std::endl;return 1;}}
