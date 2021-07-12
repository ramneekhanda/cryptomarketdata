cmake_minimum_required(VERSION 3.10)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set (CMAKE_CXX_STANDARD 17)
project(cdesk VERSION 1.0)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS)

set(CMAKE_CXX_FLAGS "-O0 -Wall -Wextra")

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

# test cases!
add_executable(tests tests/main.cxx)
target_compile_features(tests PRIVATE cxx_std_17)

target_link_libraries(tests
  CONAN_PKG::rxcpp
  CONAN_PKG::websocketpp
  CONAN_PKG::openssl
  CONAN_PKG::fmt
  CONAN_PKG::rapidjson
  CONAN_PKG::date
  CONAN_PKG::doctest
  )
