idf_component_register(SRCS 
        ${COMPONENTS}/src/Spine.cpp 
        ${COMPONENTS}/src/StringUtility.cpp 
        SerialFrame.cpp
        UdpFrame.cpp 
        Wifi.cpp
    INCLUDE_DIRS 
        . 
        ../../main  
    REQUIRES 
        "limero cbor nvs_flash " )
