cmake_minimum_required(VERSION 3.23)
project(tomatoes)

set(CMAKE_CXX_STANDARD 17)

add_executable(tomatoes main.cpp init.cpp mpak.cpp)
target_link_libraries(tomatoes SDL2)

add_executable(test_mpak test_mpak.cpp mpak.cpp)
