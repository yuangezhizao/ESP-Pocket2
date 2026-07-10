# pc_simulator_lgfx — hp28008 LVGL 效果 PC 模拟（LovyanGFX/SDL 后端变体）

`pc_simulator/`（方案 A，纯 `lv_sdl`）的 **LovyanGFX 后端变体**：显示与触摸经 LovyanGFX 的 `Panel_sdl` 后端，并通过与真机**共享**的 `main/lvgl_lgfx/lvgl_lgfx_bridge` 驱动同一份 `main/ui_app/` UI（logo + 欢迎语 + 时钟 + 旋转按钮），鼠标模拟触摸。真机对应入口是 `main_lvgl_lgfx.c`（LGFX 驱动 HP28008 显示、`esp_lcd_touch` 触摸、BM8563 RTC），二者跑**同一份 bridge 代码路径**，因此本工程可在 PC 上免烧录验证该 bridge。**不经 ESP-IDF / QEMU**，不跑真实固件或驱动。

与 `pc_simulator/` 并列保留：二者都渲染同一份 `main/ui_app/`，区别仅在显示后端——`pc_simulator/` 走 LVGL 自带的 `lv_sdl`，本工程走 LovyanGFX `Panel_sdl`（故本工程 `lv_conf.h` 中 `LV_USE_SDL 0`）。

## 依赖

- **Linux（构建 / 本地运行）**：`sudo apt-get install -y libsdl2-dev cmake build-essential python3 python3-venv`
- **Cloud 截图验证**：需 `ffmpeg`（cloud 平台 Aptfile 已预装；本地需自行 `apt-get install ffmpeg`）；`xdotool` 用于 cloud 自动化模拟点击验证旋转（cloud 亦已预装）
- **LovyanGFX**：仓库 `components/` 下的 git submodule，首次需 `git submodule update --init --recursive`
- **LVGL v9.5.0**：由 `CMakeLists.txt` 的 `FetchContent` 从 GitHub 自动拉取，无需手动安装（首次 configure 需能访问 GitHub）
- **Python3**：构建时生成 `esp_logo.c`；`cmake` 配置阶段自动创建虚拟环境并安装 `pypng` + `lz4` + `Pillow`（首次 configure 需能访问 PyPI）

## 构建与运行

```bash
# 首次构建，或 CMakeLists.txt / lv_conf.h 有变更时，需先删除旧 build：
rm -rf pc_simulator_lgfx/build
cmake -S pc_simulator_lgfx -B pc_simulator_lgfx/build
cmake --build pc_simulator_lgfx/build --parallel
./pc_simulator_lgfx/build/pc_simulator_lgfx
```

> `CMakeLists.txt` 会自动删除 LVGL 中不兼容 x86 的 ARM Helium 汇编（`lv_blend_helium.S`），属正常行为。

`lgfx::Panel_sdl::main()` 是前台阻塞循环，跑到所有窗口关闭才返回；本地有显示器时直接前台运行即可，关窗即结束。

### Cloud / 纯 ffmpeg 截图（仅初始 UI 渲染）

```bash
DISPLAY=:1 ./pc_simulator_lgfx/build/pc_simulator_lgfx &
sleep 3
ffmpeg -f x11grab -i :1 -frames:v 1 -y shot.png
```

> 此纯 ffmpeg 截图路径**仅**核对初始（rotation-0）UI；"Rotate screen" 旋转交互改用 `xdotool` 模拟点击验证——本 PR 已在 cloud 实测四方向闭环通过（0→90→180→270→0，方法见 PR 正文 / plan Task 6）。

## 交互

| 操作 | 效果 |
|---|---|
| 鼠标左键点击 / 拖拽 | 等效于触摸屏触摸 |
| 点击 "Rotate screen" 按钮 | 旋转屏幕（0° → 90° → 180° → 270° → 0°） |

## 与 pc_simulator / 真机的关系

| 方面 | pc_simulator_lgfx（本工程） | pc_simulator（方案 A） | 真机（main_lvgl_lgfx） |
|---|---|---|---|
| UI 源码 | 共享 `main/ui_app/` | 共享 `main/ui_app/` | 共享 `main/ui_app/` |
| 桥接层 | 共享 `lvgl_lgfx_bridge` | 无（直接用 `lv_sdl`） | 共享 `lvgl_lgfx_bridge` |
| 显示后端 | LovyanGFX `Panel_sdl`（SDL2 窗口） | LVGL `lv_sdl`（SDL2 窗口） | LGFX ST7789（SPI） |
| 触摸支持 | 鼠标模拟（`lcd.getTouchRaw`） | 鼠标模拟（`lv_sdl_mouse`） | GT911 真实触摸（I2C，`esp_lcd_touch`） |
| 时钟数据源 | 本机时间（`pc_clock_source`） | 本机时间 | BM8563 RTC（`real_clock_source`） |

## 局限

- 仅渲染 UI，不跑真实固件或驱动
- 时钟显示本机时间，非真实 BM8563 RTC
- 旋转在 PC 上是软件坐标映射（见设计文档第 8.3 节），不代表真机 ST7789 MADCTL 硬件旋转的性能
- 需真实固件行为（如真实 I2C 外设、SPI 显示）请使用 QEMU 或烧录到真机
