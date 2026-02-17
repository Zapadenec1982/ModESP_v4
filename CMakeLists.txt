# ModESP v4 — Modular ESP32 Firmware Framework
cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS
    "drivers"
    "modules"
)

# ── Pre-build: generate UI schema and C++ headers from module manifests ──
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_FOUND)
    execute_process(
        COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_SOURCE_DIR}/tools/generate_ui.py"
            --project     "${CMAKE_SOURCE_DIR}/project.json"
            --modules-dir "${CMAKE_SOURCE_DIR}/modules"
            --output-data "${CMAKE_SOURCE_DIR}/data"
            --output-gen  "${CMAKE_SOURCE_DIR}/generated"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        RESULT_VARIABLE GEN_RESULT
    )
    if(NOT GEN_RESULT EQUAL 0)
        message(FATAL_ERROR "generate_ui.py failed (exit code ${GEN_RESULT})")
    endif()
else()
    message(WARNING "Python3 not found — skipping UI generation")
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ModESP_v4)

# Generate LittleFS image from data/ directory and flash to "data" partition
littlefs_create_partition_image(data data FLASH_IN_PROJECT)
