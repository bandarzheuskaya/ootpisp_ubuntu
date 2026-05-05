cmake_minimum_required(VERSION 3.10)
project(ShapeEditor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt5 REQUIRED COMPONENTS Core Widgets)

add_executable(ShapeEditor
    main.cpp
    core/GeometryUtils.cpp
    shapes/Shape.cpp
    shapes/PolygonShape.cpp
    shapes/EllipseShape.cpp
    shapes/GroupShape.cpp
    serialization/EditorJson.cpp
    ui/MainWindow.cpp
)

target_include_directories(ShapeEditor PRIVATE
    core
    shapes
    ui
    serialization
)

target_link_libraries(ShapeEditor PRIVATE
    Qt5::Core
    Qt5::Widgets
)