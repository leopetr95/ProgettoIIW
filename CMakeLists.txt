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
        ClientFolder/directory_size.c
        ClientFolder/packet_functions.c
        ClientFolder/packet_functions.h
        ClientFolder/thread_functions.h
        ClientFolder/timer_functions.c
        ClientFolder/timer_functions.h
        ClientFolder/types.h
        ClientFolder/window_operations.c
        ClientFolder/window_operations.h
        ServerFolder/basic.h
        ServerFolder/basics.c
        ServerFolder/common.c
        ServerFolder/common.h
        ServerFolder/configurations.h
        ServerFolder/data_types.h
        ServerFolder/directory_size.c
        ServerFolder/packet_functions.c
        ServerFolder/packet_functions.h
        ServerFolder/server_udp.c
        ServerFolder/timer_functions.c
        ServerFolder/timer_functions.h
        ServerFolder/types.h
        ServerFolder/window_operations.c
        ServerFolder/window_operations.h)
