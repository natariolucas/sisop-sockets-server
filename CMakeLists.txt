cmake_minimum_required(VERSION 3.31)
project(sisop-sockets-server C)

set(CMAKE_C_STANDARD 11)

add_executable(server main.c
        semaphores.h
        semaphores.c
        server_socket.c
        server_socket.h
        operations.c
        operations.h
)

# get and import .h files path in MacOS to enable auto import by CLion
execute_process(
        COMMAND xcrun --sdk macosx --show-sdk-path
        OUTPUT_VARIABLE MACOSX_SDK
        OUTPUT_STRIP_TRAILING_WHITESPACE
)
include_directories(${MACOSX_SDK}/usr/include)
