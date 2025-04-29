cmake_minimum_required(VERSION 3.22)

set(catdog_blinker_m4_SOURCES
    mobilenetv3_cats_vs_dogs_m4.cc
    DATA
    ${PROJECT_SOURCE_DIR}/models/mobilenetv3_cats_vs_dogs_edgetpu.tflite
)

set(catdog_blinker_m4_LINK_LIBRARIES
    libs_base-m4_freertos
    libs_tensorflow-m4
)

# Set target executable
add_executable_m4(catdog_blinker
  ${catdog_blinker_m4_SOURCES}
)

# Link against coralmicro base libraries
target_link_libraries(catdog_blinker
  ${catdog_blinker_m4_LINK_LIBRARIES}
)