cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(test_project C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

add_executable(theremin
  theremin.c
  usb_descriptors.c
  chords.c
  hc-sr04.c
  ssd1306.c
)

pico_enable_stdio_usb(theremin 0)
pico_enable_stdio_uart(theremin 0)

target_include_directories(theremin PRIVATE ${CMAKE_CURRENT_LIST_DIR})

pico_add_extra_outputs(theremin)
target_link_libraries(theremin PRIVATE pico_stdlib tinyusb_device tinyusb_board hardware_i2c)
