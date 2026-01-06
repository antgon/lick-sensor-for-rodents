cmake_minimum_required(VERSION 3.13)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)
add_subdirectory($ENV{PICO_SDK_CONTRIB}/pico-lcd16x2_i2c/lib lcd16x2_i2c)
add_subdirectory($ENV{PICO_SDK_CONTRIB}/pico-mcp48x1/lib mcp48x1)
add_subdirectory($ENV{PICO_SDK_CONTRIB}/pico-mpr121/lib mpr121)

project(main C CXX ASM)
pico_sdk_init()

add_executable(${PROJECT_NAME} ${PROJECT_NAME}.c settings.c)
pico_set_program_name(${PROJECT_NAME} ${PROJECT_NAME})
pico_set_program_version(${PROJECT_NAME} "0.1")

pico_enable_stdio_uart(${PROJECT_NAME} 0)
pico_enable_stdio_usb(${PROJECT_NAME} 1)

# Add the standard include files to the build
target_include_directories(${PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}
  ${CMAKE_CURRENT_LIST_DIR}/..
)

target_link_libraries(
    ${PROJECT_NAME}
    pico_stdlib
    pico_multicore
    pico_sync
    hardware_spi
    hardware_i2c
    pico-lcd16x2_i2c
    pico-mcp48x1
    pico-mpr121
)

pico_add_extra_outputs(${PROJECT_NAME})
