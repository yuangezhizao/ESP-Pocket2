# QEMU 虚拟 RGB 面板 LVGL 例程 Implementation Plan

> **For agentic workers:** Task tool 未指定 model 参数时 subagent 默认继承父模型，子模型与父模型一致性现已可保证；本实现当时采用 superpowers:executing-plans（inline 批处理 + 检查点），后续任务可改用 superpowers:subagent-driven-development。步骤用 `- [x]` 复选框跟踪。

**Goal:** 新增一个仅用于 QEMU 的 LVGL 例程，经 `espressif/esp_lcd_qemu_rgb` 虚拟 RGB 帧缓冲，用 `idf.py qemu --graphics` 在 PC 上直接看到 LVGL 画面，无需烧录开发板。

**Architecture:** 独立入口 `main/main_lvgl_qemu_rgb.c` + 实现文件夹 `main/lvgl_qemu_rgb/`（自管 LVGL v9：建 QEMU RGB panel、注册 flush、esp_timer tick、LVGL task）。照搬官方 `lcd_qemu_rgb_panel` 结构并把 LVGL v8 裸 `lv_disp_drv` API 适配为项目使用的 v9。所有导出符号加 `qemu_rgb_` 前缀，避免与 GLOB 一同编译的 `hp28008.c` 全局符号冲突。

**Tech Stack:** ESP-IDF v5.5.4 (esp32s3)、LVGL v9.5.0、`espressif/esp_lcd_qemu_rgb` v1.0.2、`esp_lcd`、`esp_timer`、FreeRTOS、QEMU（qemu-system-xtensa）。

**参考：**
- 官方例程 https://github.com/espressif/idf-extra-components/tree/master/esp_lcd_qemu_rgb/examples/lcd_qemu_rgb_panel
- 官方文档 https://docs.espressif.com/projects/esp-idf/zh_CN/v5.5.4/esp32s3/api-guides/tools/qemu.html
- 设计文档 [docs/superpowers/specs/2026-07-01-lvgl-qemu-rgb-design.md](https://github.com/yuangezhizao/ESP-Pocket2/blob/dev/docs/superpowers/specs/2026-07-01-lvgl-qemu-rgb-design.md)

---

## Task 1: 引入 esp_lcd_qemu_rgb 依赖

**Files:**
- Modify: `main/idf_component.yml`

- [x] **Step 1: 在 `lvgl/lvgl: "^9.5.0"` 后追加一行**

在 `lvgl/lvgl: "^9.5.0"` 后追加：

```yaml
  esp_lcd_qemu_rgb: "^1.0.2"
```

- [x] **Step 2: 提交**

```bash
git add main/idf_component.yml
git commit -m "$(cat <<'EOF'
⬆️ Chore(main/idf_component.yml): 引入 esp_lcd_qemu_rgb 依赖用于 QEMU 虚拟 RGB 面板

idf_component.yml 增加 esp_lcd_qemu_rgb: "^1.0.2"（idf-extra-components 官方组件，为 QEMU 虚拟帧缓冲提供 esp_lcd 兼容驱动）

参照:
- https://components.espressif.com/components/espressif/esp_lcd_qemu_rgb
EOF
)"
```

---

## Task 2: 新增例程源文件（core + ui + entry）

**Files:**
- Create: `main/lvgl_qemu_rgb/lvgl_qemu_rgb.h`
- Create: `main/lvgl_qemu_rgb/lvgl_qemu_rgb.c`
- Create: `main/lvgl_qemu_rgb/lvgl_demo_ui.c`
- Create: `main/main_lvgl_qemu_rgb.c`

- [x] **Step 1: 写头文件 `main/lvgl_qemu_rgb/lvgl_qemu_rgb.h`**

```c
/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 * SPDX-FileContributor: 2026 ESP-Pocket2
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * 移植自官方例程 esp_lcd_qemu_rgb/examples/lcd_qemu_rgb_panel（LVGL v8 -> v9 适配）。
 */
#pragma once

#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 在 QEMU 虚拟 RGB 面板上启动 LVGL 例程
 *
 * 建面板 + 自管 LVGL（draw buffer / esp_timer tick / LVGL task）+ 显示 UI。
 * 仅在 QEMU 中可用：真实硬件上 esp_lcd_new_rgb_qemu 会返回 ESP_ERR_NOT_SUPPORTED。
 */
esp_err_t qemu_rgb_lvgl_run(void);

/**
 * @brief UI 入口（定义在 lvgl_demo_ui.c），由 qemu_rgb_lvgl_run 在持锁状态下调用
 */
void qemu_rgb_lvgl_demo_ui(lv_display_t *disp);

#ifdef __cplusplus
}
#endif
```

- [x] **Step 2: 写核心实现 `main/lvgl_qemu_rgb/lvgl_qemu_rgb.c`**

```c
/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 * SPDX-FileContributor: 2026 ESP-Pocket2
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * 移植自官方例程 lcd_qemu_rgb_panel，将 LVGL v8 裸 lv_disp_drv API 适配为项目使用的 v9。
 */
#include <stdlib.h>
#include <assert.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_qemu_rgb.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_log.h"
#include "lvgl.h"

#include "lvgl_qemu_rgb.h"

static const char *TAG = "qemu_rgb";

/* QEMU 虚拟 RGB 面板尺寸（可自定义；沿用官方默认） */
#define QEMU_LCD_H_RES              (800)
#define QEMU_LCD_V_RES              (480)
/* 分离式 draw buffer 行数（内部 RAM） */
#define QEMU_LVGL_BUF_LINES         (10)

#define QEMU_LVGL_TICK_PERIOD_MS    (2)
#define QEMU_LVGL_TASK_MAX_DELAY_MS (500)
#define QEMU_LVGL_TASK_MIN_DELAY_MS (1)
#define QEMU_LVGL_TASK_STACK_SIZE   (4 * 1024)
#define QEMU_LVGL_TASK_PRIORITY     (2)

static SemaphoreHandle_t s_lvgl_mux = NULL;

static void qemu_rgb_lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    esp_lcd_panel_handle_t panel = (esp_lcd_panel_handle_t)lv_display_get_user_data(disp);
    /* QEMU 面板直接写虚拟帧缓冲，无需字节交换（区别于给 SPI 用的 swap_bytes） */
    esp_lcd_panel_draw_bitmap(panel, area->x1, area->y1, area->x2 + 1, area->y2 + 1, px_map);
    lv_display_flush_ready(disp);
}

static void qemu_rgb_increase_lvgl_tick(void *arg)
{
    lv_tick_inc(QEMU_LVGL_TICK_PERIOD_MS);
}

static bool qemu_rgb_lvgl_lock(int timeout_ms)
{
    const TickType_t ticks = (timeout_ms == -1) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTakeRecursive(s_lvgl_mux, ticks) == pdTRUE;
}

static void qemu_rgb_lvgl_unlock(void)
{
    xSemaphoreGiveRecursive(s_lvgl_mux);
}

static void qemu_rgb_lvgl_task(void *arg)
{
    ESP_LOGI(TAG, "Starting LVGL task");
    uint32_t task_delay_ms = QEMU_LVGL_TASK_MAX_DELAY_MS;
    while (1) {
        if (qemu_rgb_lvgl_lock(-1)) {
            task_delay_ms = lv_timer_handler();
            qemu_rgb_lvgl_unlock();
        }
        if (task_delay_ms > QEMU_LVGL_TASK_MAX_DELAY_MS) {
            task_delay_ms = QEMU_LVGL_TASK_MAX_DELAY_MS;
        } else if (task_delay_ms < QEMU_LVGL_TASK_MIN_DELAY_MS) {
            task_delay_ms = QEMU_LVGL_TASK_MIN_DELAY_MS;
        }
        vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
    }
}

esp_err_t qemu_rgb_lvgl_run(void)
{
    ESP_LOGI(TAG, "Install QEMU RGB LCD panel driver");
    esp_lcd_panel_handle_t panel_handle = NULL;
    const esp_lcd_rgb_qemu_config_t panel_config = {
        .width = QEMU_LCD_H_RES,
        .height = QEMU_LCD_V_RES,
        .bpp = RGB_QEMU_BPP_16,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_rgb_qemu(&panel_config, &panel_handle), TAG,
                        "esp_lcd_new_rgb_qemu failed (this example only runs in QEMU)");

    ESP_LOGI(TAG, "Initialize QEMU RGB LCD panel");
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));

    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();

    lv_display_t *disp = lv_display_create(QEMU_LCD_H_RES, QEMU_LCD_V_RES);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_user_data(disp, panel_handle);
    lv_display_set_flush_cb(disp, qemu_rgb_lvgl_flush_cb);

    ESP_LOGI(TAG, "Allocate separate LVGL draw buffer");
    const size_t buf_size = QEMU_LCD_H_RES * QEMU_LVGL_BUF_LINES * sizeof(lv_color16_t); /* RGB565 = 2B/px */
    void *buf1 = malloc(buf_size);
    assert(buf1);
    lv_display_set_buffers(disp, buf1, NULL, buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);

    ESP_LOGI(TAG, "Install LVGL tick timer");
    const esp_timer_create_args_t tick_timer_args = {
        .callback = &qemu_rgb_increase_lvgl_tick,
        .name = "lvgl_tick",
    };
    esp_timer_handle_t tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&tick_timer_args, &tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(tick_timer, QEMU_LVGL_TICK_PERIOD_MS * 1000));

    s_lvgl_mux = xSemaphoreCreateRecursiveMutex();
    assert(s_lvgl_mux);

    ESP_LOGI(TAG, "Create LVGL task");
    xTaskCreate(qemu_rgb_lvgl_task, "LVGL", QEMU_LVGL_TASK_STACK_SIZE, NULL, QEMU_LVGL_TASK_PRIORITY, NULL);

    ESP_LOGI(TAG, "Display LVGL demo UI");
    if (qemu_rgb_lvgl_lock(-1)) {
        qemu_rgb_lvgl_demo_ui(disp);
        qemu_rgb_lvgl_unlock();
    }
    return ESP_OK;
}
```

> ⚠️ 更正（2026-07-04，复审 PR3-1/PR3-3）：上面 Step 2 的代码是 PR3 首版，有两个已知问题：
>
> 1. Partial 分支用 `malloc`，在开 PSRAM 且 buffer 实际落 PSRAM 时会被 QEMU esp_rgb 拒绝而黑屏。
> 2. 面板 `.bpp` 与 `lv_display_set_color_format`、每像素字节硬编码 RGB565、不随 `LV_COLOR_DEPTH` 变化。
>
> 二者已由 PR#5 修正为 `heap_caps_malloc(..., MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)` 与“色深宏推导”，PR#7 又把 buffer 分配失败改为返回 `esp_err_t`。最新实现以 `docs/superpowers/*/2026-07-02-*` 与 `main/lvgl_qemu_rgb/lvgl_qemu_rgb.c` 为准；此处保留首版仅作历史记录。

- [x] **Step 3: 写 UI `main/lvgl_qemu_rgb/lvgl_demo_ui.c`**

```c
/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 * SPDX-FileContributor: 2026 ESP-Pocket2
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * 改编自官方 lvgl_demo_ui.c（LVGL scatter chart），适配 v9。
 * 首版省略官方 v8 的逐点渐变上色回调（v9 draw-task 事件模型），仅用单色散点确保跑通。
 */
#include "lvgl.h"
#include "lvgl_qemu_rgb.h"

static void qemu_rgb_add_data(lv_timer_t *timer)
{
    lv_obj_t *chart = (lv_obj_t *)lv_timer_get_user_data(timer);
    lv_chart_series_t *ser = lv_chart_get_series_next(chart, NULL);
    lv_chart_set_next_value2(chart, ser, lv_rand(0, 200), lv_rand(0, 1000));
}

void qemu_rgb_lvgl_demo_ui(lv_display_t *disp)
{
    lv_obj_t *scr = lv_display_get_screen_active(disp);

    /* 顶部提示文字，便于确认渲染成功 */
    lv_obj_t *label = lv_label_create(scr);
    lv_label_set_text(label, LV_SYMBOL_BELL " Hello Espressif & LVGL on QEMU RGB " LV_SYMBOL_BELL);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 20);

    /* 散点图 */
    lv_obj_t *chart = lv_chart_create(scr);
    lv_obj_set_size(chart, 200, 150);
    lv_obj_align(chart, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_line_width(chart, 0, LV_PART_ITEMS); /* 去掉连线，仅散点 */

    lv_chart_set_type(chart, LV_CHART_TYPE_SCATTER);
    lv_chart_set_axis_range(chart, LV_CHART_AXIS_PRIMARY_X, 0, 200);
    lv_chart_set_axis_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, 1000);
    lv_chart_set_point_count(chart, 50);

    lv_chart_series_t *ser = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_RED), LV_CHART_AXIS_PRIMARY_Y);
    for (int i = 0; i < 50; i++) {
        lv_chart_set_next_value2(chart, ser, lv_rand(0, 200), lv_rand(0, 1000));
    }

    lv_timer_create(qemu_rgb_add_data, 200, chart);
}
```

- [x] **Step 4: 写入口薄壳 `main/main_lvgl_qemu_rgb.c`**

```c
/*
 * SPDX-FileCopyrightText: 2026 ESP-Pocket2
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * QEMU 专用 LVGL demo 入口。切换 demo 见 main/CMakeLists.txt 的 SRCS 注释。
 */
#include "esp_err.h"
#include "esp_log.h"

#include "lvgl_qemu_rgb.h"

static const char *TAG = "MAIN-LVGL-QEMU-RGB";

void app_main(void)
{
    ESP_LOGI(TAG, "Start LVGL demo on QEMU virtual RGB panel");
    ESP_ERROR_CHECK(qemu_rgb_lvgl_run());
}
```

- [x] **Step 5: 提交**

```bash
git add main/lvgl_qemu_rgb/ main/main_lvgl_qemu_rgb.c
git commit -m "$(cat <<'EOF'
✨ Feat(main/main_lvgl_qemu_rgb.c): 新增 QEMU 虚拟 RGB 面板 LVGL 例程

- 移植自官方例程 lcd_qemu_rgb_panel，将 LVGL v8 lv_disp_drv API 适配为项目使用的 LVGL v9（lv_display_* + flush_cb + lv_display_flush_ready）
- 经 esp_lcd_new_rgb_qemu 建标准 esp_lcd 面板，自管 LVGL（draw buffer / esp_timer tick / LVGL task）
- UI 照搬官方散点图并适配 v9（lv_chart_set_axis_range），首版省略逐点渐变
- 导出符号统一 qemu_rgb_ 前缀，避免与 GLOB 同编的 hp28008.c 符号冲突
- 仅用于 QEMU：真机 esp_lcd_new_rgb_qemu 返回 ESP_ERR_NOT_SUPPORTED

参照:
- https://docs.espressif.com/projects/esp-idf/zh_CN/v5.5.4/esp32s3/api-guides/tools/qemu.html
- https://github.com/espressif/idf-extra-components/tree/master/esp_lcd_qemu_rgb
EOF
)"
```

---

## Task 3: CMakeLists 集成并切换启用入口

**Files:**
- Modify: `main/CMakeLists.txt`

- [x] **Step 1: 新增 GLOB（在 `SD_CARD_C_SOURCES` 行之后加一行）**

在现有这一行下面：

```cmake
file(GLOB_RECURSE SD_CARD_C_SOURCES ${CMAKE_SOURCE_DIR} "sd_card/*.c")
```

新增：

```cmake
file(GLOB_RECURSE LVGL_QEMU_RGB_C_SOURCES ${CMAKE_SOURCE_DIR} "lvgl_qemu_rgb/*.c")
```

- [x] **Step 2: 把新 GLOB 变量加入 `SRCS`，并切换启用入口**

将 `idf_component_register(SRCS ...)` 中，模块变量列表加入 `${LVGL_QEMU_RGB_C_SOURCES}`，并把入口区改为“仅启用 QEMU 入口、其余注释”。改后 `SRCS` 段落如下：

```cmake
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
    # "main.c"
    # "main_axp202.c"
    # "main_i2c_scanner.c"
    # "main_drv2605.c"
    # "main_bm8563.c"
    # "main_lvgl.c"
    # "main_lgfx.c"
    # "main_sd_card.c"
    "main_lvgl_qemu_rgb.c"

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
```

> 备注：无需改 `REQUIRES`——`esp_lcd_qemu_rgb`（公开依赖 `esp_lcd`）与现有 `esp_lvgl_port` 都会引入 `esp_lcd`。若构建报找不到 `esp_lcd_panel_ops.h`，再在 `idf_component_register` 追加 `REQUIRES esp_lcd`。

- [x] **Step 3: 提交**

```bash
git add main/CMakeLists.txt
git commit -m "$(cat <<'EOF'
🔧 Chore(main/CMakeLists.txt): CMakeLists 集成 lvgl_qemu_rgb 并切换启用其入口

- 新增 LVGL_QEMU_RGB_C_SOURCES GLOB 与 INCLUDE_DIRS "lvgl_qemu_rgb"
- SRCS 启用 main_lvgl_qemu_rgb.c，其余 main_*.c（含 main_sd_card.c）并列为注释备选
EOF
)"
```

---

## Task 4: 构建验证

**Files:** 无（仅构建）

- [x] **Step 1: 确保 IDF 环境就绪**

Run: `source /opt/esp/idf/export.sh`
Expected: 无错误；`idf.py --version` 可用。

- [x] **Step 2: 构建（首次会自动拉取 esp_lcd_qemu_rgb 到 managed_components）**

Run: `cd /workspace && idf.py build`
Expected: 结尾出现 `Project build complete.` 与 `ESP-Pocket2.bin` 生成。

重点排查：
- 链接期 `multiple definition of 'app_lcd_init'` 等 → 说明符号未加前缀，回到 Task 2 确认所有导出符号均为 `qemu_rgb_` 前缀或 `static`。
- `unknown type name 'lv_display_t'` / `implicit declaration of 'lv_chart_set_axis_range'` → 确认使用的是 LVGL v9 API（本 plan 代码已按 v9.5.0 校对）。
- 找不到 `esp_lcd_qemu_rgb.h` → 确认 Task 1 依赖已加且已 reconfigure（`idf.py reconfigure` 或删 `build/` 重来）。

- [x] **Step 3: （可选）体积报告**

Run: `idf.py size`
Expected: 正常输出 flash/RAM 用量。

---

## Task 5: QEMU 运行验证

**Files:** 无（仅运行）

> 关键：`esp_lcd_qemu_rgb` 依赖的虚拟 RGB 帧缓冲设备由 `--graphics` 提供。只有加 `--graphics` 才能让 `esp_lcd_new_rgb_qemu` 成功并渲染出画面；不带 `--graphics` 时该虚拟设备不存在。因此以下把图形模式作为主验证。

- [x] **Step 1: 图形验证（主，启用虚拟 RGB 帧缓冲）**

Run: `ls /tmp/.X11-unix/`（确认显示号，如 `X1` 对应 `:1`）
Run: `cd /workspace && DISPLAY=:1 timeout 40 idf.py qemu --graphics --qemu-extra-args "-no-reboot" 2>&1 | tee /tmp/qemu_graphics.log`
Expected: 日志依次出现（TAG=`qemu_rgb`）：

```text
Install QEMU RGB LCD panel driver
Initialize QEMU RGB LCD panel
Initialize LVGL library
Allocate separate LVGL draw buffer
Install LVGL tick timer
Create LVGL task
Display LVGL demo UI
Starting LVGL task
```

且弹出 QEMU 窗口，显示顶部 "Hello Espressif & LVGL on QEMU RGB" 文字 + 居中动态散点图；截图留证。

- [x] **Step 2: 无图形冒烟（仅确认能在 QEMU 启动，不验证面板）**

Run: `cd /workspace && timeout 40 idf.py qemu --qemu-extra-args "-no-reboot" 2>&1 | tee /tmp/qemu_smoke.log`
Expected: 至少出现 `main_task: Calling app_main()` 与入口日志 `Start LVGL demo on QEMU virtual RGB panel`。因未加 `--graphics`、虚拟帧缓冲不存在，`esp_lcd_new_rgb_qemu` 可能返回 `ESP_ERR_NOT_SUPPORTED` 使入口提前失败——属预期，不代表 feature 有问题。

> 若平台无 X server / 无 DISPLAY 而无法执行 Step 1，则至少完成 Step 2 以确认构建产物可在 QEMU 运行，并在 PR 说明“图形验证待具备 DISPLAY 环境补充”。

---

## 提交与 PR 汇总

- 分支：`cursor/lvgl-qemu-rgb-8883`（自 `dev`）。
- 提交顺序：Task1 依赖 → Task2 源码 → Task3 CMake → Docs（spec + plan 最后提交）。
- 官方参照链接分布：Task1 依赖提交放组件注册中心链接，Task2 源码提交放 QEMU 官方文档 + idf-extra-components 组件根目录，Task3 CMakeLists 与 Docs 提交不带参照。
- 每次逻辑提交后 `git push -u origin cursor/lvgl-qemu-rgb-8883`，并更新 PR（draft，base `dev`）。

## 后续（本 plan 不含）

- 方案 2：`main_lvgl_qemu_rgb_port.c` ↔ `main/lvgl_qemu_rgb_port/`，改用 `esp_lvgl_port`。
- 方案 3：`hp28008.c` + `CONFIG_HP28008_USE_QEMU_RGB` 编译期切后端。
- UI 增强：还原官方散点图逐点渐变（v9 draw-task 事件）。

---

## 执行结果（2026-07-01）

- Task 1–3 全部提交，分支 `cursor/lvgl-qemu-rgb-8883`。
- Task 4：`idf.py build` 通过，`ESP-Pocket2.bin` 约 467KB，flash 剩余 55%，无符号冲突、无 v9 API 错误。
- Task 5：`DISPLAY=:1 idf.py qemu --graphics` 运行成功，日志依次出现所有预期步骤（Install → Initialize → Allocate buffer → tick timer → LVGL task → Display UI），`main_task: Returned from app_main()` 正常退出。图形模式截图工具不可用（QEMU 超时后窗口关闭），以完整日志作为验证证据。
