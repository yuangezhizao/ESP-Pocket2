# pc_simulator — hp28008 LVGL 效果 PC 模拟（方案 A）

复用 `main/ui_app/` 可移植 UI 层的纯 PC LVGL(SDL2) 模拟工程，用于免烧录预览 hp28008（ST7789，240x320）的 LVGL 界面，并支持鼠标模拟触摸。**不经 ESP-IDF / QEMU**，不跑真实固件或驱动。

## 依赖

- **Linux**：`sudo apt-get install -y libsdl2-dev cmake build-essential python3 python3-venv`
- **macOS**：`brew install sdl2 cmake`
- **Python3**：用于构建时生成 `esp_logo.c`；`cmake` 配置阶段会自动创建虚拟环境并安装所需依赖（`pypng` + `lz4` + `Pillow`），**无需手动 pip install**（首次 configure 需能访问 PyPI；若网络受限请先配置 pip 镜像/代理）
- **LVGL v9.5.0**：由 `CMakeLists.txt` 的 `FetchContent` 从 GitHub 自动拉取，无需手动安装（首次 configure 需能访问 GitHub；若网络受限请配置 Git 代理或预先缓存）

## 构建与运行

```bash
# 首次构建，或 CMakeLists.txt / lv_conf.h 有变更时，需先删除旧 build：
rm -rf pc_simulator/build
cmake -S pc_simulator -B pc_simulator/build
cmake --build pc_simulator/build --parallel
./pc_simulator/build/pc_simulator
```

> 注意：`CMakeLists.txt` 会自动删除 LVGL 中不兼容 x86 的 ARM Helium 汇编（`lv_blend_helium.S`），这是正常行为。

## 交互

| 操作 | 效果 |
|---|---|
| 鼠标左键点击 / 拖拽 | 等效于触摸屏触摸 |
| 点击 "Rotate screen" 按钮 | 旋转屏幕（0° → 90° → 180° → 270° → 0°） |

## 与固件的关系

| 方面 | PC 模拟 | 真机 |
|---|---|---|
| UI 源码 | 共享 `main/ui_app/` | 共享 `main/ui_app/` |
| 时钟数据源 | 本机时间（`pc_clock_source`） | BM8563 RTC（`real_clock_source`） |
| 渲染后端 | SDL2 窗口（无 ESP） | `esp_lcd` ST7789（SPI） |
| 触摸支持 | 鼠标模拟（`lv_sdl_mouse_create`） | GT911 真实触摸（I2C） |

改动 `main/ui_app/` 中的 UI 代码后，重新构建本工程即可在 PC 上立即预览，无需烧录。

## 分辨率

窗口尺寸取 `main/ui_app/ui_app.h` 的 `UI_APP_HOR_RES` / `UI_APP_VER_RES`（240 × 320），须与真机 `main/lvgl/hp28008.h` 的 `EXAMPLE_LCD_H_RES/V_RES` 保持同值（两处定义、约定同值，见 `ui_app.h` 注释）。

## esp_logo 资源

`esp_logo.c` 在**构建时自动生成**（不入库）：`CMakeLists.txt` 调用 LVGL 自带的 `scripts/LVGLImage.py`，从 `main/images/esp_logo.png` 转换为 ARGB8888 格式的 LVGL v9 C array（96 × 96）。格式与固件侧 `lvgl_port_create_c_image(... "ARGB8888" ...)` 相同。若源图更新，重新构建即可自动重新生成。

## 平台说明

主要在 **Linux / macOS**（含云端 `DISPLAY=:1`）验证；`main.c` 含 `#define SDL_MAIN_HANDLED`（SDL 官方示例惯例）。当前时钟源用 POSIX `localtime_r`，暂未适配 Windows（需改 `localtime_s` 或条件编译后方可构建）。

## 截图（可选，云端）

```bash
DISPLAY=:1 ./pc_simulator/build/pc_simulator &
sleep 2
ffmpeg -f x11grab -i :1 -frames:v 1 -y shot.png
```

## 局限

- 仅渲染 UI，不跑真实固件或驱动
- 时钟显示本机时间，非真实 BM8563 RTC
- 需真实固件行为（如 I2C 外设、LEDC 背光）请使用 QEMU 或烧录到真机
