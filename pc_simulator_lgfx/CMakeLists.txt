cmake_minimum_required(VERSION 3.18)
project(pc_simulator_lgfx C CXX)

include(FetchContent)

find_package(SDL2 CONFIG QUIET)
if(NOT SDL2_FOUND)
    find_package(SDL2 REQUIRED)
endif()
find_package(Python3 REQUIRED COMPONENTS Interpreter)

# ---- LVGL v9.5.0（与固件同一 minor；使用本工程 lv_conf.h，关闭 LV_USE_SDL） ----
FetchContent_Declare(
    lvgl
    GIT_REPOSITORY https://github.com/lvgl/lvgl.git
    GIT_TAG 85aa60d18b3d5e5588d7b247abf90198f07c8a63  # v9.5.0
)
set(LV_BUILD_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h" CACHE FILEPATH "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif()
FetchContent_GetProperties(lvgl)
if(NOT lvgl_POPULATED)
    FetchContent_Populate(lvgl)
    file(REMOVE "${lvgl_SOURCE_DIR}/src/draw/sw/blend/helium/lv_blend_helium.S")
    add_subdirectory("${lvgl_SOURCE_DIR}" "${lvgl_BINARY_DIR}")
endif()

# ---- LovyanGFX（SDL 后端源，参照 examples_for_PC/CMake_SDL） ----
set(LGFX_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../components/LovyanGFX/src")
file(GLOB LGFX_SOURCES CONFIGURE_DEPENDS
    "${LGFX_ROOT}/lgfx/Fonts/efont/*.c"
    "${LGFX_ROOT}/lgfx/Fonts/IPA/*.c"
    "${LGFX_ROOT}/lgfx/utility/*.c"
    "${LGFX_ROOT}/lgfx/v1/*.cpp"
    "${LGFX_ROOT}/lgfx/v1/misc/*.cpp"
    "${LGFX_ROOT}/lgfx/v1/panel/Panel_Device.cpp"
    "${LGFX_ROOT}/lgfx/v1/panel/Panel_FrameBufferBase.cpp"
    "${LGFX_ROOT}/lgfx/v1/platforms/sdl/*.cpp"
)

# ---- esp_logo.c 构建时生成（venv + LVGLImage.py，同 pc_simulator） ----
set(LVGL_PY_DEPS pypng lz4 Pillow)
set(LVGL_VENV_DIR "${CMAKE_CURRENT_BINARY_DIR}/.lvgl_venv")
set(LVGL_VENV_SENTINEL "${LVGL_VENV_DIR}/.deps_ok")
if(WIN32)
    set(LVGL_PYTHON "${LVGL_VENV_DIR}/Scripts/python.exe")
else()
    set(LVGL_PYTHON "${LVGL_VENV_DIR}/bin/python3")
endif()
if(NOT EXISTS "${LVGL_VENV_DIR}")
    execute_process(COMMAND "${Python3_EXECUTABLE}" -m venv "${LVGL_VENV_DIR}" RESULT_VARIABLE VENV_RESULT)
    if(NOT VENV_RESULT EQUAL 0)
        message(FATAL_ERROR "Python venv 创建失败")
    endif()
endif()
set(LVGL_VENV_DEPS_CACHED "")
if(EXISTS "${LVGL_VENV_SENTINEL}")
    file(READ "${LVGL_VENV_SENTINEL}" LVGL_VENV_DEPS_CACHED)
endif()
if(NOT LVGL_VENV_DEPS_CACHED STREQUAL "${LVGL_PY_DEPS}")
    execute_process(COMMAND "${LVGL_PYTHON}" -m pip install ${LVGL_PY_DEPS} --quiet RESULT_VARIABLE PIP_RESULT)
    if(NOT PIP_RESULT EQUAL 0)
        message(FATAL_ERROR "安装 Python 依赖失败（需访问 PyPI）")
    endif()
    file(WRITE "${LVGL_VENV_SENTINEL}" "${LVGL_PY_DEPS}")
endif()
set(ESP_LOGO_C "${CMAKE_CURRENT_BINARY_DIR}/esp_logo.c")
add_custom_command(
    OUTPUT "${ESP_LOGO_C}"
    COMMAND "${LVGL_PYTHON}" "${lvgl_SOURCE_DIR}/scripts/LVGLImage.py"
        --ofmt C --cf ARGB8888 --compress NONE --name esp_logo
        -o "${CMAKE_CURRENT_BINARY_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/../main/images/esp_logo.png"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/../main/images/esp_logo.png"
    COMMENT "从 esp_logo.png 生成 esp_logo.c"
    VERBATIM
)

# ---- 可执行：main + 共享 ui_app + 共享 bridge + LovyanGFX SDL 源 + esp_logo ----
add_executable(pc_simulator_lgfx
    "main.cpp"
    "../main/ui_app/ui_app.c"
    "../main/lvgl_lgfx/lvgl_lgfx_bridge.cpp"
    ${LGFX_SOURCES}
    "${ESP_LOGO_C}"
)
target_compile_definitions(pc_simulator_lgfx PRIVATE LGFX_SDL)
target_compile_features(pc_simulator_lgfx PUBLIC cxx_std_17)
target_include_directories(pc_simulator_lgfx PRIVATE
    "../main/ui_app"
    "../main/lvgl_lgfx"
    "${LGFX_ROOT}"
    ${SDL2_INCLUDE_DIRS}
)

if(TARGET SDL2::SDL2)
    set(PC_SIM_SDL2_LINK SDL2::SDL2)
else()
    set(PC_SIM_SDL2_LINK ${SDL2_LIBRARIES})
    target_include_directories(pc_simulator_lgfx PRIVATE ${SDL2_INCLUDE_DIRS})
endif()

# LVGL 关了 LV_USE_SDL，故 SDL2 只链给本可执行（供 LovyanGFX Panel_sdl 用）；Linux 需 pthread
target_link_libraries(pc_simulator_lgfx PRIVATE lvgl ${PC_SIM_SDL2_LINK})
if(CMAKE_SYSTEM_NAME MATCHES "Linux")
    target_link_libraries(pc_simulator_lgfx PRIVATE pthread)
endif()
