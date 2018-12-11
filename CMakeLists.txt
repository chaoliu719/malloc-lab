cmake_minimum_required(VERSION 3.3)
project(mallocLab)

set(CMAKE_C_STANDARD_REQUIRED 11)

add_executable(mdriver mdriver.c mm.c clock.c fcyc.c fsecs.c ftimer.c memlib.c)

#add_custom_target(mdriver COMMAND make -C ${mallocLab_SOURCE_DIR} CLION_EXE_DIR=${PROJECT_BINARY_DIR})