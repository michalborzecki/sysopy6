cmake_minimum_required(VERSION 3.4)
project(zad2 C)

set(CMAKE_C_FLAGS "-Wall -lrt")

add_executable(server server.c)
add_executable(client client.c)
