cmake_minimum_required(VERSION 3.13)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion RISCV_ZCB_RPI_2_2_0_3)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================

set(PICO_BOARD pico2_w CACHE STRING "Board type")

set(PICO_PLATFORM rp2350-riscv)

include(pico_sdk_import.cmake)

project(cortina_por_luminosidade C CXX)

pico_sdk_init()

add_executable(cortina_por_luminosidade 
    main.c
    adc_sensores.c
    uart_protocolo.c
)

pico_set_program_name(cortina_por_luminosidade "cortina_por_luminosidade")
pico_set_program_version(cortina_por_luminosidade "0.1")

pico_enable_stdio_uart(cortina_por_luminosidade 0)
pico_enable_stdio_usb(cortina_por_luminosidade 1)

target_link_libraries(cortina_por_luminosidade 
    pico_stdlib
    hardware_adc
    hardware_uart
    hardware_gpio
    hardware_timer
)

target_include_directories(cortina_por_luminosidade PRIVATE ${CMAKE_CURRENT_LIST_DIR})
pico_add_extra_outputs(cortina_por_luminosidade)
