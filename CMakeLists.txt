cmake_minimum_required(VERSION 3.10)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set (CMAKE_CXX_STANDARD 14)
project(cryptomarketdata VERSION 1.0)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS)

set(CMAKE_CXX_FLAGS "-Wall -Wextra")

# test cases!
add_executable(tests tests/main.cxx)
target_compile_features(tests PRIVATE cxx_std_14)
target_compile_options(tests PRIVATE -fprofile-arcs -ftest-coverage -fprofile-instr-generate)

target_link_libraries(tests
  CONAN_PKG::rxcpp
  CONAN_PKG::websocketpp
  CONAN_PKG::openssl
  CONAN_PKG::fmt
  CONAN_PKG::rapidjson
  CONAN_PKG::date
  CONAN_PKG::doctest
  )

target_link_options(tests PRIVATE --coverage)
