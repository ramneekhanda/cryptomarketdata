cmake_minimum_required(VERSION 3.10)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set (CMAKE_CXX_STANDARD 17)
project(cdesk VERSION 1.0)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS)

add_executable(cdesk
  src/main.cxx
)

target_link_libraries(cdesk
  CONAN_PKG::rxcpp
  CONAN_PKG::websocketpp
  CONAN_PKG::openssl
  CONAN_PKG::fmt
  CONAN_PKG::rapidjson
  CONAN_PKG::date
  )