# pc_simulator_lgfx — hp28008 LVGL 效果 PC 模拟（LovyanGFX 后端变体）

本工程是 `pc_simulator/` 的 **LovyanGFX 后端变体**：显示与触摸经 LovyanGFX `Panel_sdl` + 共享的 `main/lvgl_lgfx/lvgl_lgfx_bridge`，用于免烧录预览 hp28008（ST7789，240×320）的 LVGL 界面，并支持鼠标模拟触摸。它与 `pc_simulator/`（纯 `lv_sdl` 后端）并列，二者都渲染同一份 `main/ui_app/`；不同的是本工程走真机 `main_lvgl_lgfx.c` 所用的**同一份 bridge 代码路径**（PC 与真机共用 `lvgl_lgfx_bridge_create` 的 `flush_cb`/`read_cb`），因此能在 PC 上验证真机那条 LovyanGFX 显示/触摸链路。**不经 ESP-IDF / QEMU**，不跑真实固件或驱动。

## 依赖

- **Linux（构建 / 本地运行）**：`sudo apt-get install -y libsdl2-dev cmake build-essential python3 python3-venv`
- **LovyanGFX**：本仓库 submodule（`components/LovyanGFX`），其 SDL 后端源码直接参与编译；克隆仓库后需先 `git submodule update --init --recursive` 确保其存在。
- **LVGL v9.5.0**：由 `CMakeLists.txt` 的 `FetchContent` 从 GitHub 自动拉取，无需手动安装（首次 configure 需能访问 GitHub）。
- **Python3**：用于构建时生成 `esp_logo.c`；`cmake` 配置阶段会自动创建虚拟环境并安装所需依赖（`pypng` + `lz4` + `Pillow`），无需手动 pip install（首次 configure 需能访问 PyPI）。
- **Cloud / 无交互截图验证（可选）**：另需 `ffmpeg`（`sudo apt-get install -y ffmpeg`）用于无显示器环境下抓取初始 UI 截图核对；`xdotool` 仅在想脚本化点击「Rotate screen」时可选，不纳入必做验证。

## 构建与运行

```bash
# 首次构建，或 CMakeLists.txt / lv_conf.h 有变更时，需先删除旧 build：
rm -rf pc_simulator_lgfx/build
cmake -S pc_simulator_lgfx -B pc_simulator_lgfx/build
cmake --build pc_simulator_lgfx/build --parallel
./pc_simulator_lgfx/build/pc_simulator_lgfx
```

首次 configure 需联网（GitHub 拉取 LVGL v9.5.0、PyPI 安装 `pypng`/`lz4`/`Pillow`）；若网络受限请预先配置 Git/pip 代理或镜像。`CMakeLists.txt` 会自动删除 LVGL 中不兼容 x86 的 ARM Helium 汇编（`lv_blend_helium.S`），这是正常行为。

**本地（有显示器、人工）**：前台直接运行上面的可执行，会弹出 240×320 窗口显示 `ui_app`（logo / 欢迎语 / 时钟），鼠标点「Rotate screen」按钮可旋转（0° → 90° → 180° → 270° → 0°），关窗即结束进程。`lgfx::Panel_sdl::main()` 是前台阻塞循环，跑到所有窗口关闭才返回。

**Cloud / 无交互（仅验证初始 UI 渲染）**：`Panel_sdl::main()` 前台阻塞，故不能直接前台跑（会挂起）；改为后台启动 → 等待 → 全屏截图 → 结束进程，用 `ffmpeg` 核对初始画面。旋转按钮交互不在此路径覆盖，由本地人工验证。

```bash
DISPLAY_ID="${DISPLAY:-:1}"   # Cloud 通常 :1；可先 ls /tmp/.X11-unix/ 核对（X1 → :1）
DISPLAY="$DISPLAY_ID" ./pc_simulator_lgfx/build/pc_simulator_lgfx &
APP_PID=$!
trap 'kill "$APP_PID" 2>/dev/null || true' EXIT   # 退出时回收后台模拟器，避免残留进程
sleep 3
timeout 10s ffmpeg -f x11grab -i "$DISPLAY_ID" -frames:v 1 -y /tmp/shot.png   # 全屏抓取，自动取整个 display 尺寸
kill "$APP_PID" 2>/dev/null || true
wait "$APP_PID" 2>/dev/null || true
```

核对 `/tmp/shot.png` 中是否包含预期 UI（logo / 欢迎语 / 时钟）。

## 交互

| 操作 | 效果 |
|---|---|
| 鼠标左键点击 / 拖拽 | 等效于触摸屏触摸 |
| 点击「Rotate screen」按钮 | 旋转屏幕（0° → 90° → 180° → 270° → 0°） |

## 与 pc_simulator 的区别

二者都是复用 `main/ui_app/` 的 PC LVGL(SDL2) 模拟，区别只在**显示/触摸后端**：`pc_simulator` 用 LVGL 自带 SDL 后端（`LV_USE_SDL=1`），`pc_simulator_lgfx` 用 LovyanGFX `Panel_sdl` 并走与真机相同的 `lvgl_lgfx_bridge`（`LV_USE_SDL=0`）。

| 方面 | `pc_simulator`（纯 `lv_sdl`） | `pc_simulator_lgfx`（LovyanGFX 后端） |
|---|---|---|
| 显示后端 | LVGL 自带 SDL 后端（`LV_USE_SDL=1`） | LovyanGFX `Panel_sdl`（`LV_USE_SDL=0`，显示走 LGFX） |
| flush / 触摸路径 | LVGL SDL 驱动 + `lv_sdl_mouse_create` | 共享 `lvgl_lgfx_bridge` 的 `flush_cb`（`pushImageDMA` + 旋转跟随）/ `read_cb`（`lcd.getTouch()`），与真机同一份 |
| 对应真机入口 | `main_lvgl.c`（esp_lcd 驱动 ST7789） | `main_lvgl_lgfx.c`（LovyanGFX 驱动 ST7789 + esp_lcd_touch 触摸 + BM8563 RTC） |
| UI 源码 | 共享 `main/ui_app/` | 共享 `main/ui_app/` |
| 时钟数据源 | 本机时间（`localtime_r`） | 本机时间（`localtime_r`） |

一句话：`pc_simulator` 侧重快速迭代 UI，`pc_simulator_lgfx` 额外验证真机那条 LovyanGFX + bridge 的显示/触摸代码路径。

## 局限

- 仅渲染 UI，不跑真实固件或驱动；需真实固件行为（如 I2C 外设、LEDC 背光、真实 GT911/BM8563）请使用 QEMU 或烧录到真机。
- 时钟显示本机时间（`pc_clock_source`），非真实 BM8563 RTC。
- 旋转在 PC 上是纯软件坐标映射（`Panel_sdl` 写 framebuffer），**不代表真机性能**——真机旋转省 CPU 的红利来自 ST7789 的 MADCTL 硬件旋转，PC 无此硬件（详见 spec 第 8.3 节）。
- 时钟源用 POSIX `localtime_r`，暂未适配 Windows（需改 `localtime_s` 或条件编译后方可在 Windows 构建）。
