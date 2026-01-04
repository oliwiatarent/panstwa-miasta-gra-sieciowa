cmake_minimum_required(VERSION 3.16)
project(panstwa-miasta)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -Wall -Wextra")

add_executable(server server.cpp)

find_package(Qt6 COMPONENTS Widgets Network REQUIRED)
add_executable(client_gui client_gui.cpp)
target_link_libraries(client_gui PRIVATE Qt6::Widgets Qt6::Network)
