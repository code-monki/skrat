cmake_minimum_required(VERSION 3.16)

project(skrat VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Widgets Pdf PdfWidgets)

add_executable(skrat
  src/main.cpp
  src/MainWindow.cpp
  src/MainWindow.h
)

target_include_directories(skrat PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

target_link_libraries(skrat PRIVATE
  Qt6::Core
  Qt6::Gui
  Qt6::Widgets
  Qt6::Pdf
  Qt6::PdfWidgets
)

if(MSVC)
  target_compile_definitions(skrat PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
