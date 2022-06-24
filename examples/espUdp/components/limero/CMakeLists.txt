idf_component_register(SRCS 
        Sema.cpp 
        Hardware.cpp
    INCLUDE_DIRS 
        . 
        ../../main  
    REQUIRES 
        "esp_adc_cal")
