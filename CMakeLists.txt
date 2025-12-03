cmake_minimum_required(VERSION 3.16)
project(client)

set(CMAKE_AUTOMOC ON)
set(CMAKE_PREFIX_PATH "/opt/homebrew/Cellar/qt/6.9.3")

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(client client.cpp)
add_executable(server server.cpp)
target_link_libraries(client Qt6::Widgets)
