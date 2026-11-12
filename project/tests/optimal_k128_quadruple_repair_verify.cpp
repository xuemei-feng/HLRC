#include "encoder.h"
#include "repair_benchmark_plan.h"

#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace {
using ECProject::RepairBenchmark::QuadrupleRepairCase;
using ECProject::RepairBenchmark::RepairTask;
void require(bool condition,const std::string &message){if(!condition)throw std::runtime_error(message);}
std::vector<int> range(int first,int last){return ECProject::RepairBenchmark::inclusive_range(first,last);}
std::vector<int> globals(std::vector<int> sources){sources.insert(sources.end(),{128,129,130});return sources;}
void check_task(const RepairTask &task,const std::vector<int> &failed,const std::vector<int> &sources,const std::string &name){require(task.failed_block_ids==failed,name+" failed IDs");require(task.source_block_ids==sources,name+" sources");}
void verify_cases(const std::vector<QuadrupleRepairCase> &cases){
    require(cases.size()==3,"case count");
    const std::vector<std::string> names={"L0+L1+L2+L3","L1+L2+L3+L4"};
    const std::vector<std::vector<int>> ids={{131,132,133,134},{132,133,134,135}};
    const std::vector<std::vector<std::vector<int>>> sources={
        {globals(range(0,24)),globals(range(25,49)),globals(range(50,75)),globals(range(76,101))},
        {globals(range(25,49)),globals(range(50,75)),globals(range(76,101)),globals(range(102,127))}};
    for(size_t c=0;c<2;++c){require(cases[c].failed_name==names[c]&&cases[c].tasks.size()==4,names[c]);for(size_t t=0;t<4;++t)check_task(cases[c].tasks[t],{ids[c][t]},sources[c][t],names[c]);}
    require(cases[2].failed_name=="G0+G1+G2+L0"&&cases[2].tasks.size()==1,"global case");
    check_task(cases[2].tasks[0],{128,129,130,131},range(0,127),"global case");
    require(cases[2].tasks[0].failed_block_ids.front()==128,"G0 owner");
}
} // namespace

int main(){try{
    constexpr int k=128,r=3,z=5,block_size=4096;
    const auto cases=ECProject::RepairBenchmark::make_optimal_quadruple_block_cases(k);verify_cases(cases);
    std::vector<std::vector<unsigned char>> blocks(k+r+z,std::vector<unsigned char>(block_size));
    std::mt19937 generator(0x4f503854u);std::uniform_int_distribution<int> distribution(0,255);
    for(int id=0;id<k;++id)for(auto &value:blocks[id])value=static_cast<unsigned char>(distribution(generator));
    std::vector<unsigned char*> data(k),parity(r+z);for(int id=0;id<k;++id)data[id]=blocks[id].data();for(int id=0;id<r+z;++id)parity[id]=blocks[k+id].data();
    ECProject::encode_optimal_lrc(k,r,z,data.data(),parity.data(),block_size);
    for(const auto &test_case:cases){
        std::unordered_set<int> failed;for(const auto &task:test_case.tasks)failed.insert(task.failed_block_ids.begin(),task.failed_block_ids.end());require(failed.size()==4,test_case.failed_name+" failures");
        for(const auto &task:test_case.tasks){
            for(int source:task.source_block_ids)require(!failed.count(source),test_case.failed_name+" reads failure");
            std::vector<unsigned char> coefficients;require(ECProject::get_local_fill_plan(k,r,z,"OptimalLRC",task.failed_block_ids,task.source_block_ids,coefficients),test_case.failed_name+" solve");
            const size_t rows=task.failed_block_ids.size(),columns=task.source_block_ids.size();require(coefficients.size()==rows*columns,"matrix size");
            std::vector<std::vector<unsigned char>> recovered(rows,std::vector<unsigned char>(block_size));
            for(size_t row=0;row<rows;++row){for(size_t column=0;column<columns;++column)for(int byte=0;byte<block_size;++byte)recovered[row][byte]^=ECProject::gf_mul(coefficients[row*columns+column],blocks[task.source_block_ids[column]][byte]);require(recovered[row]==blocks[task.failed_block_ids[row]],test_case.failed_name+" bytes");}
            if(test_case.failed_name=="G0+G1+G2+L0"){require(recovered[0]==blocks[128],"G0 local");require(recovered[1]==blocks[129],"G1 forward");require(recovered[2]==blocks[130],"G2 forward");require(recovered[3]==blocks[131],"L0 forward");}
        }
    }
    std::cout<<"OptimalLRC k128 quadruple repair verification passed"<<std::endl;return 0;
}catch(const std::exception &error){std::cerr<<"OptimalLRC k128 quadruple repair verification failed: "<<error.what()<<std::endl;return 1;}}
