cmake_minimum_required(VERSION 4.3)
project(FileWrapper)

set(CMAKE_CXX_STANDARD 26)

add_executable(FileWrapper main.cpp
        FileWrapper.hpp
        FileWrapper.cpp)
