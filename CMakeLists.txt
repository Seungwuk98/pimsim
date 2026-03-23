cmake_minimum_required(VERSION 3.16)
project(pimsim VERSION 0.1.0)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})
llvm_map_components_to_libnames(llvm_libs support)

add_subdirectory(third-party)

include_directories(include)
include_directories(${PROJECT_BINARY_DIR}/include)
include_directories(third-party/doctest)
include_directories(third-party/DRAMsim3/src)
include_directories(third-party/DRAMsim3/ext/headers)

option(PIMSIM_BUILD_DRIVER "Build the PIMSim driver" ON)

if(PIMSIM_BUILD_DRIVER)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
  add_subdirectory(driver)
endif()

add_subdirectory(include)
add_subdirectory(src)
add_subdirectory(tools)




