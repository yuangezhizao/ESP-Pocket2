# QEMU 虚拟 RGB 面板 Dedicated FB 模式与官方 UI 对齐 Implementation Plan

> **For agentic workers:** 本实现采用 superpowers:subagent-driven-development——逐个 Task 派发独立 implementer subagent 实现（controller 提供该 Task 完整文本与上下文，不让 subagent 读本文件），每个 Task 完成后做两阶段 review（先 spec 合规、再代码质量），通过后再在 TodoWrite 标记完成并进入下一个 Task。Task tool 不指定 model 参数以让 subagent 继承父模型（本仓库要求父子模型一致，故不按 skill 的便宜模型分级选择）。步骤用 `- [ ]` / `- [x]` 复选框跟踪，执行后回填"执行结果"。

**Goal:** 在现有 `main/lvgl_qemu_rgb/` 例程上补齐官方的 Dedicated FB 渲染路径（Kconfig 开关 `CONFIG_LVGL_QEMU_RGB_DEDIC_FB`，默认关闭保持 Partial），并把 UI 对齐官方（色深/颜色格式处理、逐点渐变、刷新周期 100ms）；坐标轴刻度不补（v9 无等价 chart API），顶部 label 保留。

**Architecture:** 沿用官方结构，在 `lvgl_qemu_rgb.c` 抽 `qemu_rgb_lvgl_setup_buffers()`（对应官方 `example_lvgl_get_buffers()`）用 `#if` 分派 FULL/PARTIAL；`flush_cb` 两模式共用。UI 以 v9 官方 `lv_example_chart_7.c` 为准还原渐变。设计依据见 [docs/superpowers/specs/2026-07-02-lvgl-qemu-rgb-dedicated-fb-design.md](https://github.com/yuangezhizao/ESP-Pocket2/blob/dev/docs/superpowers/specs/2026-07-02-lvgl-qemu-rgb-dedicated-fb-design.md)。

**Tech Stack:** ESP-IDF v5.5.4 (esp32s3)、LVGL v9.5.0、`espressif/esp_lcd_qemu_rgb` v1.0.2、`esp_lcd`、`esp_timer`、FreeRTOS、QEMU（qemu-system-xtensa）。

**参考：**
- 官方例程 https://github.com/espressif/idf-extra-components/tree/master/esp_lcd_qemu_rgb/examples/lcd_qemu_rgb_panel
- LVGL v9 渐变散点图 https://github.com/lvgl/lvgl/blob/v9.5.0/examples/widgets/chart/lv_example_chart_7.c
- 设计文档 [docs/superpowers/specs/2026-07-02-lvgl-qemu-rgb-dedicated-fb-design.md](https://github.com/yuangezhizao/ESP-Pocket2/blob/dev/docs/superpowers/specs/2026-07-02-lvgl-qemu-rgb-dedicated-fb-design.md)

---

## Task 1: Kconfig 开关

**Files:**
- Modify: `main/Kconfig.projbuild`

- [x] **Step 1:** 在文件末尾追加 menu 与 config。

```kconfig
menu "LVGL QEMU RGB Example Configuration"

    config LVGL_QEMU_RGB_DEDIC_FB
        bool "Use QEMU RGB panel dedicated framebuffer as LVGL draw buffer"
        default n
        help
            启用后直接用 QEMU 虚拟面板自带帧缓冲（物理地址 0x20000000，整屏 800x480）作为 LVGL draw buffer，配合 FULL 渲染模式实现零拷贝；关闭（默认）则从内部 RAM 分配 10 行小 draw buffer，走 PARTIAL 分块渲染。

endmenu
```

- [x] **Step 2:** 提交。

```bash
git add main/Kconfig.projbuild
git commit -m "$(cat <<'EOF'
✨ Feat(main/Kconfig.projbuild): 新增 QEMU RGB Dedicated FB 开关

- config LVGL_QEMU_RGB_DEDIC_FB（default n），对齐官方 EXAMPLE_QEMU_RGB_PANEL_DEDIC_FB
- 关闭保持现状 Partial 分块渲染，开启改用 QEMU 专用帧缓冲 + FULL 整屏刷新
EOF
)"
```

---

## Task 2: core — 色深处理 + Dedicated FB 分派

**Files:**
- Modify: `main/lvgl_qemu_rgb/lvgl_qemu_rgb.c`

- [x] **Step 1:** 顶部加 `#include "sdkconfig.h"`（若未包含），新增色深宏推导（替换硬编码 RGB565）。

```c
/* 色深 -> QEMU bpp / LVGL color format / 每像素字节（对齐官方，v9 转译） */
#if CONFIG_LV_COLOR_DEPTH_32
#define QEMU_RGB_BPP            RGB_QEMU_BPP_32
#define QEMU_LVGL_COLOR_FORMAT  LV_COLOR_FORMAT_XRGB8888
#define QEMU_LVGL_BYTES_PER_PX  4
#elif CONFIG_LV_COLOR_DEPTH_16
#define QEMU_RGB_BPP            RGB_QEMU_BPP_16
#define QEMU_LVGL_COLOR_FORMAT  LV_COLOR_FORMAT_RGB565
#define QEMU_LVGL_BYTES_PER_PX  2
#else
#error "QEMU RGB Panel only supports 16-bit and 32-bit color depth, please set LV_COLOR_DEPTH to 16 or 32"
#endif
```

- [x] **Step 2:** 新增 `qemu_rgb_lvgl_setup_buffers()`（放在 `qemu_rgb_lvgl_run` 之前）。

```c
static void qemu_rgb_lvgl_setup_buffers(lv_display_t *disp, esp_lcd_panel_handle_t panel)
{
#if CONFIG_LVGL_QEMU_RGB_DEDIC_FB
    ESP_LOGI(TAG, "Use QEMU dedicated frame buffer as LVGL draw buffer");
    void *buf1 = NULL;
    ESP_ERROR_CHECK(esp_lcd_rgb_qemu_get_frame_buffer(panel, &buf1));
    const size_t buf_size = QEMU_LCD_H_RES * QEMU_LCD_V_RES * QEMU_LVGL_BYTES_PER_PX; /* 整屏 */
    lv_display_set_buffers(disp, buf1, NULL, buf_size, LV_DISPLAY_RENDER_MODE_FULL);
#else
    ESP_LOGI(TAG, "Allocate separate LVGL draw buffer");
    const size_t buf_size = QEMU_LCD_H_RES * QEMU_LVGL_BUF_LINES * QEMU_LVGL_BYTES_PER_PX;
    void *buf1 = malloc(buf_size);
    assert(buf1);
    lv_display_set_buffers(disp, buf1, NULL, buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);
#endif
}
```

> ⚠️ 更正（2026-07-04，复审 PR5）：上面 Step 2 的 `setup_buffers` 是本 plan 初版，两处已在后续演进中修正：
>
> 1. Partial 分支 `malloc` → `heap_caps_malloc(..., MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)`（本 plan Task B，修复开 PSRAM 时 buffer 落 PSRAM 导致的黑屏）。
> 2. 错误处理 → PR#7 把 `setup_buffers` 改为返回 `esp_err_t`：Partial 的 `assert(buf1)` 换成 `ESP_RETURN_ON_FALSE`、Dedicated FB 的 `ESP_ERROR_CHECK` 换成 `ESP_RETURN_ON_ERROR`，调用处用 `ESP_RETURN_ON_ERROR` 承接。
>
> 最新实现以 `main/lvgl_qemu_rgb/lvgl_qemu_rgb.c` 与本 spec 第 7.2/8 节为准。

- [x] **Step 3:** 改 `qemu_rgb_lvgl_run()`：面板 `.bpp` 用 `QEMU_RGB_BPP`；`lv_display_set_color_format(disp, QEMU_LVGL_COLOR_FORMAT)`；把原来的"分配 buffer + set_buffers"三行替换为 `qemu_rgb_lvgl_setup_buffers(disp, panel_handle);`。

- [x] **Step 4:** 提交。

```bash
git add main/lvgl_qemu_rgb/lvgl_qemu_rgb.c
git commit -m "$(cat <<'EOF'
✨ Feat(main/lvgl_qemu_rgb/lvgl_qemu_rgb.c): 补齐 Dedicated FB 渲染路径与色深处理

- 新增 qemu_rgb_lvgl_setup_buffers()：CONFIG_LVGL_QEMU_RGB_DEDIC_FB 开启走 esp_lcd_rgb_qemu_get_frame_buffer + FULL 整屏零拷贝，关闭走内部 RAM 10 行 + PARTIAL
- 色深跟随 LV_COLOR_DEPTH 推导 bpp/color format/每像素字节，带 #error 兜底（替换硬编码 RGB565），对齐官方
- flush_cb 两模式共用；Dedicated FB 必须 FULL（QEMU draw_bitmap 按紧凑 bitmap 读取，DIRECT 会错位）

参照:
- https://github.com/espressif/idf-extra-components/tree/master/esp_lcd_qemu_rgb/examples/lcd_qemu_rgb_panel
EOF
)"
```

---

## Task 3: UI — 逐点渐变 + 刷新周期

**Files:**
- Modify: `main/lvgl_qemu_rgb/lvgl_demo_ui.c`

- [x] **Step 1:** 新增 v9 draw-task 渐变回调 `qemu_rgb_draw_event_cb`（以 `lv_example_chart_7.c` 为准）。

```c
static void qemu_rgb_draw_event_cb(lv_event_t *e)
{
    lv_draw_task_t *draw_task = lv_event_get_draw_task(e);
    lv_draw_dsc_base_t *base_dsc = (lv_draw_dsc_base_t *)lv_draw_task_get_draw_dsc(draw_task);
    if (base_dsc->part != LV_PART_INDICATOR) {
        return;
    }
    lv_obj_t *obj = lv_event_get_target_obj(e);
    lv_chart_series_t *ser = lv_chart_get_series_next(obj, NULL);
    lv_draw_fill_dsc_t *fill_dsc = lv_draw_task_get_fill_dsc(draw_task);
    if (fill_dsc == NULL) {
        return;
    }
    uint32_t cnt = lv_chart_get_point_count(obj);
    /* 旧点更透明 */
    fill_dsc->opa = (lv_opa_t)((LV_OPA_COVER * base_dsc->id2) / (cnt - 1));
    /* 小值偏蓝、大值偏红 */
    int32_t *x_array = lv_chart_get_series_x_array(obj, ser);
    int32_t *y_array = lv_chart_get_series_y_array(obj, ser);
    uint32_t start_point = lv_chart_get_x_start_point(obj, ser);
    uint32_t p_act = (start_point + base_dsc->id2) % cnt;
    lv_opa_t x_opa = (lv_opa_t)((x_array[p_act] * LV_OPA_50) / 200);
    lv_opa_t y_opa = (lv_opa_t)((y_array[p_act] * LV_OPA_50) / 1000);
    fill_dsc->color = lv_color_mix(lv_palette_main(LV_PALETTE_RED),
                                   lv_palette_main(LV_PALETTE_BLUE),
                                   x_opa + y_opa);
}
```

- [x] **Step 2:** 在 `qemu_rgb_lvgl_demo_ui()` 里，chart 创建后接入渐变事件，并把定时器周期改为 100ms；保留顶部 label。

```c
    lv_obj_add_event_cb(chart, qemu_rgb_draw_event_cb, LV_EVENT_DRAW_TASK_ADDED, NULL);
    lv_obj_add_flag(chart, LV_OBJ_FLAG_SEND_DRAW_TASK_EVENTS);
```

```c
    lv_timer_create(qemu_rgb_add_data, 100, chart);  /* 200 -> 100，对齐官方 */
```

- [x] **Step 3:** 更新文件头注释（去掉"首版省略逐点渐变"表述，改为"逐点渐变已按 v9 draw-task 还原；坐标轴刻度因 v9 移除 chart tick 未补"）。

- [x] **Step 4:** 提交。

```bash
git add main/lvgl_qemu_rgb/lvgl_demo_ui.c
git commit -m "$(cat <<'EOF'
✨ Feat(main/lvgl_qemu_rgb/lvgl_demo_ui.c): UI 补齐逐点渐变并对齐刷新周期

- 以 v9 官方 lv_example_chart_7 为准还原逐点渐变（LV_EVENT_DRAW_TASK_ADDED + SEND_DRAW_TASK_EVENTS，fill_dsc 设 opa/color，红蓝 lv_color_mix）
- 刷新周期 200ms -> 100ms，对齐官方
- 保留顶部提示 label；坐标轴刻度因 v9 chart 移除 set_axis_tick 未补（见 spec）
EOF
)"
```

---

## Task 4: 构建验证（两模式）

**Files:** 无（仅构建）

- [x] **Step 1:** 环境就绪 `source /opt/esp/idf/export.sh`。
- [x] **Step 2:** 默认（Partial）`idf.py build`，期望 `Project build complete.`。
- [x] **Step 3:** Dedicated FB 构建（独立 build 目录 + `SDKCONFIG_DEFAULTS` 覆盖，避免污染主 `sdkconfig`）：
  ```bash
  printf 'CONFIG_LVGL_QEMU_RGB_DEDIC_FB=y\n' > /tmp/sdkconfig_dedic_extra
  idf.py -B build-dedic -DSDKCONFIG=build-dedic/sdkconfig -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;/tmp/sdkconfig_dedic_extra" build
  ```
  注：`idf.py -D CONFIG_LVGL_QEMU_RGB_DEDIC_FB=y` 仅写入 CMake CACHE，不影响 `sdkconfig.h`，C 代码实际读 `sdkconfig.h`，故不能用该方式。
- [x] **Step 4:**（可选）`idf.py size` 对照两模式内存。

重点排查：v9 draw-task API（`lv_draw_task_get_fill_dsc`/`lv_draw_dsc_base_t`/`id2`/`LV_PART_INDICATOR`）、color format 宏、`sdkconfig.h` 包含、符号前缀。

---

## Task 5: QEMU 运行验证

**Files:** 无（仅运行）

- [x] **Step 1:** 图形验证（主）：`ls /tmp/.X11-unix/` 确认显示号；`DISPLAY=:1 timeout 40 idf.py qemu --graphics --qemu-extra-args "-no-reboot"`，两模式均应显示 label + 动态渐变散点图；Dedicated FB 日志含 "Use QEMU dedicated frame buffer..."。截图留证。
- [x] **Step 2:** 若无 DISPLAY：`timeout 40 idf.py qemu --qemu-extra-args "-no-reboot"` 冒烟确认启动到 `app_main`，并在 PR 注明图形验证待补。

---

## Task 6: 文档（spec + plan，全部代码提交后合并为一次最后提交）

**Files:**
- Create: `docs/superpowers/specs/2026-07-02-lvgl-qemu-rgb-dedicated-fb-design.md`
- Create: `docs/superpowers/plans/2026-07-02-lvgl-qemu-rgb-dedicated-fb.md`

- [x] **Step 1:** 确认 Task1-3 代码 + 诊断(Task A) + 修复(Task B) 全部已提交；在执行 Step 2 前更新"执行结果"节（回填验证结论、体积、问题与修正）。
- [x] **Step 2:** 把 spec + plan 的**全部**更新（含 Dedicated FB 设计与 Partial 黑屏缺陷复盘）**合并为一次** docs 提交（git cz 格式）。

```bash
git add docs/superpowers/specs/2026-07-02-lvgl-qemu-rgb-dedicated-fb-design.md \
        docs/superpowers/plans/2026-07-02-lvgl-qemu-rgb-dedicated-fb.md
git commit -m "$(cat <<'EOF'
📝 Docs(docs/superpowers): QEMU RGB Dedicated FB 模式 spec 与实现计划 + Partial 黑屏缺陷复盘

- spec：Dedicated FB 的 v8->v9 适配（FULL 而非 DIRECT）、色深处理对齐、UI 逐点渐变、坐标轴刻度不补原因、错误处理差异；缺陷复盘（Partial 开 PSRAM 黑屏的根因/修复/验证）
- plan：Kconfig 开关 / core 色深+分派 / UI 渐变+周期 / 构建与 QEMU 验证 / 缺陷修复（诊断→修复→docs）
EOF
)"
```

---

## 提交与 PR 汇总

- 分支：`cursor/qemu-rgb-dedicated-fb-fc50`（自 `dev`）。
- 提交顺序：Task1 Kconfig → Task2 core → Task3 UI → 诊断(Task A) → 修复(Task B)（→ 验证一般不产生提交）→ Task6 docs（spec + plan 合并为一次，最后提交）。
- 每次逻辑提交后 `git push -u origin cursor/qemu-rgb-dedicated-fb-fc50`，创建/更新 draft PR（base `dev`）。
- **Review gate（`subagent-driven-development` 要求）**：每个实现 Task（Task1/2/3 + 缺陷修复的 Task A 诊断 / Task B 修复）提交后，必须先派 spec-reviewer subagent（确认代码与 spec 一致，无多无少），通过后再派 code-quality reviewer subagent（代码质量），两阶段全部通过后才在 TodoWrite 标记完成并进入下一个 Task。

## 后续（本 plan 不含）

- 坐标轴刻度：`lv_scale` 外挂还原（视觉近似）。
- PR3 遗留方案 2（`esp_lvgl_port`）、方案 3（Kconfig 切后端）。

---

## 执行结果

执行日期：2026-07-02。代码提交分支：`cursor/qemu-rgb-dedicated-fb-fc50`。

### 构建验证（Task 4）

- **Partial 模式**（`idf.py build`，使用项目根 `sdkconfig`，`CONFIG_LVGL_QEMU_RGB_DEDIC_FB` 未设置）：构建通过，固件 467953 bytes，DIRAM 123067 bytes（10 行小 draw buffer 16KB 占其中一部分）。
- **Dedicated FB 模式**（`idf.py -B build-dedic -DSDKCONFIG=build-dedic/sdkconfig -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;/tmp/sdkconfig_dedic_extra" build`）：构建通过，固件 468029 bytes（+76 bytes），DIRAM 同为 123067 bytes（帧缓冲在 QEMU 硬件区 `0x20000000`，不占内部堆）。`build-dedic/sdkconfig` 与 `build-dedic/config/sdkconfig.cmake` 均确认 `CONFIG_LVGL_QEMU_RGB_DEDIC_FB=y`。

> **QA-执行-1**：`idf.py -D CONFIG_LVGL_QEMU_RGB_DEDIC_FB=y` 不能用于启用 Kconfig 选项。该标志只写入 CMake CACHE，`sdkconfig.h`（C 代码实际读取）由 kconfgen 从 `sdkconfig` 文件生成，不受 CMake CACHE 影响。正确方式：`-DSDKCONFIG=build-dedic/sdkconfig` 指定独立 sdkconfig 文件 + `-DSDKCONFIG_DEFAULTS=...` 注入配置覆盖，并删除旧 `build-dedic` 目录确保 kconfgen 从 defaults 重新生成。spec 和 plan 的验证命令已在执行中同步修正。

### QEMU 图形验证（Task 5）

两模式均 `DISPLAY=:1 timeout 40 idf.py [-B build-dedic] qemu --graphics --qemu-extra-args "-no-reboot"` 通过，关键日志如下：

**Partial 模式日志（摘要）：**
```
I (273) main_task: Calling app_main()
I (273) MAIN-LVGL-QEMU-RGB: Start LVGL demo on QEMU virtual RGB panel
I (293) qemu_rgb: Allocate separate LVGL draw buffer
I (293) qemu_rgb: Starting LVGL task
I (573) qemu_rgb: Display LVGL demo UI
I (1823) main_task: Returned from app_main()
```

**Dedicated FB 模式日志（摘要）：**
```
I (169) main_task: Calling app_main()
I (169) MAIN-LVGL-QEMU-RGB: Start LVGL demo on QEMU virtual RGB panel
I (209) qemu_rgb: Use QEMU dedicated frame buffer as LVGL draw buffer
I (219) qemu_rgb: Starting LVGL task
I (239) qemu_rgb: Display LVGL demo UI
I (249) main_task: Returned from app_main()
```

两模式均正常启动，日志路径完整，无 ERROR / panic。`--graphics` 窗口需 VNC/noVNC 查看，以完整日志作为验证证据（与 PR3 一致）。

> 补记（缺陷复盘时）：`--graphics` 画面其实可用 `DISPLAY=:1 ffmpeg -f x11grab -i :1 -frames:v 1 -y shot.png` 截图核对。当初仅凭日志验收，漏掉了下文的 PSRAM 黑屏缺陷。

### 缺陷修复后验证（Task C，4 格矩阵 + [DIAG] 自证 + 截图）

修复（Task B：`heap_caps_malloc(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)`）后重跑全部 4 格，每格均有 `[DIAG]` 日志自证配置 + 截图确认画面：

| 格 | 渲染模式 | PSRAM | `[DIAG]` buffer 落点 | Invalid 告警 | 画面 |
|---|---|---|---|---|---|
| 1 | Partial | off | `buf@0x3fce9a94 INTERNAL` | 0 | 正常 |
| 2 | Partial | on | `buf@0x3fce9a40 INTERNAL` | 0 | **正常（修复前为黑屏）** |
| 3 | Dedicated FB | off | `buf@0x20000000 OTHER` | 0 | 正常 |
| 4 | Dedicated FB | on | `buf@0x20000000 OTHER` | 0 | 正常 |

修复后格 2（关键格）的 buffer 由 PSRAM(`0x3C…`) 变为 INTERNAL(`0x3FC…`)，Invalid 告警从修复前的 477 次降为 0，画面完全正常，验证通过。

---

## 缺陷修复：Partial 模式在 PSRAM 下黑屏（追加，PR #5 合并前发现）

**根因**：开 PSRAM 且 `malloc` 实际把 draw buffer 分配到 PSRAM 时（取决于 `SPIRAM_MALLOC_ALWAYSINTERNAL` 阈值等 allocator 策略），QEMU esp_rgb 设备只认 vram（`0x20000000`）/ internal DRAM 地址 → 每帧拒绝（`[ESP RGB] Invalid color content address or length`）→ 黑屏；仅开 PSRAM 但 buffer 仍落 internal 时不复现。详见 spec 第 12 节。**提交顺序：诊断（Task A）→ 修复（Task B）→ docs（并入前面的 Task 6，spec+plan 合并为一次，最后提交）。**

### Task A: 诊断 instrumentation（先提交）

**Files:** Modify `main/lvgl_qemu_rgb/lvgl_qemu_rgb.c` / `main/lvgl_qemu_rgb/lvgl_qemu_rgb.h` / `main/lvgl_qemu_rgb/lvgl_demo_ui.c`

- [x] `setup_buffers` 填充 `[DIAG]` 字符串（render mode / draw buffer 地址 / 内存类型 INTERNAL·PSRAM·OTHER / 色深 / PSRAM 开关），`ESP_LOGW` 打印 + 新增 `qemu_rgb_diag_str()` getter；GUI 底部加诊断 label；用 `esp_ptr_external_ram()`/`esp_ptr_internal()` 判定。
- [x] 提交（git cz）：

```bash
git add main/lvgl_qemu_rgb/lvgl_qemu_rgb.c main/lvgl_qemu_rgb/lvgl_qemu_rgb.h main/lvgl_qemu_rgb/lvgl_demo_ui.c
git commit -m "$(cat <<'EOF'
🔍 Chore(main/lvgl_qemu_rgb/lvgl_qemu_rgb.c): 加渲染配置诊断日志与 GUI 展示

- setup_buffers 打印 [DIAG] render mode / draw buffer 地址 / 内存类型(INTERNAL·PSRAM·OTHER) / 色深 / PSRAM 开关
- 新增 qemu_rgb_diag_str() 供 GUI 底部 label 展示，解决运行输出看不出 buffer 落点的盲区
EOF
)"
```

### Task B: 修复 Partial buffer 强制 internal RAM（后提交）

**Files:** Modify `main/lvgl_qemu_rgb/lvgl_qemu_rgb.c`

- [x] 加 `#include "esp_heap_caps.h"`；Partial 分支 `malloc(buf_size)` → `heap_caps_malloc(buf_size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)`。
- [x] 提交（git cz）：

```bash
git add main/lvgl_qemu_rgb/lvgl_qemu_rgb.c
git commit -m "$(cat <<'EOF'
🐛 Fix(main/lvgl_qemu_rgb/lvgl_qemu_rgb.c): Partial draw buffer 强制分配到 internal RAM

- malloc 换 heap_caps_malloc(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)，避免开 PSRAM 时 buffer 落 PSRAM
- 修复 QEMU esp_rgb 拒绝 PSRAM 地址导致的黑屏（见 spec 第 12 节）；小 draw buffer 放 internal RAM 真机通常也更稳
EOF
)"
```

### Task C: 验证（画面截图 + [DIAG] 自证）

- [x] 重跑 4 格矩阵（Partial/Dedicated × PSRAM off/on），确认修复后 Partial+PSRAM on 的 `[DIAG]` 显示 buffer 落 **INTERNAL**、Invalid 告警为 0、画面正常，并截图留证。（见"执行结果"节 Task C 验证表格）

### 文档提交（并入 Task 6，不单独成一次提交）

- [x] 本缺陷复盘的 spec/plan 更新**不单独提交**；随前面的 **Task 6** 与其余文档更新**合并为一次** docs 提交，在所有代码（Task1-3 + 诊断 + 修复）提交后最后执行（见 Task 6）。
