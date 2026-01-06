cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(PICO_BOARD pico CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

# Add pico-mpr121 directory
add_subdirectory($ENV{PICO_SDK_CONTRIB}/pico-mpr121/lib mpr121)

# Set project name
project(lick_bnc_multiple C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable.
add_executable(${PROJECT_NAME} ${PROJECT_NAME}.c)
pico_set_program_name(${PROJECT_NAME} ${PROJECT_NAME})
pico_set_program_version(${PROJECT_NAME} "0.1")

# Stdio configuration
pico_enable_stdio_uart(${PROJECT_NAME} 0)
pico_enable_stdio_usb(${PROJECT_NAME} 1)

# Add the standard include files to the build
target_include_directories(${PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}
)

# Add the standard and user-requested libraries to the build
target_link_libraries(
    ${PROJECT_NAME}
    pico_stdlib
    hardware_i2c
    pico-mpr121
)

pico_add_extra_outputs(${PROJECT_NAME})
