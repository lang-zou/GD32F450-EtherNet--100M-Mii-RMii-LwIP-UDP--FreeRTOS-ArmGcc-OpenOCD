# ARM GCC toolchain definition for CMake
set(CMAKE_SYSTEM_NAME              Generic)
set(CMAKE_SYSTEM_PROCESSOR        arm)

set(CMAKE_C_COMPILER              arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER            arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER            arm-none-eabi-gcc)
set(CMAKE_OBJCOPY                 arm-none-eabi-objcopy)
set(CMAKE_OBJDUMP                 arm-none-eabi-objdump)
set(CMAKE_SIZE                    arm-none-eabi-size)

set(CMAKE_EXECUTABLE_SUFFIX       .elf)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# MCU Flags
set(CPU_FLAGS     -mcpu=cortex-m4 -mthumb)
set(FPU_FLAGS     -mfpu=fpv4-sp-d16 -mfloat-abi=hard)
set(MCU_FLAGS     ${CPU_FLAGS} ${FPU_FLAGS})

set(CMAKE_C_FLAGS_INIT             "${MCU_FLAGS} -std=c11 -Os -g3 -gdwarf-2 -Wall -Wextra -Wshadow -Wundef -ffunction-sections -fdata-sections -fno-common -DGD32F450 -DUSE_STDPERIPH_DRIVER")
set(CMAKE_ASM_FLAGS_INIT           "${MCU_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS_INIT    "${MCU_FLAGS} -specs=nosys.specs -Wl,--gc-sections -Wl,-Map=${CMAKE_PROJECT_NAME}.map -lc -lm -lnosys")
