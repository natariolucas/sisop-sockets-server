cmake_minimum_required(VERSION 3.31)
project(sisop-sockets-server C)

set(CMAKE_C_STANDARD 11)

add_executable(server main.c
        semaphores/semaphores.h
        semaphores/semaphores.c
        server_socket/server_socket.c
        server_socket/server_socket.h
        operations/operations.c
        operations/operations.h
        buffers/buffers.c
        buffers/buffers.h
)

# get and import .h files path in MacOS to enable auto import by CLion
execute_process(
        COMMAND xcrun --sdk macosx --show-sdk-path
        OUTPUT_VARIABLE MACOSX_SDK
        OUTPUT_STRIP_TRAILING_WHITESPACE
)
include_directories(${MACOSX_SDK}/usr/include)
