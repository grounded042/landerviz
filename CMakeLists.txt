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
if(NOT DEFINED ENV{CMSIS_DSP_PATH})
  message(FATAL_ERROR "CMSIS_DSP_PATH is required; use nix develop")
endif()
add_subdirectory($ENV{PICO_PIO_USB_PATH} pico_pio_usb)

# LanderViz only needs the 256-point Q15 real FFT. Building this focused
# archive avoids compiling and linking the rest of CMSIS-DSP.
set(CMSIS_DSP_PATH $ENV{CMSIS_DSP_PATH})
add_library(cmsis_dsp_q15 STATIC
  ${CMSIS_DSP_PATH}/Source/CommonTables/arm_common_tables.c
  ${CMSIS_DSP_PATH}/Source/CommonTables/arm_const_structs.c
  ${CMSIS_DSP_PATH}/Source/BasicMathFunctions/arm_shift_q15.c
  ${CMSIS_DSP_PATH}/Source/TransformFunctions/arm_bitreversal.c
  ${CMSIS_DSP_PATH}/Source/TransformFunctions/arm_bitreversal2.c
  ${CMSIS_DSP_PATH}/Source/TransformFunctions/arm_cfft_q15.c
  ${CMSIS_DSP_PATH}/Source/TransformFunctions/arm_cfft_radix4_q15.c
  ${CMSIS_DSP_PATH}/Source/TransformFunctions/arm_rfft_init_q15.c
  ${CMSIS_DSP_PATH}/Source/TransformFunctions/arm_rfft_q15.c
)
target_include_directories(cmsis_dsp_q15 SYSTEM PUBLIC
  ${CMSIS_DSP_PATH}/Include
  ${CMSIS_DSP_PATH}/PrivateInclude
)
target_compile_definitions(cmsis_dsp_q15 PUBLIC ARM_MATH_CM0PLUS)
target_compile_options(cmsis_dsp_q15 PRIVATE -ffunction-sections -fdata-sections)
target_link_libraries(cmsis_dsp_q15 PUBLIC cmsis_core_headers)

add_executable(landerviz
  main.cpp
  src/audio_analysis.cpp
  src/effects.cpp
  src/flow_v3.cpp
  src/flow_v3_attack.cpp
  src/flow_v3_color.cpp
  src/flow_v3_combined.cpp
  src/flow_v3_energy.cpp
  src/flow_v3_engine.cpp
  src/flow_v3_mid.cpp
  src/flow_v3_texture.cpp
  src/visualizer.cpp
  src/usb_bridge.cpp
  src/usb_status.cpp
  src/usb_descriptors.cpp
  $ENV{PICO_TINYUSB_PATH}/src/portable/raspberrypi/pio_usb/hcd_pio_usb.c
)
target_include_directories(landerviz PRIVATE include ${CMAKE_CURRENT_LIST_DIR})
target_compile_definitions(landerviz PRIVATE
  LANDERVIZ_USE_CMSIS_DSP_Q15
  PIO_USB_USE_TINYUSB
)
target_compile_options(landerviz PRIVATE -Wall -Wextra -Werror)
target_link_libraries(landerviz PRIVATE
  pico_stdlib
  pico_multicore
  pico_pio_usb
  cmsis_dsp_q15
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
