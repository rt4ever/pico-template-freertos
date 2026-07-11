cmake_minimum_required(VERSION 3.12)

set(CMAKE_BUILD_TYPE RelWithDebInfo)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(pico_sdk_import.cmake)

set(FREERTOS_KERNEL_PATH ${CMAKE_CURRENT_LIST_DIR}/freertos)
include(FreeRTOS_Kernel_import.cmake)

project(pico-template-freertos)

pico_sdk_init()

set(LV_BUILD_CONF_DIR "${CMAKE_SOURCE_DIR}/include/lvgl" 
    CACHE PATH "Directory for generated LVGL configuration files")

add_subdirectory(lvgl)

#-------------------------------------------------------------------------------
# Extract git build info and generate build_info.h
#-------------------------------------------------------------------------------
set(GIT_HASH "")
set(GIT_HASH_SHORT "")
set(GIT_TAG "")
set(GIT_BRANCH "")
set(GIT_DIRTY "")

find_package(Git QUIET)
if(GIT_FOUND)
  execute_process(
    COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
  )
  execute_process(
    COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_HASH_SHORT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
  )
  execute_process(
    COMMAND ${GIT_EXECUTABLE} describe --tags --always --dirty
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_TAG
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
  )
  execute_process(
    COMMAND ${GIT_EXECUTABLE} rev-parse --abbrev-ref HEAD
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_BRANCH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
  )
  execute_process(
    COMMAND ${GIT_EXECUTABLE} diff-index --quiet HEAD --
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    RESULT_VARIABLE GIT_DIFF_EXIT
    ERROR_QUIET
  )
  if(NOT GIT_DIFF_EXIT EQUAL 0)
    set(GIT_DIRTY "true")
    set(GIT_DIRTY_SUFFIX " (dirty)")
  else()
    set(GIT_DIRTY "false")
    set(GIT_DIRTY_SUFFIX "")
  endif()
endif()

string(TIMESTAMP BUILD_DATE "%Y-%m-%d %H:%M:%S")

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/include/build_info.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/generated/build_info.h
  @ONLY
)

add_executable(pico-template-freertos
    src/main.c
    src/usb_descriptors.c
    src/tusb_task.c
    src/app_disp_task.c
    src/platform_hw.c
    src/pio_ctrl.c
    src/display_test.c
)


# Compile .pio → .pio.h via pioasm
pico_generate_pio_header(pico-template-freertos 
  ${CMAKE_CURRENT_SOURCE_DIR}/src/pio/pio_i3c.pio
  ${CMAKE_CURRENT_SOURCE_DIR}/src/pio/pio_toggle.pio
  )

target_include_directories(pico-template-freertos PRIVATE
        include/
        ${CMAKE_CURRENT_BINARY_DIR}/generated
        lvgl
        )


target_compile_options(pico-template-freertos PRIVATE -Wall)


target_link_libraries(pico-template-freertos PRIVATE
        pico_multicore
        pico_stdlib
        pico_unique_id
        tinyusb_device
        tinyusb_board
        hardware_spi
        hardware_pio
        hardware_irq
        hardware_clocks
        FreeRTOS-Kernel
        FreeRTOS-Kernel-Heap1
        lvgl
)

pico_set_binary_type(pico-template-freertos copy_to_ram)

pico_add_extra_outputs(pico-template-freertos)
