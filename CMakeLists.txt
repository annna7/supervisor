cmake_minimum_required(VERSION 3.24)
project(supervisor C)

set(CMAKE_C_STANDARD 11)

add_executable(supervisor
        main.c utils.c supervisor.c service.c)
