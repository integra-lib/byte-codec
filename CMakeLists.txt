cmake_minimum_required(VERSION 3.21)

project(
  hwlib_byte_codec
  VERSION 0.2.0
  LANGUAGES CXX)

add_library(hwlib_byte_codec INTERFACE)
add_library(Hwlib::byte_codec ALIAS hwlib_byte_codec)

target_include_directories(hwlib_byte_codec
                           INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(hwlib_byte_codec INTERFACE cxx_std_20)

# Dependants read the version from this property rather than from a variable: a
# variable set here is invisible in a sibling directory, and a stale one left in
# the parent scope would be read instead — a version check that silently passes.
set_target_properties(hwlib_byte_codec PROPERTIES HWLIB_VERSION
                                                  ${PROJECT_VERSION})

if(PROJECT_IS_TOP_LEVEL)
  include(FetchContent)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2)
  FetchContent_MakeAvailable(googletest)

  include(GoogleTest)
  enable_testing()

  add_executable(byte-codec-test tests/byte_codec_test.cpp)
  target_link_libraries(byte-codec-test PRIVATE Hwlib::byte_codec
                                                GTest::gtest_main)
  gtest_discover_tests(byte-codec-test)
endif()
