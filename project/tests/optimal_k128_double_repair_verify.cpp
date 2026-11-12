#include "encoder.h"
#include "repair_benchmark_plan.h"

#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace {
using ECProject::RepairBenchmark::DoubleRepairCase;using ECProject::RepairBenchmark::RepairTask;
void require(bool c,const std::string&m){if(!c)throw std::runtime_error(m);}
std::vector<int> range(int a,int b){return ECProject::RepairBenchmark::inclusive_range(a,b);}
std::vector<int> globals(std::vector<int>s){s.insert(s.end(),{128,129,130});return s;}
void task(const RepairTask&t,const std::vector<int>&f,const std::vector<int>&s,const std::string&n){require(t.failed_block_ids==f,n+" failed IDs");require(t.source_block_ids==s,n+" sources");}
void verify(const std::vector<DoubleRepairCase>&c){
 require(c.size()==4,"case count");
 require(c[0].failed_name=="L0+L1"&&c[0].tasks.size()==2,"L0+L1");task(c[0].tasks[0],{131},globals(range(0,24)),"L0");task(c[0].tasks[1],{132},globals(range(25,49)),"L1");
 require(c[1].failed_name=="L0+L2"&&c[1].tasks.size()==2,"L0+L2");task(c[1].tasks[0],{131},globals(range(0,24)),"L0");task(c[1].tasks[1],{133},globals(range(50,75)),"L2");
 require(c[2].failed_name=="L2+L3"&&c[2].tasks.size()==2,"L2+L3");task(c[2].tasks[0],{133},globals(range(50,75)),"L2");task(c[2].tasks[1],{134},globals(range(76,101)),"L3");
 require(c[3].failed_name=="G0+G1"&&c[3].tasks.size()==1,"G0+G1");task(c[3].tasks[0],{128,129},range(0,127),"globals");require(c[3].tasks[0].failed_block_ids.front()==128,"G0 owner");
}
} // namespace
int main(){try{
 constexpr int k=128,r=3,z=5,bs=4096;const auto cases=ECProject::RepairBenchmark::make_optimal_uneven_double_block_cases(k);verify(cases);
 std::vector<std::vector<unsigned char>> b(k+r+z,std::vector<unsigned char>(bs));std::mt19937 g(0x4f503844u);std::uniform_int_distribution<int>d(0,255);for(int i=0;i<k;++i)for(auto&v:b[i])v=static_cast<unsigned char>(d(g));
 std::vector<unsigned char*>data(k),parity(r+z);for(int i=0;i<k;++i)data[i]=b[i].data();for(int i=0;i<r+z;++i)parity[i]=b[k+i].data();ECProject::encode_optimal_lrc(k,r,z,data.data(),parity.data(),bs);
 for(const auto&c:cases){std::unordered_set<int>failed;for(const auto&t:c.tasks)failed.insert(t.failed_block_ids.begin(),t.failed_block_ids.end());require(failed.size()==2,c.failed_name+" failures");for(const auto&t:c.tasks){for(int s:t.source_block_ids)require(!failed.count(s),c.failed_name+" reads failure");std::vector<unsigned char>coef;require(ECProject::get_local_fill_plan(k,r,z,"OptimalLRC",t.failed_block_ids,t.source_block_ids,coef),c.failed_name+" solve");size_t rows=t.failed_block_ids.size(),cols=t.source_block_ids.size();require(coef.size()==rows*cols,"matrix size");std::vector<std::vector<unsigned char>>out(rows,std::vector<unsigned char>(bs));for(size_t x=0;x<rows;++x){for(size_t y=0;y<cols;++y)for(int q=0;q<bs;++q)out[x][q]^=ECProject::gf_mul(coef[x*cols+y],b[t.source_block_ids[y]][q]);require(out[x]==b[t.failed_block_ids[x]],c.failed_name+" bytes");}if(c.failed_name=="G0+G1"){require(out[0]==b[128],"G0 local");require(out[1]==b[129],"G1 forward");}}}
 std::cout<<"OptimalLRC k128 double repair verification passed"<<std::endl;return 0;}catch(const std::exception&e){std::cerr<<"OptimalLRC k128 double repair verification failed: "<<e.what()<<std::endl;return 1;}}
