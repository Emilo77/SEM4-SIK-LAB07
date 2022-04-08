cmake_minimum_required(VERSION 3.1)

project(MIMUW_SIK_TCP_POLL)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "-g -Wall -Wextra")

add_executable(client client.c common.h err.h)
add_executable(poll_server poll_server.c common.h err.h)

install(TARGETS DESTINATION .)
