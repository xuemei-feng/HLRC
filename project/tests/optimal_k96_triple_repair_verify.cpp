#include "encoder.h"
#include "repair_benchmark_plan.h"

#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace {
using ECProject::RepairBenchmark::RepairTask;
using ECProject::RepairBenchmark::TripleRepairCase;
void require(bool condition,const std::string &message){if(!condition)throw std::runtime_error(message);}
std::vector<int> range(int first,int last){return ECProject::RepairBenchmark::inclusive_range(first,last);}
std::vector<int> with_globals(std::vector<int> sources){sources.insert(sources.end(),{96,97,98});return sources;}
void require_task(const RepairTask &task,const std::vector<int> &failed,const std::vector<int> &sources,const std::string &name){
    require(task.failed_block_ids==failed,name+" failed block order mismatch");
    require(task.source_block_ids==sources,name+" source block order mismatch");
}
void verify_registered_cases(const std::vector<TripleRepairCase> &cases){
    require(cases.size()==2,"unexpected OptimalLRC k96 triple case count");
    require(cases[0].failed_name=="L0+L1+L2"&&cases[0].tasks.size()==3,"missing L0+L1+L2 case");
    require_task(cases[0].tasks[0],{99},with_globals(range(0,23)),"L0");
    require_task(cases[0].tasks[1],{100},with_globals(range(24,47)),"L1");
    require_task(cases[0].tasks[2],{101},with_globals(range(48,71)),"L2");
    require(cases[1].failed_name=="G0+G1+G2"&&cases[1].tasks.size()==1,"missing G0+G1+G2 shared case");
    require_task(cases[1].tasks[0],{96,97,98},range(0,95),"G0+G1+G2");
    require(cases[1].tasks[0].failed_block_ids.front()==96,"G0 must be the shared task owner proxy");
}
} // namespace

int main(){
    try{
        constexpr int k=96,r=3,z=4,block_size=4096;
        const auto cases=ECProject::RepairBenchmark::make_optimal_triple_block_cases(k);
        verify_registered_cases(cases);
        std::vector<std::vector<unsigned char>> blocks(k+r+z,std::vector<unsigned char>(block_size));
        std::mt19937 generator(0x4f503954u);std::uniform_int_distribution<int> distribution(0,255);
        for(int id=0;id<k;++id)for(auto &value:blocks[id])value=static_cast<unsigned char>(distribution(generator));
        std::vector<unsigned char*> data(k),parity(r+z);
        for(int id=0;id<k;++id)data[id]=blocks[id].data();
        for(int id=0;id<r+z;++id)parity[id]=blocks[k+id].data();
        ECProject::encode_optimal_lrc(k,r,z,data.data(),parity.data(),block_size);
        for(const auto &test_case:cases){
            std::unordered_set<int> all_failed;
            for(const auto &task:test_case.tasks)all_failed.insert(task.failed_block_ids.begin(),task.failed_block_ids.end());
            require(all_failed.size()==3,test_case.failed_name+" must contain three failures");
            for(const auto &task:test_case.tasks){
                for(int source:task.source_block_ids)require(all_failed.count(source)==0,test_case.failed_name+" reads a failed block");
                std::vector<unsigned char> coefficients;
                require(ECProject::get_local_fill_plan(k,r,z,"OptimalLRC",task.failed_block_ids,task.source_block_ids,coefficients),test_case.failed_name+" coefficient solve failed");
                const size_t rows=task.failed_block_ids.size(),columns=task.source_block_ids.size();
                require(coefficients.size()==rows*columns,"coefficient matrix size mismatch");
                std::vector<std::vector<unsigned char>> recovered(rows,std::vector<unsigned char>(block_size,0));
                for(size_t row=0;row<rows;++row){
                    for(size_t col=0;col<columns;++col)for(int byte=0;byte<block_size;++byte)
                        recovered[row][byte]^=ECProject::gf_mul(coefficients[row*columns+col],blocks[task.source_block_ids[col]][byte]);
                    require(recovered[row]==blocks[task.failed_block_ids[row]],test_case.failed_name+" recovered bytes mismatch");
                }
                if(test_case.failed_name=="G0+G1+G2"){
                    require(rows==3,"G0 owner must produce three output rows");
                    require(recovered[0]==blocks[96],"G0 local write value mismatch");
                    require(recovered[1]==blocks[97],"G1 forwarded value mismatch");
                    require(recovered[2]==blocks[98],"G2 forwarded value mismatch");
                }
            }
        }
        std::cout<<"OptimalLRC k96 L0+L1+L2/G0+G1+G2 triple repair verification passed"<<std::endl;return 0;
    }catch(const std::exception &error){std::cerr<<"OptimalLRC k96 triple repair verification failed: "<<error.what()<<std::endl;return 1;}
}
