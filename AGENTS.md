# AGENTS

## 交互约定

- 与本仓库交互时，请始终使用中文回复（代码、路径、命令除外）。

## Cursor Cloud specific instructions

> 说明：保留该英文章节标题作为工具约定锚点；下方内容使用中文。

本仓库是 **ESP-Pocket2**，一个面向 **ESP32-S3** 芯片的 ESP-IDF（v5.5.4）固件项目。没有 host 应用或 web 服务——构建产物是运行在 ESP32-S3（或 Espressif QEMU 模拟器）上的固件。

### Cloud Agent 环境（Dockerfile 模式，配置即代码）

- 环境由仓库内 **`.cursor/environment.json` + `.cursor/Dockerfile`** 定义，基于官方镜像 **`espressif/idf:v5.5.4`**（Dockerfile 中 tag+digest 双锁定），不依赖任何个人快照。
- 解析优先级：仓库 `.cursor/environment.json` > 个人 saved environment > 团队 saved environment。因此**从带本配置的分支起 Cloud Agent 会自动使用本 Dockerfile**：无需在 dashboard 手动创建环境（那条路径才依赖 GitHub 关联向导），也无需删除已有的个人快照（它会被更高优先级的仓库配置覆盖，仅作 fallback）。
- ESP-IDF 位于 **`/opt/esp/idf`**（`IDF_PATH`），工具链在 `/opt/esp`。`export.sh` 已在镜像的 `/etc/bash.bashrc` 自动 source，新 shell 可直接用 `idf.py`；若某个 shell 没有该命令，运行 `source /opt/esp/idf/export.sh`（或官方 alias `get_idf`）。
- 构建目标 `esp32s3` 由 `sdkconfig.defaults`（`CONFIG_IDF_TARGET="esp32s3"`）**声明式固定**：全新环境首次 `idf.py build` 会自动选中 esp32s3，**无需手动 `set-target`**。实际 `sdkconfig` 由其生成（首次构建/`reconfigure` 时产生），不应提交到仓库（注意勿用 `git add -A` 误加入）。`idf.py set-target esp32s3` 只在需要纠正/切换一个「已存在且目标错误」的 `sdkconfig` 时才用（Cloud 全新环境不会遇到）。
- 第三方驱动（`XPowersLib`、`SensorLib`、`LovyanGFX`）是 `components/` 下的 **git submodule**；`environment.json` 的 `install`（每次启动运行的 update 命令）会执行 `git submodule update --init --recursive` 拉齐它们。
- managed components（以 `main/idf_component.yml` 为准）会在 `set-target`/`reconfigure`/`build` 时自动拉取到 `managed_components/`。
- 未来若需 Tailscale / cloudflared：直接在 `.cursor/Dockerfile` 里安装（或运行时装），按官方 userspace 方式启动即可——环境本身就是容器，无需 docker-in-docker。

### 构建 / 运行 / 体积

标准命令（另见 `.github/workflows/build-esp-idf-project.yml`）：

- 构建：`idf.py build`
- 体积报告：`idf.py size` / `idf.py size-components`
- 无硬件运行（模拟器）：`idf.py qemu --qemu-extra-args "-no-reboot"`
  - 官方镜像已内置 `qemu-system-xtensa`（用户态网络依赖 `libslirp0` 亦已装）。
  - QEMU **没有真实外设**：SD 卡、I2C 传感器（AXP202/BM8563/DRV2605）、GT911 触摸屏和显示屏都不存在。因此：
    - I2C 总线扫描（`i2c_drv_scan`）会打印大量 `i2c_master_probe ... i2c handle not initialized` 错误；
    - 若当前启用 SD 卡 demo（如 `main_sd_card.c`），会因无真实 SD 卡而阻塞在 `Mounting filesystem`。
    这些是模拟器缺少硬件的**预期现象**，**不是**环境故障。能启动到 `main_task: Calling app_main()` 及应用自身初始化日志即可证明构建可用。
  - `idf.py qemu` 是交互式的（串口接当前终端）；做非交互冒烟测试时用 `timeout` 包裹，例如 `timeout 35 idf.py qemu --qemu-extra-args "-no-reboot"`。
  - **桌面图形演示**：`idf.py qemu --graphics` 会开 SDL 窗口，图形输出走 `DISPLAY`。Desktop/XFCE/TigerVNC 一般由 Cursor 平台在启动时自动提供（对应 noVNC 面板，通常是 `DISPLAY=:1`）；当前 shell 的 `DISPLAY` 可能为空，运行前先设置，即 `DISPLAY=:1 idf.py qemu --graphics`。若 `:1` 不存在，用 `ls /tmp/.X11-unix/`（如 `X1` 对应 `:1`）或 `echo $DISPLAY` 确认实际编号。若平台未提供 X server/桌面包，再按需在 `.cursor/Dockerfile` 中补装缺失项。图形模式下按 `Ctrl-a` 再按 `x` 终止 QEMU。

### PC 模拟（纯 LVGL SDL，免烧录看 UI）

`pc_simulator/` 是复用 `main/ui_app/` 的纯 PC LVGL 模拟工程（SDL2，不经 ESP-IDF/QEMU），用于快速迭代 hp28008 的 LVGL 界面，并支持鼠标模拟触摸。构建运行见 `pc_simulator/README.md`。注意：它只渲染 UI、不跑真实固件/驱动；需要真实固件行为请用 QEMU 或真机。

### 选择运行哪个 demo

实际生效的 `app_main` 由 `main/CMakeLists.txt` 中 `SRCS` 列表里**未被注释**的那个 `main_*.c` 决定；其余 `main_*.c` 以注释形式并列在同一处作为备选（完整清单以 `main/CMakeLists.txt` 为准，新增/删减例程时无需回来改本文件）。切换 demo 只需在该 `SRCS` 中注释/取消注释对应文件并重新构建。

### 测试 / lint

本仓库没有单元测试或 lint 配置。CI 仅构建固件，并输出 `idf.py size` 与 `esptool image_info`。
