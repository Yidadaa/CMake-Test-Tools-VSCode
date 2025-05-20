cmake_minimum_required(VERSION 3.6)
project(CMakeHelloWorld)

# version number
set(CMakeHelloWorld_VERSION_MAJOR 1)
set(CMakeHelloWorld_VERSION_MINOR 0)
set(CMAKE_CXX_STANDARD 20)

# catch2 library
include(FetchContent)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.0.1 # or a later release
)

FetchContent_MakeAvailable(Catch2)

# gtest library
FetchContent_Declare(
  gtest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt
    ON
    CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(gtest)

add_executable(Catch2Test Catch2Test.cpp)
target_link_libraries(Catch2Test Catch2::Catch2WithMain)

add_executable(GoogleTest GoogleTest.cpp)
target_link_libraries(GoogleTest gtest_main)

# indicate the entry point for the executable
add_executable(CMakeHelloWorld HelloWorld.cpp)
