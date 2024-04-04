idf_component_register(SRCS "simulation.cpp" "renderer.cpp" "ball.cpp" "pca9554_controller.cpp" "lcd_controller.cpp" "buttons.cpp" "main.cpp" "midi_usb.c"
                    INCLUDE_DIRS ".")

target_compile_options(${COMPONENT_LIB} PRIVATE -fpermissive)