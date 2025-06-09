add_executable_m7(catdog_blinker 
    main_m7.cc
    DATA
    ${PROJECT_SOURCE_DIR}/models/mobilenetv2_catdog_quant_edgetpu.tflite
)

target_link_libraries(catdog_blinker
    libs_base-m7_freertos
    libs_jpeg_m7
)
