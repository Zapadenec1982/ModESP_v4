# ModESP v4 — Modular ESP32 Firmware Framework
cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS
    "drivers"
    "modules"
)

# ── Board selection: читаємо з sdkconfig (menuconfig) ──
set(_BOARD_DIR "dev")
if(EXISTS "${CMAKE_SOURCE_DIR}/sdkconfig")
    file(STRINGS "${CMAKE_SOURCE_DIR}/sdkconfig" _sdkconfig_lines)
    foreach(_line ${_sdkconfig_lines})
        if(_line MATCHES "^CONFIG_MODESP_BOARD_DIR=\"(.+)\"$")
            set(_BOARD_DIR "${CMAKE_MATCH_1}")
        endif()
    endforeach()
endif()

set(_BOARD_PATH "${CMAKE_SOURCE_DIR}/boards/${_BOARD_DIR}")
if(NOT EXISTS "${_BOARD_PATH}/board.json")
    message(FATAL_ERROR "Board '${_BOARD_DIR}' not found in boards/")
endif()
file(COPY "${_BOARD_PATH}/board.json" DESTINATION "${CMAKE_SOURCE_DIR}/data")
file(COPY "${_BOARD_PATH}/bindings.json" DESTINATION "${CMAKE_SOURCE_DIR}/data")
message(STATUS "ModESP board: ${_BOARD_DIR}")

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

# Читаємо назву плати з board.json для ідентифікації прошивки
file(READ "${CMAKE_SOURCE_DIR}/data/board.json" _BOARD_JSON_CONTENT)
string(JSON _BOARD_NAME GET "${_BOARD_JSON_CONTENT}" "board")
message(STATUS "ModESP board name: ${_BOARD_NAME}")

# ── Firmware version (semver) ──
# Used by esp_app_get_description()->version in heartbeat & OTA version matching.
# MUST be updated before each release build.
set(PROJECT_VER "1.0.1")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(modesp_${_BOARD_NAME})

# ── WebUI: збираємо Svelte-бандл у складі прошивки (єдиний `idf.py build`) ──
# Раніше WebUI збирався окремо (`npm run deploy`), тому data/www/bundle.*.gz
# міг застаріти відносно webui/src. Тепер бандл — залежність LittleFS-образу.
# Gracefully пропускається, якщо npm недоступний (напр. CI/контейнер без node):
# тоді у прошивку йде закомічений data/www/bundle.*.gz.
option(MODESP_BUILD_WEBUI "Build Svelte WebUI during firmware build" ON)
set(_WEBUI_DEP "")
if(MODESP_BUILD_WEBUI)
    find_program(NPM_EXECUTABLE NAMES npm npm.cmd)
    if(NPM_EXECUTABLE)
        # Перший білд: підтягнути залежності, якщо node_modules відсутній
        if(NOT EXISTS "${CMAKE_SOURCE_DIR}/webui/node_modules")
            message(STATUS "WebUI: node_modules відсутній — npm ci (перший білд)")
            execute_process(
                COMMAND ${NPM_EXECUTABLE} ci
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/webui"
                RESULT_VARIABLE _NPM_CI_RES)
            if(NOT _NPM_CI_RES EQUAL 0)
                message(WARNING "npm ci failed (${_NPM_CI_RES}) — WebUI може не зібратись")
            endif()
        endif()

        file(GLOB_RECURSE _WEBUI_SRCS
            "${CMAKE_SOURCE_DIR}/webui/src/*")
        set(_WEBUI_OUTPUTS
            "${CMAKE_SOURCE_DIR}/data/www/bundle.js.gz"
            "${CMAKE_SOURCE_DIR}/data/www/bundle.css.gz"
            "${CMAKE_SOURCE_DIR}/data/www/index.html")
        add_custom_command(
            OUTPUT ${_WEBUI_OUTPUTS}
            COMMAND ${NPM_EXECUTABLE} run deploy
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/webui"
            DEPENDS ${_WEBUI_SRCS}
                    "${CMAKE_SOURCE_DIR}/webui/rollup.config.js"
                    "${CMAKE_SOURCE_DIR}/webui/package.json"
            COMMENT "Building ModESP WebUI (Svelte → data/www)"
            VERBATIM)
        add_custom_target(webui_assets DEPENDS ${_WEBUI_OUTPUTS})
        set(_WEBUI_DEP webui_assets)
        message(STATUS "ModESP WebUI: build enabled (npm: ${NPM_EXECUTABLE})")
    else()
        message(WARNING "npm not found — WebUI не перебудовується; "
                        "у прошивку піде закомічений data/www/bundle.*.gz")
    endif()
endif()

# Generate LittleFS image from data/ directory and flash to "data" partition.
# WebUI-бандл — залежність образу, тож пакується свіжим (якщо webui_assets є).
littlefs_create_partition_image(data data FLASH_IN_PROJECT DEPENDS ${_WEBUI_DEP})
