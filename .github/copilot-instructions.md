# ESP-Pocket2 AI Coding Assistant Instructions

## Project Overview
ESP-Pocket2 is an ESP-IDF based embedded system project targeting ESP32-C3 and ESP32-S3 chips. It's a multi-component testing platform that integrates display (LovyanGFX/LVGL), sensor libraries (SensorLib), and power management (XPowersLib) for embedded device development.

## Architecture & Component Structure

### Core Components
- **LovyanGFX**: Primary display driver library (components/LovyanGFX) - handles LCD/OLED displays with SPI/I2C
- **LVGL**: GUI framework via managed components (esp_lvgl_port, lvgl/lvgl v9.2.2)
- **SensorLib**: Sensor driver collection for various I2C sensors
- **XPowersLib**: Power management library (AXP202, etc.)

### Main Application Structure
The main component (`main/`) uses a modular approach with separate directories for each functional area:
- `i2c_*`: I2C communication layers (driver, scanner, port, app)
- `sensor_*`: Sensor-specific implementations (BM8563 RTC, DRV2605 haptic)
- `lgfx/`: LovyanGFX display implementation
- `lvgl/`: LVGL GUI implementation
- `power/`: Power management integration

### Build System Patterns
- Uses ESP-IDF CMake build system (v5.3.0+)
- Multiple main files for different demos: `main_lgfx.c`, `main_lvgl.c`, `main_axp202.c`, etc.
- Currently builds `main_lgfx.c` (see `main/CMakeLists.txt` line 32)
- Supports both ESP32-C3 (4MB flash) and ESP32-S3 (16MB flash, PSRAM enabled)

## Development Workflows

### ESP-IDF Environment Setup
This project uses ESP-IDF v5.5.1. Before building or flashing, activate the ESP-IDF environment:
```bash
# Activate ESP-IDF v5.5.1 environment
. ~/esp/v5.5.1/esp-idf/export.sh
```

### Target Selection
Switch between chip targets using sdkconfig files:
```bash
# For ESP32-C3
cp sdkconfig.defaults.esp32c3 sdkconfig.defaults
idf.py set-target esp32c3

# For ESP32-S3  
cp sdkconfig.defaults.esp32s3 sdkconfig.defaults
idf.py set-target esp32s3
```

### Building & Flashing
```bash
idf.py build                    # Build project
idf.py flash monitor           # Flash and monitor
idf.py menuconfig              # Configure project settings
```

### Switching Demo Applications
Edit `main/CMakeLists.txt` line 32 to change which main file builds:
- `"main_lgfx.c"` - LovyanGFX display test
- `"main_lvgl.c"` - LVGL GUI with sensors
- `"main_i2c_scanner.c"` - I2C device scanner
- `"main_axp202.c"` - Power management demo

## Project-Specific Conventions

### Mixed C/C++ Architecture
- Main application entry points are C files (`main_*.c`)
- Driver implementations use C++ (`.cpp` files in subdirectories)
- C++ components expose C-compatible headers for integration

### I2C Communication Patterns
- Multiple I2C abstraction layers: `i2c_driver`, `i2c_port`, `i2c_app`
- Uses conditional compilation for different I2C methods (`CONFIG_I2C_COMMUNICATION_METHOD_*`)
- Standard device scanning pattern in `i2c_scanner/`

### Display Integration
- LovyanGFX uses auto-detection (`LGFX_AUTODETECT`) for hardware compatibility
- LVGL port integration handles display buffer management and touch input
- Image assets processed via `lvgl_port_create_c_image()` in CMake

### Component Dependencies
- Managed components handle external dependencies (LVGL, ESP LCD touch drivers)
- Local components in `components/` are Git submodules or custom libraries
- Use `idf_component.yml` for version-pinned ESP-IDF component dependencies

## Key Integration Points

### Hardware Abstraction
- Display: LovyanGFX → LVGL port → Application
- Sensors: SensorLib → I2C drivers → Application tasks  
- Power: XPowersLib → AXP202 port → System management

### Memory Management
- ESP32-S3 builds enable PSRAM for large display buffers
- Partition table configured for 4MB factory app (see `partitions.csv`)
- Heap tracking enabled in S3 configuration for debugging

## Critical Files for Understanding
- `main/CMakeLists.txt` - Component registration and build configuration
- `main/idf_component.yml` - External dependency versions
- `sdkconfig.defaults.esp32*` - Target-specific hardware configurations
- `main/main_lvgl.c` - Full integration example with all subsystems
- `main/lgfx/lgfx.h` - Display hardware abstraction interface

When working with this codebase, prioritize understanding the component integration patterns and the build system's multi-target approach before diving into specific implementations.