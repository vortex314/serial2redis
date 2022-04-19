cmake_minimum_required(VERSION 3.12)
project(serial2redis)

add_executable(serial2redis)
add_executable(udp2redis)
add_executable(ws2redis)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g ")
set(CMAKE_C_FLAGS "${CMAKE_CXX_FLAGS} -g ")
add_definitions(-DLINUX -std=c++17) # needed 17 for seasocks

set(HIREDIS ./hiredis )
set(LINUX ../limero/linux)
set(LIMERO ../limero)
set(ARDUINOJSON ./ArduinoJson/src)
set(SEASOCKS ./seasocks)
set(SEASOCKS_LDIR ${SEASOCKS}/build/src/main/c)
set(SEASOCKS_INC ${SEASOCKS}/src/main/c)

include_directories(
    src 
    ${LIMERO}/inc
    ${HIREDIS}
    ${ARDUINOJSON}
    ${SEASOCKS_INC}
    )

target_sources(serial2redis PRIVATE 
    src/serial2redis.cpp 
    src/config.cpp 
    src/SessionSerial.cpp
    src/serial.cpp 
    src/Redis.cpp
    src/Framing.cpp
    ${LIMERO}/src/PPP.cpp
    ${LIMERO}/linux/Log.cpp
    ${LIMERO}/linux/Sys.cpp
    ${LIMERO}/linux/limero.cpp
    ${LIMERO}/src/printf.c
    src/Common.cpp
    ${LIMERO}/src/StringUtility.cpp
    ) 

target_sources(ws2redis PRIVATE 
    src/ws2redis.cpp 
    src/config.cpp 
    src/Redis.cpp
    ${LIMERO}/linux/Log.cpp
    ${LIMERO}/linux/Sys.cpp
    ${LIMERO}/linux/limero.cpp
    ${LIMERO}/src/printf.c
    src/Common.cpp
    ${LIMERO}/src/StringUtility.cpp
    ) 

target_sources(udp2redis PRIVATE 
    src/udp2redis.cpp 
    src/Redis.cpp
    src/config.cpp 
    src/SessionUdp.cpp
    src/Udp.cpp 
    src/Common.cpp
    ${LIMERO}/linux/Log.cpp
    ${LIMERO}/linux/Sys.cpp
    ${LIMERO}/linux/limero.cpp
    ${LIMERO}/src/StringUtility.cpp
    ${LIMERO}/src/printf.c
    ) 


target_link_libraries(serial2redis 
    -lpthread  
    -L${HIREDIS} -l:libhiredis.a
    -lrt  
    -latomic)

target_link_libraries(ws2redis 
    -lpthread  
    -L/home/lieven/workspace/serial2redis/seasocks/build/src/main/c -l:libseasocks.a
    -lz
    -L${HIREDIS} -l:libhiredis.a
    -lrt  
    -latomic)

target_link_libraries(udp2redis 
    -lpthread  
    -L${HIREDIS} -l:libhiredis.a
    -lrt  
    -latomic)
