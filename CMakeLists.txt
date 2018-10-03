cmake_minimum_required(VERSION 3.10)
project(NuovoProgetto C)

set(CMAKE_C_STANDARD 11)

include_directories(ClientFolder)
include_directories(ServerFolder)

add_executable(NuovoProgetto
        ClientFolder/basic.h
        ClientFolder/client_udp.c
        ClientFolder/common.c
        ClientFolder/common.h
        ClientFolder/configurations.h
        ClientFolder/data_types.h
        ClientFolder/thread_functions.h
        ServerFolder/basic.h
        ServerFolder/basics.c
        ServerFolder/common.c
        ServerFolder/common.h
        ServerFolder/configurations.h
        ServerFolder/data_types.h
        ServerFolder/server_udp.c
        )
