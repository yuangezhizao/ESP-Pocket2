file(GLOB_RECURSE I2C_PORT_CPP_SOURCES ${CMAKE_SOURCE_DIR} "i2c_port/*.cpp")
file(GLOB_RECURSE POWER_CPP_SOURCES ${CMAKE_SOURCE_DIR} "power/*.cpp")
file(GLOB_RECURSE I2C_SCANNER_C_SOURCES ${CMAKE_SOURCE_DIR} "i2c_scanner/*.c")
file(GLOB_RECURSE I2C_DRIVER_CPP_SOURCES ${CMAKE_SOURCE_DIR} "i2c_driver/*.cpp")

idf_component_register(
    SRCS
    ${I2C_PORT_CPP_SOURCES}
    ${POWER_CPP_SOURCES}
    ${I2C_SCANNER_C_SOURCES}
    ${I2C_DRIVER_CPP_SOURCES}
    # "main.c"
    # "main_axp202.c"
    "main_i2c_scanner.c"

    INCLUDE_DIRS
    "."
    "i2c_port"
    "power"
    "i2c_scanner"
    "i2c_driver"
)
