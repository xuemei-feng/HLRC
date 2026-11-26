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
void require(bool condition, const std::string &message) { if (!condition) throw std::runtime_error(message); }
std::vector<int> range(int first, int last) { return ECProject::RepairBenchmark::inclusive_range(first,last); }
void require_task(const RepairTask &actual,std::vector<int> failed,std::vector<int> sources){require(actual.failed_block_ids==failed,"failed order mismatch");require(actual.source_block_ids==sources,"source range mismatch");require(actual.failed_block_ids.front()==failed.front(),"owner mismatch");}

void verify_cases(const std::vector<TripleRepairCase> &cases)
{
    const std::vector<std::string> names={"L0+L1+L2","L0+L1+L4","L4+L5+L6","L0+L2+L4","L2+L4+L6","L4+L6+L8","G0+G1+G2"};
    const std::vector<size_t> counts={2,2,2,3,3,3,1};
    require(cases.size()==names.size(),"case count mismatch");
    for(size_t i=0;i<cases.size();++i){require(cases[i].failed_name==names[i],"case name/order mismatch");require(cases[i].tasks.size()==counts[i],"task count mismatch");require(cases[i].sum_task_times==(i<6),"timing policy mismatch");}
    require_task(cases[0].tasks[0],{131,132},range(0,24));require_task(cases[0].tasks[1],{133},range(25,49));
    require_task(cases[1].tasks[0],{131,132},range(0,24));require_task(cases[1].tasks[1],{135},range(50,75));
    require_task(cases[2].tasks[0],{135,136},range(50,75));require_task(cases[2].tasks[1],{137},range(76,101));
    require_task(cases[3].tasks[0],{131},range(0,24));require_task(cases[3].tasks[1],{133},range(25,49));require_task(cases[3].tasks[2],{135},range(50,75));
    require_task(cases[4].tasks[0],{133},range(25,49));require_task(cases[4].tasks[1],{135},range(50,75));require_task(cases[4].tasks[2],{137},range(76,101));
    require_task(cases[5].tasks[0],{135},range(50,75));require_task(cases[5].tasks[1],{137},range(76,101));require_task(cases[5].tasks[2],{139},range(102,127));
    require_task(cases[6].tasks[0],{128,129,130},range(0,127));
}
}

int main()
{
 try{
  constexpr int k=128,r=3,z=9,block_size=4096;const auto cases=ECProject::RepairBenchmark::make_alrc_k128_triple_block_cases();verify_cases(cases);
  std::vector<std::vector<unsigned char>> blocks(k+r+z,std::vector<unsigned char>(block_size));std::mt19937 gen(0x414c5438u);for(int id=0;id<k;++id)for(auto &b:blocks[id])b=static_cast<unsigned char>(gen());
  std::vector<unsigned char*> data(k),parity(r+z);for(int id=0;id<k;++id)data[id]=blocks[id].data();for(int id=0;id<r+z;++id)parity[id]=blocks[k+id].data();ECProject::encode_alrc(k,r,z,data.data(),parity.data(),block_size);
  for(const auto &test_case:cases){std::unordered_set<int> failed;for(const auto &task:test_case.tasks)failed.insert(task.failed_block_ids.begin(),task.failed_block_ids.end());require(failed.size()==3,"not exactly three failures");for(const auto &task:test_case.tasks){for(int source:task.source_block_ids)require(!failed.count(source),"source contains failure");std::vector<unsigned char> coefficients;require(ECProject::get_local_fill_plan(k,r,z,"ALRC",task.failed_block_ids,task.source_block_ids,coefficients),"coefficient solve failed");const int rows=task.failed_block_ids.size(),cols=task.source_block_ids.size();require(coefficients.size()==static_cast<size_t>(rows*cols),"coefficient size mismatch");std::vector<unsigned char*> inputs;for(int source:task.source_block_ids)inputs.push_back(blocks[source].data());std::vector<std::vector<unsigned char>> recovered(rows,std::vector<unsigned char>(block_size));std::vector<unsigned char*> outputs;for(auto &out:recovered)outputs.push_back(out.data());std::vector<unsigned char> tables(static_cast<size_t>(rows)*cols*32);ECProject::ec_init_tables(cols,rows,coefficients.data(),tables.data());ECProject::ec_encode_data_avx2(block_size,cols,rows,tables.data(),inputs.data(),outputs.data());for(int row=0;row<rows;++row){require(recovered[row]==blocks[task.failed_block_ids[row]],"AVX2 recovery mismatch");std::vector<unsigned char> direct(block_size,0);for(int col=0;col<cols;++col)for(int byte=0;byte<block_size;++byte)direct[byte]^=ECProject::gf_mul(coefficients[row*cols+col],blocks[task.source_block_ids[col]][byte]);require(direct==blocks[task.failed_block_ids[row]],"scalar recovery mismatch");}}}
  std::cout<<"ALRC k128 triple repair verification passed\n";return 0;
 }catch(const std::exception &e){std::cerr<<"ALRC k128 triple repair failed: "<<e.what()<<'\n';return 1;}
}
