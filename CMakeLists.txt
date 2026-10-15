cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED ENV{PICO_SDK_PATH})
  message(FATAL_ERROR "PICO_SDK_PATH is required; use nix develop")
endif()
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(landerviz LANGUAGES C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

if(NOT DEFINED ENV{PICO_PIO_USB_PATH})
  message(FATAL_ERROR "PICO_PIO_USB_PATH is required; use nix develop")
endif()
if(NOT DEFINED ENV{PICO_TINYUSB_PATH})
  message(FATAL_ERROR "PICO_TINYUSB_PATH is required; use nix develop")
endif()
add_subdirectory($ENV{PICO_PIO_USB_PATH} pico_pio_usb)

add_executable(landerviz
  main.cpp
  src/audio_analysis.cpp
  src/effects.cpp
  src/usb_bridge.cpp
  src/usb_status.cpp
  src/usb_descriptors.cpp
  $ENV{PICO_TINYUSB_PATH}/src/portable/raspberrypi/pio_usb/hcd_pio_usb.c
)
target_include_directories(landerviz PRIVATE include ${CMAKE_CURRENT_LIST_DIR})
target_compile_definitions(landerviz PRIVATE PIO_USB_USE_TINYUSB)
target_compile_options(landerviz PRIVATE -Wall -Wextra -Werror)
target_link_libraries(landerviz PRIVATE
  pico_stdlib
  pico_multicore
  pico_pio_usb
  hardware_adc
  hardware_clocks
  hardware_timer
  tinyusb_device
  tinyusb_host
)
pico_enable_stdio_usb(landerviz 0)
pico_enable_stdio_uart(landerviz 0)
pico_add_extra_outputs(landerviz)

include(CTest)
if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
  add_subdirectory(tests)
endif()
