cmake_minimum_required(VERSION 3.10)
project(supervisor_daemon)

set(CMAKE_C_STANDARD 99)

add_executable(supervisor_daemon daemon.c supervisor.c service.c utils.c)