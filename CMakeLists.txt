cmake_minimum_required(VERSION 3.20)
project(gd32f450_freertos C ASM)

# NOTE: CMake support is secondary. The primary build system is Makefile.
# To build with CMake:
#   mkdir -p build && cd build
#   cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain.cmake
#   make

# ── MCU Flags ────────────────────────────────────────────────────────────────
add_compile_definitions(GD32F450 USE_STDPERIPH_DRIVER)

# ── Include Paths ────────────────────────────────────────────────────────────
include_directories(
    ${CMAKE_SOURCE_DIR}/config
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/bsp
    ${CMAKE_SOURCE_DIR}/src/bsp/gd32f4xx/CMSIS
    ${CMAKE_SOURCE_DIR}/src/bsp/gd32f4xx/StdPeriph
    ${CMAKE_SOURCE_DIR}/src/drivers/eth
    ${CMAKE_SOURCE_DIR}/src/drivers/uart
    ${CMAKE_SOURCE_DIR}/src/drivers/flash
    ${CMAKE_SOURCE_DIR}/src/drivers/timer
    ${CMAKE_SOURCE_DIR}/src/middleware/freertos
    ${CMAKE_SOURCE_DIR}/src/middleware/lwip/include
    ${CMAKE_SOURCE_DIR}/src/middleware/shell
    ${CMAKE_SOURCE_DIR}/src/utils
    ${CMAKE_SOURCE_DIR}/src/app/boot
    ${CMAKE_SOURCE_DIR}/src/app/ota
    ${CMAKE_SOURCE_DIR}/src/app/demo
)

# ── Collect Sources ──────────────────────────────────────────────────────────
file(GLOB_RECURSE PROJ_SOURCES
    src/*.c
    src/*.S
)

# ── Executable ───────────────────────────────────────────────────────────────
add_executable(${PROJECT_NAME} ${PROJ_SOURCES})

# ── Linker ───────────────────────────────────────────────────────────────────
target_link_options(${PROJECT_NAME} PRIVATE
    -T${CMAKE_SOURCE_DIR}/linker/gd32f450_flash.ld
)

# ── Output Formats ───────────────────────────────────────────────────────────
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex  $<TARGET_FILE:${PROJECT_NAME}> ${PROJECT_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${PROJECT_NAME}> ${PROJECT_NAME}.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${PROJECT_NAME}>
    COMMENT "Generating HEX and BIN files..."
)

# ── Flash Target ─────────────────────────────────────────────────────────────
add_custom_target(flash
    COMMAND openocd -f ${CMAKE_SOURCE_DIR}/scripts/openocd/gd32f4x_jlink.cfg
            -c "init; reset halt; program ${PROJECT_NAME}.hex verify reset exit"
    DEPENDS ${PROJECT_NAME}
    COMMENT "Flashing firmware via J-Link..."
)

# ── Erase Target ─────────────────────────────────────────────────────────────
add_custom_target(erase
    COMMAND openocd -f ${CMAKE_SOURCE_DIR}/scripts/openocd/gd32f4x_jlink.cfg
            -c "init; reset halt; gd32f4x mass_erase; exit"
    COMMENT "Mass erasing chip..."
)
