cmake_minimum_required(VERSION 3.20)
project(paint_preview LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

list(PREPEND CMAKE_PREFIX_PATH /usr/lib64)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

add_executable(paint_preview main.cpp)
target_link_libraries(paint_preview PRIVATE Qt6::Core Qt6::Widgets)
