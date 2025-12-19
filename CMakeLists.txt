cmake_minimum_required(VERSION 3.16)
project(panstwa-miasta)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

add_executable(client client.cpp)

add_executable(server server.cpp)
