cmake_minimum_required(VERSION 2.9)
project(ringqueue)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -Wall -fPIC")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -Wall -fPIC")

add_executable(testapp tt_ringqueue.c)
target_link_libraries(testapp PUBLIC pthread)
