cmake_minimum_required(VERSION 3.3)
project(mallocLab)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")


set(MDRIVER mdriver.c memlib.c mm.c fsecs.c fcyc.c ftimer.c clock.c)
add_executable(mdriver ${MDRIVER})

# add_custom_target(mdriver COMMAND make -C ${mallocLab_SOURCE_DIR} CLION_EXE_DIR=${PROJECT_BINARY_DIR})