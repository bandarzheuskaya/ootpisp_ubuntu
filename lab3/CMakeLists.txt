cmake_minimum_required(VERSION 3.10)
project(ShapeEditor)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)

find_package(Qt5 REQUIRED COMPONENTS Core Widgets)

add_executable(ShapeEditor ShapeEditor.cpp)

target_link_libraries(ShapeEditor Qt5::Core Qt5::Widgets)
