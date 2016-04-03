project(ffmpeg_wrapper)
cmake_minimum_required(VERSION 2.8)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -ggdb -Wall -std=c++14")

# Global requirements
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Boost REQUIRED filesystem program_options system)
include_directories(${Boost_INCLUDE_DIRS})
link_directories(${Boost_LIBRARY_DIRS})
set(LIBS ${LIBS} ${Boost_LIBRARIES})

set(FFMPEG_WRAPPER_SOURCE ffmpeg_wrapper.h ffmpeg_wrapper.cpp)
set(LIBS ${LIBS} pthread)

add_library(ffmpeg_wrapper ${FFMPEG_WRAPPER_SOURCE})
target_link_libraries(ffmpeg_wrapper ${LIBS})

add_executable(test_wrapper test_wrapper.cpp)
target_link_libraries(test_wrapper ffmpeg_wrapper)