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
void task(const RepairTask &actual,std::vector<int> failed,std::vector<int> sources){require(actual.failed_block_ids==failed,"failed order mismatch");require(actual.source_block_ids==sources,"source range mismatch");require(actual.failed_block_ids.front()==failed.front(),"owner mismatch");}

void verify_cases(const std::vector<QuadrupleRepairCase> &c)
{
 const std::vector<std::string> names={"L0+L1+L2+L3","L2+L3+L4+L5","L4+L5+L6+L7","L0+L1+L2+L4","L2+L3+L4+L6","L4+L5+L6+L8","L0+L2+L4+L6","L2+L4+L6+L8","G0+G1+G2+L0"};
 const std::vector<size_t> counts={2,2,2,3,3,3,4,4,1};require(c.size()==names.size(),"case count mismatch");for(size_t i=0;i<c.size();++i){require(c[i].failed_name==names[i],"case order mismatch");require(c[i].tasks.size()==counts[i],"task count mismatch");require(c[i].sum_task_times==(i<8),"timing policy mismatch");}
 task(c[0].tasks[0],{131,132},range(0,24));task(c[0].tasks[1],{133,134},range(25,49));
 task(c[1].tasks[0],{133,134},range(25,49));task(c[1].tasks[1],{135,136},range(50,75));
 task(c[2].tasks[0],{135,136},range(50,75));task(c[2].tasks[1],{137,138},range(76,101));
 task(c[3].tasks[0],{131,132},range(0,24));task(c[3].tasks[1],{133},range(25,49));task(c[3].tasks[2],{135},range(50,75));
 task(c[4].tasks[0],{133,134},range(25,49));task(c[4].tasks[1],{135},range(50,75));task(c[4].tasks[2],{137},range(76,101));
 task(c[5].tasks[0],{135,136},range(50,75));task(c[5].tasks[1],{137},range(76,101));task(c[5].tasks[2],{139},range(102,127));
 task(c[6].tasks[0],{131},range(0,24));task(c[6].tasks[1],{133},range(25,49));task(c[6].tasks[2],{135},range(50,75));task(c[6].tasks[3],{137},range(76,101));
 task(c[7].tasks[0],{133},range(25,49));task(c[7].tasks[1],{135},range(50,75));task(c[7].tasks[2],{137},range(76,101));task(c[7].tasks[3],{139},range(102,127));
 task(c[8].tasks[0],{128,129,130,131},range(0,127));
}
}
int main(){try{constexpr int k=128,r=3,z=9,bs=4096;const auto cases=ECProject::RepairBenchmark::make_alrc_k128_quadruple_block_cases();verify_cases(cases);std::vector<std::vector<unsigned char>> blocks(k+r+z,std::vector<unsigned char>(bs));std::mt19937 gen(0x414c5138u);for(int i=0;i<k;++i)for(auto &b:blocks[i])b=static_cast<unsigned char>(gen());std::vector<unsigned char*> data(k),parity(r+z);for(int i=0;i<k;++i)data[i]=blocks[i].data();for(int i=0;i<r+z;++i)parity[i]=blocks[k+i].data();ECProject::encode_alrc(k,r,z,data.data(),parity.data(),bs);
for(const auto &tc:cases){std::unordered_set<int> failed;for(const auto &t:tc.tasks)failed.insert(t.failed_block_ids.begin(),t.failed_block_ids.end());require(failed.size()==4,"not four failures");for(const auto &t:tc.tasks){for(int source:t.source_block_ids)require(!failed.count(source),"source contains failure");std::vector<unsigned char> coeff;require(ECProject::get_local_fill_plan(k,r,z,"ALRC",t.failed_block_ids,t.source_block_ids,coeff),"coefficient solve failed");int rows=t.failed_block_ids.size(),cols=t.source_block_ids.size();require(coeff.size()==static_cast<size_t>(rows*cols),"coefficient size mismatch");std::vector<unsigned char*> in;for(int source:t.source_block_ids)in.push_back(blocks[source].data());std::vector<std::vector<unsigned char>> recovered(rows,std::vector<unsigned char>(bs));std::vector<unsigned char*> out;for(auto &b:recovered)out.push_back(b.data());std::vector<unsigned char> tables(static_cast<size_t>(rows)*cols*32);ECProject::ec_init_tables(cols,rows,coeff.data(),tables.data());ECProject::ec_encode_data_avx2(bs,cols,rows,tables.data(),in.data(),out.data());for(int row=0;row<rows;++row){require(recovered[row]==blocks[t.failed_block_ids[row]],"AVX2 mismatch");std::vector<unsigned char> direct(bs,0);for(int col=0;col<cols;++col)for(int byte=0;byte<bs;++byte)direct[byte]^=ECProject::gf_mul(coeff[row*cols+col],blocks[t.source_block_ids[col]][byte]);require(direct==blocks[t.failed_block_ids[row]],"scalar mismatch");}}}std::cout<<"ALRC k128 quadruple repair verification passed\n";return 0;}catch(const std::exception&e){std::cerr<<"ALRC k128 quadruple repair failed: "<<e.what()<<'\n';return 1;}}
