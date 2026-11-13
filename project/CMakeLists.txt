cmake_minimum_required(VERSION 3.22)
project(project)

enable_testing()

find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME parameter_configuration_test
    COMMAND ${Python3_EXECUTABLE} -m unittest test_generate_xml_from_ini.py
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/config)
add_test(NAME ip_layout_test
    COMMAND ${Python3_EXECUTABLE} -m unittest test_ip_layout.py
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/config)


# 启用汇编语言支持（NASM）
enable_language(ASM_NASM)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE INTERNAL "")
set(CMAKE_ASM_NASM_SOURCE_FILE_EXTENSIONS asm)  # 识别 .asm 文件
set(CMAKE_ASM_NASM_COMPILER "/usr/bin/nasm")              # 指定 NASM 汇编器
set(CMAKE_ASM_NASM_OBJECT_FORMAT elf64)
set(CMAKE_ASM_NASM_COMPILE_OBJECT "<CMAKE_ASM_NASM_COMPILER> <INCLUDES> <FLAGS> -f elf64 -o <OBJECT> <SOURCE>")
set(CMAKE_ASM_NASM_OUTPUT_EXTENSION .o)  
# 设置 NASM 编译选项时添加包含路径
set(CMAKE_ASM_NASM_FLAGS "-f elf64 -I${CMAKE_CURRENT_SOURCE_DIR}/src/asm")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(ASM_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/asm/reg_sizes.asm
    ${CMAKE_CURRENT_SOURCE_DIR}/src/asm/gf_vect_dot_prod_avx2.asm
    ${CMAKE_CURRENT_SOURCE_DIR}/src/asm/gf_2vect_dot_prod_avx2.asm
    ${CMAKE_CURRENT_SOURCE_DIR}/src/asm/gf_3vect_dot_prod_avx2.asm
    ${CMAKE_CURRENT_SOURCE_DIR}/src/asm/gf_4vect_dot_prod_avx2.asm
    ${CMAKE_CURRENT_SOURCE_DIR}/src/asm/gf_5vect_dot_prod_avx2.asm
    ${CMAKE_CURRENT_SOURCE_DIR}/src/asm/gf_6vect_dot_prod_avx2.asm
    ${CMAKE_CURRENT_SOURCE_DIR}/src/asm/xor_gen_avx.asm
)

# 标记为 NASM 汇编文件
set_source_files_properties(
    ${ASM_SOURCES}
    PROPERTIES
    LANGUAGE ASM_NASM
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "-gdwarf-4 -gstrict-dwarf -g -Wall -std=c++17 -DASIO_STANDALONE -O3 -funroll-loops -fdata-sections -ffunction-sections")

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -funroll-loops -ftree-vectorize -march=native")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Intel")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -unroll -qopt-report -xHost")
endif()

find_package(Threads REQUIRED)

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include")
list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/third_party/grpc")
set(spdlog_DIR ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

set(protobuf_MODULE_COMPATIBLE TRUE)
find_package(Protobuf CONFIG REQUIRED)
message(STATUS "Using protobuf ${Protobuf_VERSION}")

set(_PROTOBUF_LIBPROTOBUF protobuf::libprotobuf)
set(_REFLECTION gRPC::grpc++_reflection)
if(CMAKE_CROSSCOMPILING)
  find_program(_PROTOBUF_PROTOC protoc)
else()
  set(_PROTOBUF_PROTOC $<TARGET_FILE:protobuf::protoc>)
endif()

find_package(gRPC CONFIG REQUIRED)
message(STATUS "Using gRPC ${gRPC_VERSION}")
message(STATUS "gRPC_DIR = ${gRPC_DIR}")
message(STATUS "gRPC_INCLUDE_DIRS = ${gRPC_INCLUDE_DIRS}")
message(STATUS "gRPC_LIBS = ${gRPC_LIBS}")

set(_GRPC_GRPCPP gRPC::grpc++)
if(CMAKE_CROSSCOMPILING)
  find_program(_GRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin)
else()
  set(_GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:gRPC::grpc_cpp_plugin>)
endif()

# Proto file
get_filename_component(cn_proto "${CMAKE_CURRENT_SOURCE_DIR}/src/proto/coordinator.proto" ABSOLUTE)
get_filename_component(pn_proto "${CMAKE_CURRENT_SOURCE_DIR}/src/proto/proxy.proto" ABSOLUTE)
get_filename_component(dn_proto "${CMAKE_CURRENT_SOURCE_DIR}/src/proto/datanode.proto" ABSOLUTE)
get_filename_component(cn_proto_path "${cn_proto}" PATH)
message("${cn_proto}")
message("${cn_proto_path}")

set(cn_proto_srcs "${CMAKE_CURRENT_SOURCE_DIR}/src/proto/coordinator.pb.cc")
set(cn_proto_hdrs "${CMAKE_CURRENT_SOURCE_DIR}/src/proto/coordinator.pb.h")
set(cn_grpc_srcs "${CMAKE_CURRENT_SOURCE_DIR}/src/proto/coordinator.grpc.pb.cc")
set(cn_grpc_hdrs "${CMAKE_CURRENT_SOURCE_DIR}/src/proto/coordinator.grpc.pb.h")
set(pn_proto_srcs "${CMAKE_CURRENT_SOURCE_DIR}/src/proto/proxy.pb.cc")
set(pn_proto_hdrs "${CMAKE_CURRENT_SOURCE_DIR}/src/proto/proxy.pb.h")
set(pn_grpc_srcs "${CMAKE_CURRENT_SOURCE_DIR}/src/proto/proxy.grpc.pb.cc")
set(pn_grpc_hdrs "${CMAKE_CURRENT_SOURCE_DIR}/src/proto/proxy.grpc.pb.h")
set(dn_proto_srcs "${CMAKE_CURRENT_SOURCE_DIR}/src/proto/datanode.pb.cc")
set(dn_proto_hdrs "${CMAKE_CURRENT_SOURCE_DIR}/src/proto/datanode.pb.h")
set(dn_grpc_srcs "${CMAKE_CURRENT_SOURCE_DIR}/src/proto/datanode.grpc.pb.cc")
set(dn_grpc_hdrs "${CMAKE_CURRENT_SOURCE_DIR}/src/proto/datanode.grpc.pb.h")

add_custom_command(
        OUTPUT ${cn_proto_srcs} ${cn_proto_hdrs} ${cn_grpc_srcs} ${cn_grpc_hdrs} ${pn_proto_srcs} ${pn_proto_hdrs} ${pn_grpc_srcs} ${pn_grpc_hdrs} ${dn_proto_srcs} ${dn_proto_hdrs} ${dn_grpc_srcs} ${dn_grpc_hdrs}
        COMMAND ${_PROTOBUF_PROTOC}
        ARGS --grpc_out "${CMAKE_CURRENT_SOURCE_DIR}/src/proto"
        --cpp_out "${CMAKE_CURRENT_SOURCE_DIR}/src/proto"
        -I "${CMAKE_CURRENT_SOURCE_DIR}/src/proto"
        --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
        "${cn_proto}" "${pn_proto}" "${dn_proto}"
        DEPENDS "${cn_proto}" "${pn_proto}" "${dn_proto}")

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/src/proto")
# include_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party/libmemcached/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party/asio/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party/gf-complete/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party/jerasure/include/jerasure)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party/jerasure/include)

# link_directories("${CMAKE_CURRENT_SOURCE_DIR}/third_party/libmemcached/lib")
link_directories("${CMAKE_CURRENT_SOURCE_DIR}/third_party/jerasure/lib")
link_directories("${CMAKE_CURRENT_SOURCE_DIR}/third_party/gf-complete/lib")
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/lib)

set(Boost_USE_STATIC_LIBS ON)
#find_package(Boost COMPONENTS thread REQUIRED)

# for gdb debug
# add_compile_options(-gdwarf-4 -gstrict-dwarf)

# set(testsrc ${CMAKE_CURRENT_SOURCE_DIR}/test/test.cpp)
# add_executable(unittest ${testsrc})
# target_link_libraries(unittest libJerasure.a libgf_complete.a)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src/   PROJECT_SRC)

# Targets (client|server)
foreach (_target
       run_coordinator run_proxy run_datanode main_client repair_benchmark)
    add_executable(${_target}
            ${cn_proto_srcs}
            ${cn_grpc_srcs}
            ${pn_proto_srcs}
            ${pn_grpc_srcs}
            ${dn_proto_srcs}
            ${dn_grpc_srcs}
            ${ASM_SOURCES}
            ${CMAKE_CURRENT_SOURCE_DIR}/src/client.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/coordinator.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/proxy.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/run_cpp/${_target}.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/toolbox.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/tinyxml2.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/datanode.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/lrc.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/config.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder_layout.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/recovery_strategy.cpp
            #${_target}.cpp
            )
    target_link_libraries(${_target}
            ${_REFLECTION}
            ${_GRPC_GRPCPP}
	    ${_GRPC_GRPCPP_UNSECURE}
            ${_PROTOBUF_LIBPROTOBUF}
            # -lmemcached
            -lpthread
            -lJerasure -lgf_complete
            )
endforeach ()

# Layout 单元测试（仅依赖 encoder_layout，不依赖 gRPC）
add_executable(layout_test
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/layout.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder_layout.cpp
)
target_include_directories(layout_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Legacy recovery-plan test executables are intentionally not built.
# repair_benchmark is the only repair benchmark entry point.

# Exp#7: pure encoding microbenchmark (no gRPC / cluster)
add_executable(encode_benchmark
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/encode_benchmark.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/config.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/tinyxml2.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder_layout.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/recovery_strategy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder.cpp
    ${ASM_SOURCES}
)
target_include_directories(encode_benchmark PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(encode_benchmark
    -lpthread
    -lJerasure
    -lgf_complete
)


# LotusLRC k=24,r=1,z=4 matrix and registered repair-plan verification.
add_executable(lotus_k24_z4_verify
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/lotus_k24_z4_verify.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder_layout.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/recovery_strategy.cpp
    ${ASM_SOURCES}
)
target_include_directories(lotus_k24_z4_verify PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(lotus_k24_z4_verify
    -lpthread
    -lJerasure
    -lgf_complete
)
add_test(NAME lotus_k24_z4_verify COMMAND lotus_k24_z4_verify)
set_tests_properties(lotus_k24_z4_verify PROPERTIES TIMEOUT 30)

# AzureLRC uneven local-group matrix, layout, and repair verification.
add_executable(azure_uneven_verify
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/azure_uneven_verify.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lrc.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder_layout.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/recovery_strategy.cpp
    ${ASM_SOURCES}
)
target_include_directories(azure_uneven_verify PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(azure_uneven_verify
    -lpthread
    -lJerasure
    -lgf_complete
)
add_test(NAME azure_uneven_verify COMMAND azure_uneven_verify)
set_tests_properties(azure_uneven_verify PROPERTIES TIMEOUT 30)

# AzureLRC k=96 exact L0/G0 single-block recovery verification.
add_executable(azure_k96_single_repair_verify
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/azure_k96_single_repair_verify.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder_layout.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/recovery_strategy.cpp
    ${ASM_SOURCES}
)
target_include_directories(azure_k96_single_repair_verify PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(azure_k96_single_repair_verify
    -lpthread
    -lJerasure
    -lgf_complete
)
add_test(NAME azure_k96_single_repair_verify COMMAND azure_k96_single_repair_verify)
set_tests_properties(azure_k96_single_repair_verify PROPERTIES TIMEOUT 30)

# AzureLRC k=96 exact L0+L1/G0+G1 double-block recovery verification.
add_executable(azure_k96_double_repair_verify
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/azure_k96_double_repair_verify.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder_layout.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/recovery_strategy.cpp
    ${ASM_SOURCES}
)
target_include_directories(azure_k96_double_repair_verify PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(azure_k96_double_repair_verify
    -lpthread
    -lJerasure
    -lgf_complete
)
add_test(NAME azure_k96_double_repair_verify COMMAND azure_k96_double_repair_verify)
set_tests_properties(azure_k96_double_repair_verify PROPERTIES TIMEOUT 30)

# AzureLRC k=96 exact L0+L1+L2/G0+G1+G2 triple-block recovery verification.
add_executable(azure_k96_triple_repair_verify
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/azure_k96_triple_repair_verify.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder_layout.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/recovery_strategy.cpp
    ${ASM_SOURCES}
)
target_include_directories(azure_k96_triple_repair_verify PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(azure_k96_triple_repair_verify
    -lpthread
    -lJerasure
    -lgf_complete
)
add_test(NAME azure_k96_triple_repair_verify COMMAND azure_k96_triple_repair_verify)
set_tests_properties(azure_k96_triple_repair_verify PROPERTIES TIMEOUT 30)

# AzureLRC k=96 exact G0+G1+G2+L0 quadruple-block recovery verification.
add_executable(azure_k96_quadruple_repair_verify
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/azure_k96_quadruple_repair_verify.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder_layout.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/recovery_strategy.cpp
    ${ASM_SOURCES}
)
target_include_directories(azure_k96_quadruple_repair_verify PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(azure_k96_quadruple_repair_verify
    -lpthread
    -lJerasure
    -lgf_complete
)
add_test(NAME azure_k96_quadruple_repair_verify COMMAND azure_k96_quadruple_repair_verify)
set_tests_properties(azure_k96_quadruple_repair_verify PROPERTIES TIMEOUT 30)

# AzureLRC k=128 exact L0/L2/G0 single-block recovery verification.
add_executable(azure_k128_single_repair_verify
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/azure_k128_single_repair_verify.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder_layout.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/recovery_strategy.cpp
    ${ASM_SOURCES}
)
target_include_directories(azure_k128_single_repair_verify PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(azure_k128_single_repair_verify
    -lpthread
    -lJerasure
    -lgf_complete
)
add_test(NAME azure_k128_single_repair_verify COMMAND azure_k128_single_repair_verify)
set_tests_properties(azure_k128_single_repair_verify PROPERTIES TIMEOUT 30)

# AzureLRC k=128 exact local-pair/G0+G1 double-block recovery verification.
add_executable(azure_k128_double_repair_verify
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/azure_k128_double_repair_verify.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder_layout.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/recovery_strategy.cpp
    ${ASM_SOURCES}
)
target_include_directories(azure_k128_double_repair_verify PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(azure_k128_double_repair_verify
    -lpthread
    -lJerasure
    -lgf_complete
)
add_test(NAME azure_k128_double_repair_verify COMMAND azure_k128_double_repair_verify)
set_tests_properties(azure_k128_double_repair_verify PROPERTIES TIMEOUT 30)

# AzureLRC k=128 exact local-triple/G0+G1+G2 recovery verification.
add_executable(azure_k128_triple_repair_verify
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/azure_k128_triple_repair_verify.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder_layout.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/recovery_strategy.cpp
    ${ASM_SOURCES}
)
target_include_directories(azure_k128_triple_repair_verify PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(azure_k128_triple_repair_verify
    -lpthread
    -lJerasure
    -lgf_complete
)
add_test(NAME azure_k128_triple_repair_verify COMMAND azure_k128_triple_repair_verify)
set_tests_properties(azure_k128_triple_repair_verify PROPERTIES TIMEOUT 30)

# AzureLRC k=128 exact local-quadruple/G0+G1+G2+L0 recovery verification.
add_executable(azure_k128_quadruple_repair_verify
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/azure_k128_quadruple_repair_verify.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder_layout.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/recovery_strategy.cpp
    ${ASM_SOURCES}
)
target_include_directories(azure_k128_quadruple_repair_verify PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(azure_k128_quadruple_repair_verify
    -lpthread
    -lJerasure
    -lgf_complete
)
add_test(NAME azure_k128_quadruple_repair_verify COMMAND azure_k128_quadruple_repair_verify)
set_tests_properties(azure_k128_quadruple_repair_verify PROPERTIES TIMEOUT 30)

# OptimalLRC uneven local-group matrix, layout, legacy, and repair verification.
add_executable(optimal_uneven_verify
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/optimal_uneven_verify.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lrc.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder_layout.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/recovery_strategy.cpp
    ${ASM_SOURCES}
)
target_include_directories(optimal_uneven_verify PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(optimal_uneven_verify
    -lpthread
    -lJerasure
    -lgf_complete
)
add_test(NAME optimal_uneven_verify COMMAND optimal_uneven_verify)
set_tests_properties(optimal_uneven_verify PROPERTIES TIMEOUT 30)

# OptimalLRC k=96 exact L0 single-block recovery verification.
add_executable(optimal_k96_single_repair_verify
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/optimal_k96_single_repair_verify.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder_layout.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/recovery_strategy.cpp
    ${ASM_SOURCES}
)
target_include_directories(optimal_k96_single_repair_verify PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(optimal_k96_single_repair_verify
    -lpthread
    -lJerasure
    -lgf_complete
)
add_test(NAME optimal_k96_single_repair_verify COMMAND optimal_k96_single_repair_verify)
set_tests_properties(optimal_k96_single_repair_verify PROPERTIES TIMEOUT 30)

# OptimalLRC k=96 exact L0+L1/G0+G1 double-block recovery verification.
add_executable(optimal_k96_double_repair_verify
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/optimal_k96_double_repair_verify.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder_layout.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/recovery_strategy.cpp
    ${ASM_SOURCES}
)
target_include_directories(optimal_k96_double_repair_verify PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(optimal_k96_double_repair_verify
    -lpthread
    -lJerasure
    -lgf_complete
)
add_test(NAME optimal_k96_double_repair_verify COMMAND optimal_k96_double_repair_verify)
set_tests_properties(optimal_k96_double_repair_verify PROPERTIES TIMEOUT 30)

# OptimalLRC k=96 exact L0+L1+L2/G0+G1+G2 triple-block recovery verification.
add_executable(optimal_k96_triple_repair_verify
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/optimal_k96_triple_repair_verify.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder_layout.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/recovery_strategy.cpp
    ${ASM_SOURCES}
)
target_include_directories(optimal_k96_triple_repair_verify PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(optimal_k96_triple_repair_verify
    -lpthread
    -lJerasure
    -lgf_complete
)
add_test(NAME optimal_k96_triple_repair_verify COMMAND optimal_k96_triple_repair_verify)
set_tests_properties(optimal_k96_triple_repair_verify PROPERTIES TIMEOUT 30)

# OptimalLRC k=96 exact G0+G1+G2+L0 quadruple-block recovery verification.
add_executable(optimal_k96_quadruple_repair_verify
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/optimal_k96_quadruple_repair_verify.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder_layout.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/recovery_strategy.cpp
    ${ASM_SOURCES}
)
target_include_directories(optimal_k96_quadruple_repair_verify PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(optimal_k96_quadruple_repair_verify
    -lpthread
    -lJerasure
    -lgf_complete
)
add_test(NAME optimal_k96_quadruple_repair_verify COMMAND optimal_k96_quadruple_repair_verify)
set_tests_properties(optimal_k96_quadruple_repair_verify PROPERTIES TIMEOUT 30)

# OptimalLRC k=128 exact L0/L2 single-block recovery verification.
add_executable(optimal_k128_single_repair_verify
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/optimal_k128_single_repair_verify.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder_layout.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/recovery_strategy.cpp
    ${ASM_SOURCES}
)
target_include_directories(optimal_k128_single_repair_verify PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(optimal_k128_single_repair_verify
    -lpthread
    -lJerasure
    -lgf_complete
)
add_test(NAME optimal_k128_single_repair_verify COMMAND optimal_k128_single_repair_verify)
set_tests_properties(optimal_k128_single_repair_verify PROPERTIES TIMEOUT 30)

# OptimalLRC k=128 exact local-pair/G0+G1 double-block recovery verification.
add_executable(optimal_k128_double_repair_verify
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/optimal_k128_double_repair_verify.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder_layout.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/recovery_strategy.cpp
    ${ASM_SOURCES}
)
target_include_directories(optimal_k128_double_repair_verify PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(optimal_k128_double_repair_verify -lpthread -lJerasure -lgf_complete)
add_test(NAME optimal_k128_double_repair_verify COMMAND optimal_k128_double_repair_verify)
set_tests_properties(optimal_k128_double_repair_verify PROPERTIES TIMEOUT 30)

# OptimalLRC k=128 exact local-triple/G0+G1+G2 recovery verification.
add_executable(optimal_k128_triple_repair_verify
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/optimal_k128_triple_repair_verify.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder_layout.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/recovery_strategy.cpp
    ${ASM_SOURCES}
)
target_include_directories(optimal_k128_triple_repair_verify PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(optimal_k128_triple_repair_verify -lpthread -lJerasure -lgf_complete)
add_test(NAME optimal_k128_triple_repair_verify COMMAND optimal_k128_triple_repair_verify)
set_tests_properties(optimal_k128_triple_repair_verify PROPERTIES TIMEOUT 30)

# OptimalLRC k=128 exact local-quadruple/G0+G1+G2+L0 recovery verification.
add_executable(optimal_k128_quadruple_repair_verify
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/optimal_k128_quadruple_repair_verify.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder_layout.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/recovery_strategy.cpp
    ${ASM_SOURCES}
)
target_include_directories(optimal_k128_quadruple_repair_verify PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(optimal_k128_quadruple_repair_verify -lpthread -lJerasure -lgf_complete)
add_test(NAME optimal_k128_quadruple_repair_verify COMMAND optimal_k128_quadruple_repair_verify)
set_tests_properties(optimal_k128_quadruple_repair_verify PROPERTIES TIMEOUT 30)

# UniformLRC k=96 exact L0/L1 single-block recovery verification.
add_executable(uniform_k96_single_repair_verify
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/uniform_k96_single_repair_verify.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder_layout.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/recovery_strategy.cpp
    ${ASM_SOURCES}
)
target_include_directories(uniform_k96_single_repair_verify PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(uniform_k96_single_repair_verify -lpthread -lJerasure -lgf_complete)
add_test(NAME uniform_k96_single_repair_verify COMMAND uniform_k96_single_repair_verify)
set_tests_properties(uniform_k96_single_repair_verify PROPERTIES TIMEOUT 30)

# UniformLRC k=96 exact local-pair/G0+G1 double-block recovery verification.
add_executable(uniform_k96_double_repair_verify
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/uniform_k96_double_repair_verify.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder_layout.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/recovery_strategy.cpp
    ${ASM_SOURCES}
)
target_include_directories(uniform_k96_double_repair_verify PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(uniform_k96_double_repair_verify -lpthread -lJerasure -lgf_complete)
add_test(NAME uniform_k96_double_repair_verify COMMAND uniform_k96_double_repair_verify)
set_tests_properties(uniform_k96_double_repair_verify PROPERTIES TIMEOUT 30)

# UniformLRC k=96 exact local-triple/G0+G1+G2 recovery verification.
add_executable(uniform_k96_triple_repair_verify
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/uniform_k96_triple_repair_verify.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder_layout.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/recovery_strategy.cpp
    ${ASM_SOURCES}
)
target_include_directories(uniform_k96_triple_repair_verify PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(uniform_k96_triple_repair_verify -lpthread -lJerasure -lgf_complete)
add_test(NAME uniform_k96_triple_repair_verify COMMAND uniform_k96_triple_repair_verify)
set_tests_properties(uniform_k96_triple_repair_verify PROPERTIES TIMEOUT 30)

# UniformLRC k=96 exact G0+G1+G2+L0 quadruple-block recovery verification.
add_executable(uniform_k96_quadruple_repair_verify
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/uniform_k96_quadruple_repair_verify.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder_layout.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/recovery_strategy.cpp
    ${ASM_SOURCES}
)
target_include_directories(uniform_k96_quadruple_repair_verify PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(uniform_k96_quadruple_repair_verify -lpthread -lJerasure -lgf_complete)
add_test(NAME uniform_k96_quadruple_repair_verify COMMAND uniform_k96_quadruple_repair_verify)
set_tests_properties(uniform_k96_quadruple_repair_verify PROPERTIES TIMEOUT 30)

# UniformLRC k=128 exact L0/L4 single-block recovery verification.
add_executable(uniform_k128_single_repair_verify
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/uniform_k128_single_repair_verify.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder_layout.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/recovery_strategy.cpp
    ${ASM_SOURCES}
)
target_include_directories(uniform_k128_single_repair_verify PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(uniform_k128_single_repair_verify -lpthread -lJerasure -lgf_complete)
add_test(NAME uniform_k128_single_repair_verify COMMAND uniform_k128_single_repair_verify)
set_tests_properties(uniform_k128_single_repair_verify PROPERTIES TIMEOUT 30)

# UniformLRC k=128 exact local-pair/G0+G1 double-block recovery verification.
add_executable(uniform_k128_double_repair_verify
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/uniform_k128_double_repair_verify.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder_layout.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/recovery_strategy.cpp
    ${ASM_SOURCES}
)
target_include_directories(uniform_k128_double_repair_verify PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(uniform_k128_double_repair_verify -lpthread -lJerasure -lgf_complete)
add_test(NAME uniform_k128_double_repair_verify COMMAND uniform_k128_double_repair_verify)
set_tests_properties(uniform_k128_double_repair_verify PROPERTIES TIMEOUT 30)

# UniformLRC k=128 exact local-triple/G0+G1+G2 recovery verification.
add_executable(uniform_k128_triple_repair_verify
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/uniform_k128_triple_repair_verify.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder_layout.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/recovery_strategy.cpp
    ${ASM_SOURCES}
)
target_include_directories(uniform_k128_triple_repair_verify PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(uniform_k128_triple_repair_verify -lpthread -lJerasure -lgf_complete)
add_test(NAME uniform_k128_triple_repair_verify COMMAND uniform_k128_triple_repair_verify)
set_tests_properties(uniform_k128_triple_repair_verify PROPERTIES TIMEOUT 30)

# UniformLRC k=128 exact local-quadruple/G0+G1+G2+L4 recovery verification.
add_executable(uniform_k128_quadruple_repair_verify
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/uniform_k128_quadruple_repair_verify.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder_layout.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/recovery_strategy.cpp
    ${ASM_SOURCES}
)
target_include_directories(uniform_k128_quadruple_repair_verify PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(uniform_k128_quadruple_repair_verify -lpthread -lJerasure -lgf_complete)
add_test(NAME uniform_k128_quadruple_repair_verify COMMAND uniform_k128_quadruple_repair_verify)
set_tests_properties(uniform_k128_quadruple_repair_verify PROPERTIES TIMEOUT 30)

# UniformLRC uneven data-like groups, placement, encoding, and recovery verification.
add_executable(uniform_uneven_verify
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/uniform_uneven_verify.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder_layout.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/recovery_strategy.cpp
    ${ASM_SOURCES}
)
target_include_directories(uniform_uneven_verify PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(uniform_uneven_verify
    -lpthread
    -lJerasure
    -lgf_complete
)
add_test(NAME uniform_uneven_verify COMMAND uniform_uneven_verify)
set_tests_properties(uniform_uneven_verify PROPERTIES TIMEOUT 30)

# Two-node ASIO TCP write bandwidth microbenchmark (no gRPC / cluster)
add_executable(asio_write_bench
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/asio_write_benchmark.cpp
)
target_include_directories(asio_write_bench PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/asio/include
)
target_compile_definitions(asio_write_bench PRIVATE ASIO_STANDALONE)
target_link_libraries(asio_write_bench -lpthread)
add_executable(proxy_gateway_integration
    ${cn_proto_srcs} ${cn_grpc_srcs} ${pn_proto_srcs} ${pn_grpc_srcs} ${dn_proto_srcs} ${dn_grpc_srcs}
    ${ASM_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/proxy_gateway_integration.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/proxy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/datanode.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/config.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/toolbox.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/tinyxml2.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lrc.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder_layout.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/recovery_strategy.cpp)
target_link_libraries(proxy_gateway_integration ${_REFLECTION} ${_GRPC_GRPCPP} ${_PROTOBUF_LIBPROTOBUF} -lpthread -lJerasure -lgf_complete)

add_test(NAME proxy_gateway_integration COMMAND proxy_gateway_integration)
set_tests_properties(proxy_gateway_integration PROPERTIES TIMEOUT 20)

add_test(NAME layout_test COMMAND layout_test)
set_tests_properties(layout_test
    PROPERTIES TIMEOUT 30)

# Certified HLRC encoding and placement verification.
add_executable(hlrc_encode_verify
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/hlrc_encode_verify.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder_layout.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/recovery_strategy.cpp
    ${ASM_SOURCES}
)
target_include_directories(hlrc_encode_verify PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(hlrc_encode_verify
    -lpthread
    -lJerasure
    -lgf_complete
)
add_test(NAME hlrc_encode_verify COMMAND hlrc_encode_verify)
set_tests_properties(hlrc_encode_verify PROPERTIES TIMEOUT 30)

# HLRC k=64 exact single-block recovery verification.
add_executable(hlrc_k64_single_repair_verify
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/hlrc_k64_single_repair_verify.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder_layout.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/recovery_strategy.cpp
    ${ASM_SOURCES}
)
target_include_directories(hlrc_k64_single_repair_verify PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(hlrc_k64_single_repair_verify
    -lpthread
    -lJerasure
    -lgf_complete
)
add_test(NAME hlrc_k64_single_repair_verify COMMAND hlrc_k64_single_repair_verify)
set_tests_properties(hlrc_k64_single_repair_verify PROPERTIES TIMEOUT 30)

# HLRC k=96 exact L1/L0/U0 single-block recovery verification.
add_executable(hlrc_k96_single_repair_verify
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/hlrc_k96_single_repair_verify.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder_layout.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/recovery_strategy.cpp
    ${ASM_SOURCES}
)
target_include_directories(hlrc_k96_single_repair_verify PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(hlrc_k96_single_repair_verify -lpthread -lJerasure -lgf_complete)
add_test(NAME hlrc_k96_single_repair_verify COMMAND hlrc_k96_single_repair_verify)
set_tests_properties(hlrc_k96_single_repair_verify PROPERTIES TIMEOUT 30)

# HLRC k=64 exact double-block recovery verification.
add_executable(hlrc_k64_double_repair_verify
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/hlrc_k64_double_repair_verify.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder_layout.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/recovery_strategy.cpp
    ${ASM_SOURCES}
)
target_include_directories(hlrc_k64_double_repair_verify PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(hlrc_k64_double_repair_verify
    -lpthread
    -lJerasure
    -lgf_complete
)
add_test(NAME hlrc_k64_double_repair_verify COMMAND hlrc_k64_double_repair_verify)
set_tests_properties(hlrc_k64_double_repair_verify PROPERTIES TIMEOUT 30)

# HLRC k=96 exact local/upper and G0+L0 double-block recovery verification.
add_executable(hlrc_k96_double_repair_verify
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/hlrc_k96_double_repair_verify.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder_layout.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/recovery_strategy.cpp
    ${ASM_SOURCES}
)
target_include_directories(hlrc_k96_double_repair_verify PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(hlrc_k96_double_repair_verify -lpthread -lJerasure -lgf_complete)
add_test(NAME hlrc_k96_double_repair_verify COMMAND hlrc_k96_double_repair_verify)
set_tests_properties(hlrc_k96_double_repair_verify PROPERTIES TIMEOUT 30)

# HLRC k=64 exact triple-block recovery verification.
add_executable(hlrc_k64_triple_repair_verify
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/hlrc_k64_triple_repair_verify.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder_layout.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/recovery_strategy.cpp
    ${ASM_SOURCES}
)
target_include_directories(hlrc_k64_triple_repair_verify PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(hlrc_k64_triple_repair_verify
    -lpthread
    -lJerasure
    -lgf_complete
)
add_test(NAME hlrc_k64_triple_repair_verify COMMAND hlrc_k64_triple_repair_verify)
set_tests_properties(hlrc_k64_triple_repair_verify PROPERTIES TIMEOUT 30)

# HLRC k=96 exact local/upper/global triple-block recovery verification.
add_executable(hlrc_k96_triple_repair_verify
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/hlrc_k96_triple_repair_verify.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder_layout.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/recovery_strategy.cpp
    ${ASM_SOURCES}
)
target_include_directories(hlrc_k96_triple_repair_verify PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(hlrc_k96_triple_repair_verify -lpthread -lJerasure -lgf_complete)
add_test(NAME hlrc_k96_triple_repair_verify COMMAND hlrc_k96_triple_repair_verify)
set_tests_properties(hlrc_k96_triple_repair_verify PROPERTIES TIMEOUT 30)

# HLRC k=64 exact quadruple-block recovery verification.
add_executable(hlrc_k64_quadruple_repair_verify
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/hlrc_k64_quadruple_repair_verify.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder_layout.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/recovery_strategy.cpp
    ${ASM_SOURCES}
)
target_include_directories(hlrc_k64_quadruple_repair_verify PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(hlrc_k64_quadruple_repair_verify
    -lpthread
    -lJerasure
    -lgf_complete
)
add_test(NAME hlrc_k64_quadruple_repair_verify COMMAND hlrc_k64_quadruple_repair_verify)
set_tests_properties(hlrc_k64_quadruple_repair_verify PROPERTIES TIMEOUT 30)

# HLRC k=96 exact local/upper/global quadruple-block recovery verification.
add_executable(hlrc_k96_quadruple_repair_verify
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/hlrc_k96_quadruple_repair_verify.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder_layout.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/recovery_strategy.cpp
    ${ASM_SOURCES}
)
target_include_directories(hlrc_k96_quadruple_repair_verify PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(hlrc_k96_quadruple_repair_verify -lpthread -lJerasure -lgf_complete)
add_test(NAME hlrc_k96_quadruple_repair_verify COMMAND hlrc_k96_quadruple_repair_verify)
set_tests_properties(hlrc_k96_quadruple_repair_verify PROPERTIES TIMEOUT 30)

# NLRC matrix, encoding, placement, and local MDS verification.
add_executable(nlrc_encode_verify
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/nlrc_encode_verify.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder_layout.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/recovery_strategy.cpp
    ${ASM_SOURCES}
)
target_include_directories(nlrc_encode_verify PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(nlrc_encode_verify -lpthread -lJerasure -lgf_complete)
add_test(NAME nlrc_encode_verify COMMAND nlrc_encode_verify)
set_tests_properties(nlrc_encode_verify PROPERTIES TIMEOUT 60)

# NLRC k=64 exact single-block recovery verification.
add_executable(nlrc_k64_single_repair_verify
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/nlrc_k64_single_repair_verify.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder_layout.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/recovery_strategy.cpp
    ${ASM_SOURCES}
)
target_include_directories(nlrc_k64_single_repair_verify PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(nlrc_k64_single_repair_verify
    -lpthread
    -lJerasure
    -lgf_complete
)
add_test(NAME nlrc_k64_single_repair_verify COMMAND nlrc_k64_single_repair_verify)
set_tests_properties(nlrc_k64_single_repair_verify PROPERTIES TIMEOUT 30)

# NLRC k=96 exact L0/L6 single-block recovery verification.
add_executable(nlrc_k96_single_repair_verify
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/nlrc_k96_single_repair_verify.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder_layout.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/recovery_strategy.cpp
    ${ASM_SOURCES}
)
target_include_directories(nlrc_k96_single_repair_verify PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(nlrc_k96_single_repair_verify -lpthread -lJerasure -lgf_complete)
add_test(NAME nlrc_k96_single_repair_verify COMMAND nlrc_k96_single_repair_verify)
set_tests_properties(nlrc_k96_single_repair_verify PROPERTIES TIMEOUT 30)

# NLRC k=128 exact L0/L8 single-block recovery verification.
add_executable(nlrc_k128_single_repair_verify
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/nlrc_k128_single_repair_verify.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder_layout.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/recovery_strategy.cpp
    ${ASM_SOURCES}
)
target_include_directories(nlrc_k128_single_repair_verify PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(nlrc_k128_single_repair_verify -lpthread -lJerasure -lgf_complete)
add_test(NAME nlrc_k128_single_repair_verify COMMAND nlrc_k128_single_repair_verify)
set_tests_properties(nlrc_k128_single_repair_verify PROPERTIES TIMEOUT 30)


# NLRC k=64 exact double-block recovery verification.
add_executable(nlrc_k64_double_repair_verify
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/nlrc_k64_double_repair_verify.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder_layout.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/recovery_strategy.cpp
    ${ASM_SOURCES}
)
target_include_directories(nlrc_k64_double_repair_verify PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(nlrc_k64_double_repair_verify
    -lpthread
    -lJerasure
    -lgf_complete
)
add_test(NAME nlrc_k64_double_repair_verify COMMAND nlrc_k64_double_repair_verify)
set_tests_properties(nlrc_k64_double_repair_verify PROPERTIES TIMEOUT 30)

# NLRC k=96 exact local/shared and G0+G1 double-block recovery verification.
add_executable(nlrc_k96_double_repair_verify
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/nlrc_k96_double_repair_verify.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder_layout.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/recovery_strategy.cpp
    ${ASM_SOURCES}
)
target_include_directories(nlrc_k96_double_repair_verify PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(nlrc_k96_double_repair_verify -lpthread -lJerasure -lgf_complete)
add_test(NAME nlrc_k96_double_repair_verify COMMAND nlrc_k96_double_repair_verify)
set_tests_properties(nlrc_k96_double_repair_verify PROPERTIES TIMEOUT 30)

# NLRC k=128 exact local/shared and G0+G1 double-block recovery verification.
add_executable(nlrc_k128_double_repair_verify
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/nlrc_k128_double_repair_verify.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder_layout.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/recovery_strategy.cpp
    ${ASM_SOURCES}
)
target_include_directories(nlrc_k128_double_repair_verify PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(nlrc_k128_double_repair_verify -lpthread -lJerasure -lgf_complete)
add_test(NAME nlrc_k128_double_repair_verify COMMAND nlrc_k128_double_repair_verify)
set_tests_properties(nlrc_k128_double_repair_verify PROPERTIES TIMEOUT 30)

# NLRC k=64 exact triple-block recovery verification.
add_executable(nlrc_k64_triple_repair_verify
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/nlrc_k64_triple_repair_verify.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder_layout.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/recovery_strategy.cpp
    ${ASM_SOURCES}
)
target_include_directories(nlrc_k64_triple_repair_verify PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(nlrc_k64_triple_repair_verify
    -lpthread
    -lJerasure
    -lgf_complete
)
add_test(NAME nlrc_k64_triple_repair_verify COMMAND nlrc_k64_triple_repair_verify)
set_tests_properties(nlrc_k64_triple_repair_verify PROPERTIES TIMEOUT 30)

# NLRC k=96 exact local/shared and G0+G1+G2 triple-block recovery verification.
add_executable(nlrc_k96_triple_repair_verify
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/nlrc_k96_triple_repair_verify.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder_layout.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/recovery_strategy.cpp
    ${ASM_SOURCES}
)
target_include_directories(nlrc_k96_triple_repair_verify PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(nlrc_k96_triple_repair_verify -lpthread -lJerasure -lgf_complete)
add_test(NAME nlrc_k96_triple_repair_verify COMMAND nlrc_k96_triple_repair_verify)
set_tests_properties(nlrc_k96_triple_repair_verify PROPERTIES TIMEOUT 30)

# NLRC k=128 exact local/shared and G0+G1+G2 triple-block recovery verification.
add_executable(nlrc_k128_triple_repair_verify
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/nlrc_k128_triple_repair_verify.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder_layout.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/recovery_strategy.cpp
    ${ASM_SOURCES}
)
target_include_directories(nlrc_k128_triple_repair_verify PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(nlrc_k128_triple_repair_verify -lpthread -lJerasure -lgf_complete)
add_test(NAME nlrc_k128_triple_repair_verify COMMAND nlrc_k128_triple_repair_verify)
set_tests_properties(nlrc_k128_triple_repair_verify PROPERTIES TIMEOUT 30)

# NLRC k=96 exact local/shared and G0+G1+G2+L6 quadruple-block recovery verification.
add_executable(nlrc_k96_quadruple_repair_verify
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/nlrc_k96_quadruple_repair_verify.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder_layout.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/recovery_strategy.cpp
    ${ASM_SOURCES}
)
target_include_directories(nlrc_k96_quadruple_repair_verify PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(nlrc_k96_quadruple_repair_verify -lpthread -lJerasure -lgf_complete)
add_test(NAME nlrc_k96_quadruple_repair_verify COMMAND nlrc_k96_quadruple_repair_verify)
set_tests_properties(nlrc_k96_quadruple_repair_verify PROPERTIES TIMEOUT 30)

# NLRC k=128 exact local/shared and G0+G1+G2+L8 quadruple-block recovery verification.
add_executable(nlrc_k128_quadruple_repair_verify
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/nlrc_k128_quadruple_repair_verify.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoder_layout.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/recovery_strategy.cpp
    ${ASM_SOURCES}
)
target_include_directories(nlrc_k128_quadruple_repair_verify PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(nlrc_k128_quadruple_repair_verify -lpthread -lJerasure -lgf_complete)
add_test(NAME nlrc_k128_quadruple_repair_verify COMMAND nlrc_k128_quadruple_repair_verify)
set_tests_properties(nlrc_k128_quadruple_repair_verify PROPERTIES TIMEOUT 30)
