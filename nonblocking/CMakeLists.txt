cmake_minimum_required(VERSION 3.1)

project(MIMUW_SIK_TCP_POLL)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "-g -Wall -Wextra")

add_executable(echo-client-timeout echo-client-timeout.c common.h err.h)
add_executable(echo-server-nonblocking echo-server-nonblocking.c common.h err.h)

install(TARGETS DESTINATION .)
