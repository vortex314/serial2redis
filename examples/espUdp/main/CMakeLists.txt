idf_component_register(SRCS 
        main.cpp 
        LedBlinker.cpp 
        ${COMPONENTS}/src/CborWriter.cpp 
        ${COMPONENTS}/src/CborReader.cpp 
        ${COMPONENTS}/src/Crc32.cpp
        ${COMPONENTS}/esp32/Log.cpp
        ${COMPONENTS}/esp32/limero.cpp
        ${COMPONENTS}/esp32/Sys.cpp

    INCLUDE_DIRS 
        ${COMPONENTS}/inc/
    REQUIRES 
        "limero spine spi_flash tinycbor as5600")

# component_compile_options("-fexceptions")
