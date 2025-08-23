# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico_w CACHE STRING "Board type")

# We need to turn off the default descriptor, so the CMAKE variable must be set
# before pico_sdk_init() is called
set(PICO_STDIO_USB_NO_DEFAULT_DESCRIPTOR 1)

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(RP C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(RP
        RP.c
        usb_descriptors.c
        )

# Add compile definition to enable TinyUSB HID class
target_compile_definitions(RP PRIVATE
    CFG_TUD_HID=1
)

pico_set_program_name(RP "RP")
pico_set_program_version(RP "0.1")

# Generate PIO header
pico_generate_pio_header(RP ${CMAKE_CURRENT_LIST_DIR}/blink.pio)

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(RP 1)
pico_enable_stdio_usb(RP 1) # Re-enabled for composite device

# Add the standard library to the build
target_link_libraries(RP
        pico_stdlib)

# Add the standard include files to the build
target_include_directories(RP PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

# Add any user requested libraries
target_link_libraries(RP 
        hardware_i2c
        hardware_dma
        hardware_pio
        hardware_interp
        hardware_timer
        hardware_clocks
        pico_cyw43_arch_lwip_poll
        pico_tinyusb_device
        )

pico_add_extra_outputs(RP)

