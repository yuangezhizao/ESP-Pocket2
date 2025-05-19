file(GLOB_RECURSE I2C_CPP_SOURCES ${CMAKE_SOURCE_DIR} "i2c/*.cpp")
file(GLOB_RECURSE POWER_CPP_SOURCES ${CMAKE_SOURCE_DIR} "power/*.cpp")

idf_component_register(
    SRCS
    ${I2C_CPP_SOURCES}
    ${POWER_CPP_SOURCES}
    # "main.c"
    "main_axp202.c"

    INCLUDE_DIRS
    "."
    "i2c"
    "power"
)
