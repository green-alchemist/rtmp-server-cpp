cmake_minimum_required(VERSION 3.10)
project(rtmp_server)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find the Boost libraries
find_package(Boost 1.71.0 REQUIRED COMPONENTS system)

add_executable(rtmp_server
    src/main.cpp
    src/RtmpServer.cpp
    src/RtmpSession.cpp
)

# Link Boost libraries to our executable
target_link_libraries(rtmp_server PRIVATE Boost::system)