file(GLOB_RECURSE I2C_PORT_CPP_SOURCES ${CMAKE_SOURCE_DIR} "i2c_port/*.cpp")
file(GLOB_RECURSE POWER_CPP_SOURCES ${CMAKE_SOURCE_DIR} "power/*.cpp")
file(GLOB_RECURSE I2C_SCANNER_C_SOURCES ${CMAKE_SOURCE_DIR} "i2c_scanner/*.c")
file(GLOB_RECURSE I2C_DRIVER_CPP_SOURCES ${CMAKE_SOURCE_DIR} "i2c_driver/*.cpp")
file(GLOB_RECURSE SENSOR_DRV2605_CPP_SOURCES ${CMAKE_SOURCE_DIR} "sensor_drv2605/*.cpp")
file(GLOB_RECURSE SENSOR_BM8563_CPP_SOURCES ${CMAKE_SOURCE_DIR} "sensor_bm8563/*.cpp")
file(GLOB_RECURSE LVGL_C_SOURCES ${CMAKE_SOURCE_DIR} "lvgl/*.c")
file(GLOB_RECURSE I2C_APP_CPP_SOURCES ${CMAKE_SOURCE_DIR} "i2c_app/*.cpp")
file(GLOB_RECURSE LGFX_CPP_SOURCES ${CMAKE_SOURCE_DIR} "lgfx/*.cpp")
file(GLOB_RECURSE SD_CARD_C_SOURCES ${CMAKE_SOURCE_DIR} "sd_card/*.c")
file(GLOB_RECURSE LVGL_QEMU_RGB_C_SOURCES ${CMAKE_SOURCE_DIR} "lvgl_qemu_rgb/*.c")
file(GLOB_RECURSE FONTS_C_SOURCES ${CMAKE_SOURCE_DIR} "fonts/*.c")

idf_component_register(
    SRCS
    ${I2C_PORT_CPP_SOURCES}
    ${POWER_CPP_SOURCES}
    ${I2C_SCANNER_C_SOURCES}
    ${I2C_DRIVER_CPP_SOURCES}
    ${SENSOR_DRV2605_CPP_SOURCES}
    ${SENSOR_BM8563_CPP_SOURCES}
    ${LVGL_C_SOURCES}
    ${I2C_APP_CPP_SOURCES}
    ${LGFX_CPP_SOURCES}
    ${SD_CARD_C_SOURCES}
    ${LVGL_QEMU_RGB_C_SOURCES}
    ${FONTS_C_SOURCES}
    # "main.c"
    # "main_axp202.c"
    # "main_i2c_scanner.c"
    # "main_drv2605.c"
    # "main_bm8563.c"
    # "main_lvgl.c"
    # "main_lgfx.c"
    # "main_sd_card.c"
    # "main_lvgl_qemu_rgb.c"
    "main_lvgl_axp202.c"

    INCLUDE_DIRS
    "."
    "i2c_port"
    "power"
    "i2c_scanner"
    "i2c_driver"
    "sensor_drv2605"
    "sensor_bm8563"
    "lvgl"
    "i2c_app"
    "lgfx"
    "sd_card"
    "lvgl_qemu_rgb"
)

lvgl_port_create_c_image("images/esp_logo.png" "images/" "ARGB8888" "NONE")
lvgl_port_add_images(${COMPONENT_LIB} "images/")
