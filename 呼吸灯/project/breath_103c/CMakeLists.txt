cmake_minimum_required(VERSION 3.20)
project(app C ASM)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 引用共享编译配置（库路径、HAL 源、启动文件、链接脚本）
include(${CMAKE_CURRENT_LIST_DIR}/../../../cmake/stm32f1.cmake)

# 阻塞 + 单灯呼吸（A0）
stm32f1_add_executable(blocking_sync.elf blocking_sync.c breath.c)
# 非阻塞 + 单灯呼吸（A0）
stm32f1_add_executable(nonblocking_sync.elf nonblocking_sync.c breath.c)
