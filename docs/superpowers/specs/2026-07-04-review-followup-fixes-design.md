# PR1/3/5/6 复审后续修复 — 设计文档（Spec）

- 日期：2026-07-04
- 状态：已评审（逐项方向经用户确认）
- 主题：对已合并的 PR1/PR3/PR5/PR6 做二次 Review，逐项修复发现的后续问题
- 关联分支 / PR：`cursor/review-fixes-pr1356-4d47`（PR #7，base `dev`）

## 1. 背景与目标

在 PR6 合并、本地同步到 `dev` 后，对 PR1/3/5/6 全部内容（文件改动、PR 正文/标题、提交信息）做了一次复审，得到一份编号化的问题清单。本任务逐条修复这些问题，并按 superpowers 流程沉淀 spec/plan。目标是消除复审发现的正确性、健壮性、可复现性与文档一致性缺口，而不改变各例程既有行为。

原则：改动最小、按原 PR 分组、优先采用复审时确认的推荐方向；对无法在本环境运行的例程如实说明。

## 2. 修复清单（按原 PR 分组）

### PR1（cloud-env）

- PR1-1：`.cursor/Dockerfile` 的 `FROM` 由仅 tag 改为 **tag+digest** 双锁定（`espressif/idf:v5.5.4@sha256:b9f2d6ea1c19e0c9f7959bdb74a9e3c775642f9d0f3b841937c5fa3363db892b`），移除“（发布 tag，可复现）”表述，并补 digest 更新方式注释。digest 由 `docker buildx imagetools inspect` / registry manifest 获取，是多架构 index 的 digest，`FROM tag@sha256:index` 为官方推荐写法。
- PR1-2：`AGENTS.md` 的 managed components 列表改为“以 `main/idf_component.yml` 为准”，避免随组件增减而漂移（该列表已因 PR3 增加 `esp_lcd_qemu_rgb` 而滞后）。
- PR1-3：`.cursor/Dockerfile` 平台自动安装清单保留在文件内，但加“实测于 2026-07-04，平台清单可能随时间变化”标注，并**重新校验**了 `/usr/local/share/vnc-desktop.Aptfile`：新增补记“云资产依赖”一节（`bash ca-certificates coreutils curl findutils gzip tar`）；`gh` 软链、`install-google-chrome`、chrome 等其余项仍与注释一致。

### PR3（QEMU RGB 例程）

- PR3-2：`main/lvgl_qemu_rgb/lvgl_qemu_rgb.c` 的 `lv_display_create` / `xSemaphoreCreateRecursiveMutex` / `xTaskCreate` 增加返回值检查（`ESP_RETURN_ON_FALSE`），失败返回明确错误码而非把空指针继续传给 LVGL。
- PR3-4：flush 回调记录 `esp_lcd_panel_draw_bitmap` 失败（`ESP_LOGW` + `esp_err_to_name`），但仍调用 `lv_display_flush_ready`（见 5.2 契约说明）。
- PR3-1 / PR3-3：这两条的代码问题（Partial `malloc` 落 PSRAM 黑屏、硬编码 RGB565）已由 PR5 在代码中修复。本次不改 PR3 首版代码，而在 PR3 的 plan/spec（`2026-07-01-*`）相应位置加“纠正注记”，指向 PR5 的最终实现，保留历史首版语义、避免与 PR5 文档重复（方案乙）。

### PR5（Dedicated FB）

- PR5-1：`2026-07-02` 的 spec 与 plan 均同步为最终实现——spec（`...-dedicated-fb-design.md`）§7.2 的 `malloc(...)` 改为 `heap_caps_malloc(..., MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)`、§8 补 `esp_err_t`（`ESP_RETURN_ON_*`）错误处理表述（assert 已移除）；plan（`...-dedicated-fb.md`）Task 2 加同源纠正注记（列表形式）。
- PR5-2：`qemu_rgb_lvgl_setup_buffers` 改为返回 `esp_err_t`（Partial 用 `ESP_RETURN_ON_FALSE`，Dedicated FB 用 `ESP_RETURN_ON_ERROR`），移除不再使用的 `<assert.h>`；调用处用 `ESP_RETURN_ON_ERROR` 承接。理由见 5.1。
- PR5-3：修正 PR5 标题 scope（`main/main_lvgl_qemu_rgb.c` → `main/lvgl_qemu_rgb`）。这是 GitHub 元数据操作（`gh pr edit` 对已合并 PR 的标题仍有效，仅改 PR 元信息、不动已合并代码/提交），在本分支之外单独执行。

### PR6（PC 模拟 / ui_app）

- PR6-1：`main/i2c_app/i2c_app.{h,cpp}` 的 `update_RTC_global_variable` 改为返回 `bool`，`strftime` 失败时清空缓冲并返回 `false`；`main/lvgl/hp28008.c` 的 `real_clock_source` 据此返回 `NULL`，避免 RTC 失败时 UI 显示上一次的旧时间。已知边界见 5.3。
- PR6-2：`pc_simulator/CMakeLists.txt` 的 LVGL FetchContent 由 tag `v9.5.0` pin 到对应 commit `85aa60d18b3d5e5588d7b247abf90198f07c8a63`（注释保留 `v9.5.0` 便于人读），提升可复现性。
- PR6-3：`main/ui_app/ui_app.c` 的 `ui_app_create` 增加运行时防重复守卫（见 5.4），不再仅依赖 `LV_ASSERT_MSG`；同步头文件注释。方案 2/3 记录在 5.4 作为后续演进。
- PR6-4：`.gitignore` 的 `build` 收窄为 `build/`（仅匹配目录，见 5.5）。
- PR6-5：`AGENTS.md` 顶部说明补充 `pc_simulator`（纯 PC LVGL SDL 模拟工程，非产品 host 服务）。
- PR6-6：`pc_simulator/README.md` 的 Linux 依赖补 `python3 python3-venv`（CMake 用 `python3 -m venv` 生成 esp_logo，最小环境缺 `python3-venv` 会失败）。

## 3. 未选方案与不扩范围

- PR3-1/PR3-3 不采用“字面重写 PR3 首版代码块”（方案甲）：会与 PR3 周边“首版简化”叙述矛盾且与 PR5 文档重复。
- PR5-2 不保留 `assert`（方案乙）：改用 `esp_err_t` 与 PR3-2 统一，且不受 assertion 级别开关影响（见 5.1）。
- PR6-3 不采用 `ui_app_destroy`（方案 2）/`ui_app_t` 实例化（方案 3）：当前均为一次性调用，方案 1 足够；2/3 记录备选。
- PR6-2 只 pin LVGL commit，不动 Python 依赖版本 pin / 自动安装开关（按用户圈定范围）。

## 4. 架构影响

无对外接口/数据流的结构性变化。唯一签名变化是 `update_RTC_global_variable(void)` 由 `void` 改为 `bool`（唯一代码调用点为 `main/lvgl/hp28008.c`，其余仅文档提及）。QEMU RGB 例程与 ui_app 的运行时行为在正常路径上不变，仅在失败路径上更可控。

## 5. 关键技术结论

### 5.1 assert / ESP_ERROR_CHECK vs ESP_RETURN_ON_ERROR / ESP_RETURN_ON_FALSE（遇错会停住还是重启？）

- `assert()`：断言启用时（本工程默认 `CONFIG_COMPILER_OPTIMIZATION_ASSERTION_LEVEL` = full，未在 `sdkconfig.defaults` 覆盖），失败会 `abort()` 触发 panic。本工程 panic 行为为 ESP-IDF 默认 `CONFIG_ESP_SYSTEM_PANIC_PRINT_REBOOT`（见 `components/esp_system/Kconfig` 的 `default ESP_SYSTEM_PANIC_PRINT_REBOOT`）：**打印寄存器后重启**，不是“停住”。仅当改为 `PANIC_PRINT_HALT` 才会停机等手动复位；`SILENT_REBOOT` 则静默重启。QEMU 下加 `-no-reboot` 时，panic 的软复位会让 QEMU 退出而非重启。
- `ESP_ERROR_CHECK(x)`：`x != ESP_OK` 时打印错误并 `abort()`，与 assert 同样走 panic→（默认）重启。
- `ESP_RETURN_ON_ERROR(x, tag, msg)` / `ESP_RETURN_ON_FALSE(cond, err, tag, msg)`：失败时只 `ESP_LOGE` 打印一条日志，然后从**当前函数 `return` 错误码**；**不 abort、不重启**，把后续处置交给调用方。
- 本 PR 的取舍：`qemu_rgb_lvgl_setup_buffers` / `qemu_rgb_lvgl_run` 内部用 `ESP_RETURN_ON_*` 优雅返回错误码；入口薄壳 `app_main` 仍是 `ESP_ERROR_CHECK(qemu_rgb_lvgl_run())`，因此真正致命失败最终仍会 abort（默认重启），但失败点有清晰日志与错误码传播，而非就地崩溃或把空指针传入 LVGL。

### 5.2 flush 回调为何“记录但仍 flush_ready”

LVGL 的 flush_cb 契约要求：无论刷新成功与否，都必须调用 `lv_display_flush_ready()` 通知 LVGL 该 draw buffer 可复用。若失败时提前 `return` 不调用，LVGL 会一直等待 buffer 就绪 → 渲染停摆（画面冻结）。故 PR3-4 在失败时仅 `ESP_LOGW` 记录（不 abort），随后照常 `flush_ready`；这样既能在整帧被 QEMU 拒绝（如历史上的 PSRAM 黑屏）时留下日志线索，又不牺牲可用性。

### 5.3 PR6-1 已知边界（记录在案）

本次修复只能检出 `strftime` 失败这一路径。若 `rtc.getDateTime()`（I2C 读 BM8563）失败但 `timeinfo` 仍保留上一次的旧值，`strftime` 仍会成功输出旧时间——此时 `update_RTC_global_variable` 会返回 `true`、UI 显示旧时间。要覆盖这种情况需要更底层的“RTC 读失败检测”（让 `rtc.getDateTime` 返回状态或校验时间有效性），属更大范围改动，本次不扩范围，可另开 issue/PR 处理。

### 5.4 PR6-3 三种防重复方案（采用方案 1，记录 2/3）

问题：`ui_app_create` 用模块级全局 `s_clock_label/s_clock_timer/s_clock_src` + `LV_ASSERT_MSG` 防重复。`LV_ASSERT_MSG` 受 LVGL 自身的 `LV_USE_ASSERT*` 配置控制，可能被关闭或只打印不中断；一旦失效，重复调用会再建一个 `lv_timer`，其 handle 覆盖 `s_clock_timer`——**第一个 timer 仍在 LVGL 链表里每周期触发，但 handle 丢失、再也无法 `lv_timer_delete`**（“悬挂 timer”），且旧对象被孤立，将来配合“销毁重建屏幕”会访问已释放对象而崩溃。

- 方案 1（采用）：运行时硬守卫。`if (s_clock_label != NULL) { LV_LOG_WARN(...); return; }`。优点：极小、与 assert 配置无关、真正阻止双建。缺点：只拒绝重复，不支持重新初始化。当前真机与 pc_simulator 都只调用一次，足够。
- 方案 2（备选）：新增 `ui_app_destroy()`，内部 `lv_timer_delete(s_clock_timer)` 并复位全局，支持销毁+重建。缺点：多一个 API、调用方需负责调用。
- 方案 3（备选）：`ui_app_t` 实例化，去模块级单例，支持多实例/多 display、可测。缺点：改动最大，需改头文件 API 与所有调用方（`hp28008.c` / `pc_simulator/main.c`），当前属过度设计（YAGNI）。

### 5.5 `.gitignore` `build` → `build/`

`build`（无斜杠）匹配任意层级下名为 build 的**文件或目录**；`build/`（尾斜杠）仅匹配**目录**，比前者收窄，同名普通文件不再被忽略。两者都仍是“任意层级”（无前导 `/`）。注意：`build/` 只匹配名为 `build` 的目录，不匹配 `build-dedic`/`build-hp` 等临时构建目录（这些应由使用者手动清理，勿 `git add -A`）。

## 6. 验证策略

- 固件编译：`idf.py build`（默认 Partial）需通过；覆盖 PR3-2/3-4、PR5-2（`lvgl_qemu_rgb.c`）、PR6-1（`hp28008.c`/`i2c_app.cpp`，因 GLOB 恒参与编译）、PR6-3（`ui_app.c`，恒参与编译）。
- QEMU 图形（PR3/PR5 例程）：`DISPLAY=:1 idf.py qemu --graphics` 跑 **PSRAM off/on × Partial/Dedicated FB 四格矩阵**（PSRAM on 用 `CONFIG_SPIRAM=y` + `SPIRAM_MALLOC_ALWAYSINTERNAL=0` 强制暴露 buffer 落点），核对 `[DIAG]` buffer 落点、无 `Invalid color content`、画面完整（截桌面以含顶部 label 与底部诊断 label）。
- pc_simulator（PR6 的 ui_app + LVGL pin）：`cmake` 配置+构建+运行，核对窗口渲染 hp28008 UI，并确认 FetchContent 检出的 LVGL commit 等于所 pin 的 `85aa60d…`。
- hp28008 真机例程（PR6-1 运行路径）：QEMU 无 ST7789/BM8563 硬件、无法出画面，仅能编译/链接校验；真机运行需实机（与仓库既有 QEMU 限制说明一致）。

## 7. 验证结果（2026-07-04，本分支 Cloud Agent）

| 验证项 | 结果 |
| --- | --- |
| `idf.py build`（4 种配置） | 均通过：默认（Partial/PSRAM off）、`build-ppsram`（Partial/PSRAM on）、`build-dedic`（Dedicated/PSRAM off）、`build-dpsram`（Dedicated/PSRAM on）；PSRAM on 者 sdkconfig 含 `CONFIG_SPIRAM=y`，Dedicated 者含 `CONFIG_LVGL_QEMU_RGB_DEDIC_FB=y` |
| QEMU `--graphics` 四格矩阵 | 见下表；4 格画面均完整、`Invalid color content` 均为 0、`Returned from app_main()` 正常 |
| pc_simulator 构建+运行 | 通过，窗口渲染 hp28008 UI（logo+欢迎语+本机时钟 `2026-07-04 05:24:32`+旋转按钮）；FetchContent 检出 LVGL commit = `85aa60d18b3d5e5588d7b247abf90198f07c8a63`（= 所 pin） |
| hp28008 真机入口（`main_lvgl.c`） | `idf.py -B build-hp build` 编译/链接通过；运行需真机（QEMU 无 ST7789/BM8563，不出画面） |

QEMU `--graphics` 四格矩阵（PSRAM on 用 `SPIRAM_MALLOC_ALWAYSINTERNAL=0` 强制暴露落点；均截桌面确认画面完整）：

| 渲染模式 | PSRAM | `[DIAG]` buffer 落点 | Invalid | 画面 |
|---|---|---|---|---|
| Partial | off | `buf@0x3fce9a94 INTERNAL` | 0 | 正常 |
| Partial | on | `buf@0x3fce9a40 INTERNAL` | 0 | 正常（fix 关键格：开 PSRAM 仍强制落 INTERNAL、不黑屏） |
| Dedicated FB | off | `buf@0x20000000 OTHER`（vram） | 0 | 正常 |
| Dedicated FB | on | `buf@0x20000000 OTHER`（vram） | 0 | 正常 |

> 环境备注：本环境构建 pc_simulator 需临时 `apt-get install libsdl2-dev`（首次 configure 还会联网 GitHub+PyPI）。这些依赖不持久化到后续 Cloud Agent，建议将 SDL2 等纳入 `.cursor` 环境配置。

## 8. QA 记录

1. QA1（PR5-3 已合并 PR 标题能否改？）：能。`gh pr edit <n> --title` 对已合并 PR 仍生效，仅改 PR 元数据，不影响已合并的代码/提交历史。故本次修正 PR5 标题 scope。
2. QA2（assert/ESP_RETURN 遇错停住还是重启？）：见 5.1——assert/ESP_ERROR_CHECK→abort→panic→默认重启（PRINT_HALT 才停住；QEMU `-no-reboot` 则退出）；ESP_RETURN_ON_*→打印日志并 return 错误码，不 abort/重启。
3. QA3（PR6-1 是否覆盖 I2C 读失败？）：否，见 5.3，属更底层检测，本次不扩范围。
4. QA4（提交结构）：同一原 PR 的改动（含该 PR 相关的旧文档纠正）合并为 1 个提交——对 07-01 的纠正并入 PR3 提交、对 07-02 的纠正并入 PR5 提交、对 07-03 的纠正并入 PR6 提交；最后 1 个提交只含新增的 07-04 spec/plan。共 5 个提交。
