# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

set(WIFI_SSID $ENV{WIFI_SSID}) # copy from env
set(WIFI_PASS $ENV{WIFI_PASS})
set(ESPPORT /dev/ttyUSB0)
set(COMPONENTS /home/lieven/workspace/components )

include_directories( main 
    ${COMPONENTS}/inc
    json/single_include
    components/gps 
    component/ultrasonic 
    ../common
    ../tinycbor/src  )

add_definitions(-DWIFI_PASS=${WIFI_PASS} 
    -DWIFI_SSID=${WIFI_SSID} 
    -DESP32_IDF 
    -DSERIAL_BAUD=921600
    -DHOSTNAME=esp32
    -fexceptions
    ) # was no-exceptions 

# add_compile_options("-fexceptions")

# add_compile_options( -mtext-section-literals -ffunction-sections )

# set(EXTRA_COMPONENT_DIRS /home/lieven/workspace/limero/esp32_idf)
# set ( COMPONENTS cbor )

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(device)
