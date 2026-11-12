#include "encoder.h"
#include "repair_benchmark_plan.h"

#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace {
void require(bool condition,const std::string &message){if(!condition)throw std::runtime_error(message);}
std::vector<int> range(int first,int last){return ECProject::RepairBenchmark::inclusive_range(first,last);}
} // namespace

int main(){
    try{
        constexpr int k=96,r=3,z=4,block_size=4096;
        const auto cases=ECProject::RepairBenchmark::make_optimal_quadruple_block_cases(k);
        require(cases.size()==1,"unexpected OptimalLRC k96 quadruple case count");
        require(cases[0].failed_name=="G0+G1+G2+L0"&&cases[0].tasks.size()==1,
                "missing shared G0+G1+G2+L0 case");
        const auto &task=cases[0].tasks[0];
        require(task.failed_block_ids==std::vector<int>({96,97,98,99}),
                "quadruple failed block order mismatch");
        require(task.source_block_ids==range(0,95),"G0 proxy must read D0-D95");
        require(task.failed_block_ids.front()==96,"G0 must be the shared task owner proxy");
        std::unordered_set<int> all_failed(task.failed_block_ids.begin(),task.failed_block_ids.end());
        require(all_failed.size()==4,"case must contain four distinct failures");
        for(int source:task.source_block_ids)require(all_failed.count(source)==0,"source list contains a failed block");

        std::vector<std::vector<unsigned char>> blocks(k+r+z,std::vector<unsigned char>(block_size));
        std::mt19937 generator(0x4f503951u);std::uniform_int_distribution<int> distribution(0,255);
        for(int id=0;id<k;++id)for(auto &value:blocks[id])value=static_cast<unsigned char>(distribution(generator));
        std::vector<unsigned char*> data(k),parity(r+z);
        for(int id=0;id<k;++id)data[id]=blocks[id].data();
        for(int id=0;id<r+z;++id)parity[id]=blocks[k+id].data();
        ECProject::encode_optimal_lrc(k,r,z,data.data(),parity.data(),block_size);

        std::vector<unsigned char> coefficients;
        require(ECProject::get_local_fill_plan(k,r,z,"OptimalLRC",task.failed_block_ids,
                                               task.source_block_ids,coefficients),
                "quadruple coefficient solve failed");
        const size_t rows=task.failed_block_ids.size(),columns=task.source_block_ids.size();
        require(rows==4&&coefficients.size()==rows*columns,"quadruple coefficient matrix size mismatch");
        std::vector<std::vector<unsigned char>> recovered(rows,std::vector<unsigned char>(block_size,0));
        for(size_t row=0;row<rows;++row){
            for(size_t col=0;col<columns;++col)for(int byte=0;byte<block_size;++byte)
                recovered[row][byte]^=ECProject::gf_mul(coefficients[row*columns+col],
                                                       blocks[task.source_block_ids[col]][byte]);
            require(recovered[row]==blocks[task.failed_block_ids[row]],"quadruple recovered bytes mismatch");
        }
        require(recovered[0]==blocks[96],"G0 local write value mismatch");
        require(recovered[1]==blocks[97],"G1 forwarded value mismatch");
        require(recovered[2]==blocks[98],"G2 forwarded value mismatch");
        require(recovered[3]==blocks[99],"L0 forwarded value mismatch");
        std::cout<<"OptimalLRC k96 G0+G1+G2+L0 quadruple repair verification passed"<<std::endl;return 0;
    }catch(const std::exception &error){std::cerr<<"OptimalLRC k96 quadruple repair verification failed: "<<error.what()<<std::endl;return 1;}
}
