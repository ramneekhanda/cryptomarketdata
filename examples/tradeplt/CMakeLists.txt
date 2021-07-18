cmake_minimum_required(VERSION 3.10)
set (CMAKE_CXX_STANDARD 11)
project(testplt VERSION 1.0)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS)

add_executable(testplt
  main.cxx
  imgui/backends/imgui_impl_glfw.cpp
  imgui/backends/imgui_impl_opengl2.cpp
  imgui/imgui.cpp
  imgui/imgui_draw.cpp
  imgui/imgui_tables.cpp
  imgui/imgui_widgets.cpp
  imgui/imgui_demo.cpp
  implot/implot_demo.cpp
  implot/implot.cpp
  implot/implot_items.cpp
)

target_link_libraries(testplt
  CONAN_PKG::glfw
  CONAN_PKG::rxcpp
  CONAN_PKG::websocketpp
  CONAN_PKG::openssl
  CONAN_PKG::fmt
  CONAN_PKG::rapidjson
  CONAN_PKG::date
  CONAN_PKG::doctest

)


target_include_directories(testplt
  PUBLIC "imgui"
  PUBLIC "implot"
  )
