cmake_minimum_required(VERSION 3.22)
project(cli C)

set(CMAKE_C_STANDARD 11)

include_directories(.)

add_executable(cli
        cli.c)