cmake_minimum_required(VERSION 3.3)
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

add_executable(Catch2Test Catch2Test.cpp)
target_link_libraries(Catch2Test Catch2::Catch2WithMain)

# indicate the entry point for the executable
add_executable(CMakeHelloWorld Hello HelloWorld.cpp)

# Indicate which libraries to include during the link process.
target_link_libraries(CMakeHelloWorld Hello)
