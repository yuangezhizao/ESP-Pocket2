cmake_minimum_required(VERSION 3.18)
project(pc_simulator C)

include(FetchContent)

# 提前查找 SDL2 和 Python3，确保其 include 路径可在 LVGL 构建时使用。SDL2 优先使用 CMake config 模式（macOS Homebrew 提供 sdl2-config.cmake），失败则退回 module 模式（Linux 系统包）。
find_package(SDL2 CONFIG QUIET)
if(NOT SDL2_FOUND)
    find_package(SDL2 REQUIRED)
endif()
find_package(Python3 REQUIRED COMPONENTS Interpreter)

FetchContent_Declare(
    lvgl
    GIT_REPOSITORY https://github.com/lvgl/lvgl.git
    GIT_TAG 85aa60d18b3d5e5588d7b247abf90198f07c8a63  # v9.5.0（pin 到 commit 以保证可复现）
)
# 让 LVGL 使用本工程的 lv_conf.h
set(LV_BUILD_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h" CACHE FILEPATH "" FORCE)
# 禁用 demos/examples 以加速构建
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

# FetchContent_Populate 在 CMake ≥3.28 被标为废弃；此处需要它在 add_subdirectory 之前删除 ARM Helium 汇编（lv_blend_helium.S 被 os_desktop.cmake 无条件 glob），因为 x86 汇编器无法解析 ARM MVE 指令集。
# if(POLICY ...) guard 防止旧版 CMake 不认识该 policy 时报错。
if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif()
FetchContent_GetProperties(lvgl)
if(NOT lvgl_POPULATED)
    FetchContent_Populate(lvgl)
    # x86 汇编器无法解析 ARM MVE 指令集，故在 add_subdirectory 前删除此文件
    file(REMOVE "${lvgl_SOURCE_DIR}/src/draw/sw/blend/helium/lv_blend_helium.S")
    add_subdirectory("${lvgl_SOURCE_DIR}" "${lvgl_BINARY_DIR}")
endif()

# 统一 SDL2 链接目标 PC_SIMULATOR_SDL2_LINK：config 模式用 SDL2::SDL2 imported target（含完整
# include/链接属性，macOS Homebrew 需要）；module 模式（Linux 系统包）用 ${SDL2_LIBRARIES} 并补 include 路径。
if(TARGET SDL2::SDL2)
    set(PC_SIMULATOR_SDL2_LINK SDL2::SDL2)
else()
    set(PC_SIMULATOR_SDL2_LINK ${SDL2_LIBRARIES})
    target_include_directories(lvgl PUBLIC ${SDL2_INCLUDE_DIRS})
endif()
# lvgl 内部用到 <SDL2/SDL.h>；PUBLIC 链接使 include/链接属性向下传播到本工程
target_link_libraries(lvgl PUBLIC ${PC_SIMULATOR_SDL2_LINK})

# 为 LVGLImage.py 创建 Python 虚拟环境并安装依赖。
# 背景：macOS Homebrew / Ubuntu 等 PEP 668 "外部管理"Python 环境禁止直接 pip install；虚拟环境是官方推荐做法，且使构建完全自包含（venv 位于 build 目录，随 build 忽略规则被忽略）。
# 本段 pip 安装依赖需访问 PyPI；此外首次 configure 前面的 FetchContent 还会访问 GitHub 拉取 LVGL。网络受限时需配置 Git/pip 代理或预先缓存。
# 幂等策略：把依赖清单写入 sentinel，configure 时比对——清单变化（或 sentinel 缺失）即重装，避免"依赖列表变更但旧 sentinel 仍在"导致不更新。
# 路径统一加引号，兼容含空格的构建目录。
set(LVGL_PY_DEPS pypng lz4 Pillow)
set(LVGL_VENV_DIR "${CMAKE_CURRENT_BINARY_DIR}/.lvgl_venv")
set(LVGL_VENV_SENTINEL "${LVGL_VENV_DIR}/.deps_ok")
if(WIN32)
    set(LVGL_PYTHON "${LVGL_VENV_DIR}/Scripts/python.exe")
else()
    set(LVGL_PYTHON "${LVGL_VENV_DIR}/bin/python3")
endif()

if(NOT EXISTS "${LVGL_VENV_DIR}")
    message(STATUS "正在创建 Python 虚拟环境（${LVGL_VENV_DIR}）...")
    execute_process(
        COMMAND "${Python3_EXECUTABLE}" -m venv "${LVGL_VENV_DIR}"
        RESULT_VARIABLE VENV_RESULT
    )
    if(NOT VENV_RESULT EQUAL 0)
        message(FATAL_ERROR "Python 虚拟环境创建失败，请确认 Python3 已安装（python3 -m venv）")
    endif()
endif()

# 读取 sentinel 中已安装的依赖清单，与当前 LVGL_PY_DEPS 比对
set(LVGL_VENV_DEPS_CACHED "")
if(EXISTS "${LVGL_VENV_SENTINEL}")
    file(READ "${LVGL_VENV_SENTINEL}" LVGL_VENV_DEPS_CACHED)
endif()
if(NOT LVGL_VENV_DEPS_CACHED STREQUAL "${LVGL_PY_DEPS}")
    message(STATUS "正在安装 LVGLImage.py 依赖（pypng lz4 Pillow；首次 configure 需访问 PyPI）...")
    execute_process(
        COMMAND "${LVGL_PYTHON}" -m pip install ${LVGL_PY_DEPS} --quiet
        RESULT_VARIABLE PIP_RESULT
    )
    if(NOT PIP_RESULT EQUAL 0)
        message(FATAL_ERROR "安装 Python 依赖（pypng lz4 Pillow）失败，请检查网络连接（首次 configure 需访问 PyPI）")
    endif()
    file(WRITE "${LVGL_VENV_SENTINEL}" "${LVGL_PY_DEPS}")
endif()

# 构建时由 LVGL 自带脚本（scripts/LVGLImage.py）从 main/images/esp_logo.png 生成 esp_logo.c，不入库。
# 格式：ARGB8888，与固件侧 lvgl_port_create_c_image(... "ARGB8888" ...) 一致。
# Python 依赖（pypng lz4 Pillow）由上方 venv 自动处理，无需手动安装。
set(ESP_LOGO_C "${CMAKE_CURRENT_BINARY_DIR}/esp_logo.c")
add_custom_command(
    OUTPUT "${ESP_LOGO_C}"
    COMMAND "${LVGL_PYTHON}"
        "${lvgl_SOURCE_DIR}/scripts/LVGLImage.py"
        --ofmt C --cf ARGB8888 --compress NONE
        --name esp_logo
        -o "${CMAKE_CURRENT_BINARY_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../main/images/esp_logo.png"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/../main/images/esp_logo.png"
    COMMENT "从 esp_logo.png 生成 esp_logo.c"
    VERBATIM
)

add_executable(pc_simulator
    "main.c"
    "../main/ui_app/ui_app.c"
    "${ESP_LOGO_C}"
)
target_include_directories(pc_simulator PRIVATE
    "../main/ui_app"
    ${SDL2_INCLUDE_DIRS}
)
target_link_libraries(pc_simulator PRIVATE lvgl ${PC_SIMULATOR_SDL2_LINK})
