add_executable_m7(catdog_blinker
   mobilenetv3_cats_vs_dogs_m4.cc
)

target_link_libraries(catdog_blinker
    libs_base-m7_freertos
)