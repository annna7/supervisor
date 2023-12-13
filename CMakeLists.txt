cmake_minimum_required(VERSION 3.22)
project(supervisor)

set(CMAKE_C_STANDARD 11)

add_subdirectory(daemon)
add_subdirectory(cli)
add_subdirectory(parrot_service)
