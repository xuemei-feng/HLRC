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
std::vector<int> range(int first, int last) { return ECProject::RepairBenchmark::inclusive_range(first, last); }
std::vector<int> sources(std::initializer_list<std::pair<int,int>> ranges)
{
    std::vector<int> result;
    for (const auto &bounds : ranges) { const auto ids=range(bounds.first,bounds.second); result.insert(result.end(),ids.begin(),ids.end()); }
    result.insert(result.end(),{128,129,130}); return result;
}
void task(const RepairTask &actual,std::vector<int> failed,std::vector<int> inputs)
{
    require(actual.failed_block_ids==failed,"failed block order mismatch");require(actual.source_block_ids==inputs,"source order mismatch");require(actual.failed_block_ids.front()==failed.front(),"owner mismatch");
}
void verify_plan(const std::vector<TripleRepairCase> &cases)
{
    const std::vector<std::string> names={"L0+L1+L2","L0+L1+L4","L2+L3+L4","L0+L2+L4","L2+L4+L6","L4+L6+L8","G0+G1+G2"};
    require(cases.size()==names.size(),"case count mismatch");for(size_t i=0;i<cases.size();++i){require(cases[i].failed_name==names[i],"case order mismatch");require(cases[i].tasks.size()==1,"case must use one shared task");require(!cases[i].sum_task_times,"shared task must use wall timing");}
    task(cases[0].tasks[0],{131,132,133},sources({{0,24},{25,49}}));
    task(cases[1].tasks[0],{131,132,135},sources({{0,24},{50,75}}));
    task(cases[2].tasks[0],{133,134,135},sources({{25,49},{50,75}}));
    task(cases[3].tasks[0],{131,133,135},sources({{0,24},{25,49},{50,75}}));
    task(cases[4].tasks[0],{133,135,137},sources({{25,49},{50,75},{76,101}}));
    task(cases[5].tasks[0],{135,137,139},sources({{50,75},{76,101},{102,127}}));
    task(cases[6].tasks[0],{128,129,130},range(0,127));
}
}
int main()
{
 try {constexpr int k=128,r=3,z=9,block_size=4096;const auto cases=ECProject::RepairBenchmark::make_olrc_k128_triple_block_cases();verify_plan(cases);
 std::vector<std::vector<unsigned char>> blocks(k+r+z,std::vector<unsigned char>(block_size));std::mt19937 gen(0x4f4c5438u);for(int i=0;i<k;++i)for(auto &b:blocks[i])b=static_cast<unsigned char>(gen());std::vector<unsigned char*> data(k),parity(r+z);for(int i=0;i<k;++i)data[i]=blocks[i].data();for(int i=0;i<r+z;++i)parity[i]=blocks[k+i].data();ECProject::encode_olrc(k,r,z,data.data(),parity.data(),block_size);
 for(const auto &tc:cases){const auto &t=tc.tasks.front();std::unordered_set<int> failed(t.failed_block_ids.begin(),t.failed_block_ids.end());require(failed.size()==3,"not three failures");for(int source:t.source_block_ids)require(!failed.count(source),"source contains failure");std::vector<unsigned char> coeff;require(ECProject::get_local_fill_plan(k,r,z,"OLRC",t.failed_block_ids,t.source_block_ids,coeff),tc.failed_name+" solve failed");int rows=t.failed_block_ids.size(),cols=t.source_block_ids.size();require(coeff.size()==static_cast<size_t>(rows*cols),"coefficient size mismatch");std::vector<unsigned char*> in;for(int source:t.source_block_ids)in.push_back(blocks[source].data());std::vector<std::vector<unsigned char>> recovered(rows,std::vector<unsigned char>(block_size));std::vector<unsigned char*> out;for(auto &b:recovered)out.push_back(b.data());std::vector<unsigned char> tables(static_cast<size_t>(rows)*cols*32);ECProject::ec_init_tables(cols,rows,coeff.data(),tables.data());ECProject::ec_encode_data_avx2(block_size,cols,rows,tables.data(),in.data(),out.data());for(int row=0;row<rows;++row){require(recovered[row]==blocks[t.failed_block_ids[row]],tc.failed_name+" AVX2 mismatch");std::vector<unsigned char> scalar(block_size,0);for(int col=0;col<cols;++col)for(int byte=0;byte<block_size;++byte)scalar[byte]^=ECProject::gf_mul(coeff[row*cols+col],blocks[t.source_block_ids[col]][byte]);require(scalar==blocks[t.failed_block_ids[row]],tc.failed_name+" scalar mismatch");}}
 std::cout<<"OLRC k128 triple repair verification passed\n";return 0;}catch(const std::exception&e){std::cerr<<"OLRC k128 triple repair verification failed: "<<e.what()<<'\n';return 1;}
}
