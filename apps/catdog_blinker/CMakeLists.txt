add_executable_m7(catdog_blinker
   main.cc
)

target_link_libraries(catdog_blinker
    libs_base-m7_freertos
    libs_jpeg_m7
)