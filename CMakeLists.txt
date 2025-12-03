cmake_minimum_required(VERSION 3.16)
project(panstwa-miasta)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(client client.cpp)
target_link_libraries(client Qt6::Widgets)

add_executable(server server.cpp)
