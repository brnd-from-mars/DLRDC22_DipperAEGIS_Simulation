cmake_minimum_required(VERSION 3.17)
project(MissionSimulation)

set(CMAKE_CXX_STANDARD 20)

find_package(SFML 2.5 COMPONENTS graphics window REQUIRED)

add_executable(MissionSimulation main.cpp)

target_link_libraries(MissionSimulation sfml-graphics sfml-window)