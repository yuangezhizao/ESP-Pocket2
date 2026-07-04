# PR1/3/5/6 复审后续修复 Implementation Plan

> **For agentic workers:** 本任务在 agent 模式直接执行（决策已与用户逐条确认）。Task tool 不指定 model 参数以让 subagent 继承父模型（本仓库要求父子模型一致）。步骤用 `- [ ]` / `- [x]` 复选框跟踪，执行后回填“执行结果”。

**Goal:** 修复对已合并 PR1/3/5/6 复审发现的后续问题；按原 PR 分组为 4 个提交 + 最后 1 个 superpowers 文档单提交；不改变各例程既有行为，仅补正确性/健壮性/可复现性/文档一致性。

**Architecture:** 见 [docs/superpowers/specs/2026-07-04-review-followup-fixes-design.md](https://github.com/yuangezhizao/ESP-Pocket2/blob/dev/docs/superpowers/specs/2026-07-04-review-followup-fixes-design.md)。关键结论（assert vs ESP_RETURN、PR6-1 边界、PR6-3 方案 1/2/3）均记录在该 spec。

**Tech Stack:** ESP-IDF v5.5.4 (esp32s3)、LVGL v9.5.0、`espressif/esp_lcd_qemu_rgb` v1.0.2、SDL2（pc_simulator）、QEMU。

---

## Task 1: PR1 修复（cloud-env）

**Files:** Modify `.cursor/Dockerfile`、`AGENTS.md`

- [x] `.cursor/Dockerfile`：`FROM` 改为 `espressif/idf:v5.5.4@sha256:b9f2d6ea1c19e0c9f7959bdb74a9e3c775642f9d0f3b841937c5fa3363db892b`；移除“（发布 tag，可复现）”，补 digest 说明注释。
- [x] `.cursor/Dockerfile`：平台清单加“实测于 2026-07-04”，补记“云资产依赖：bash ca-certificates coreutils curl findutils gzip tar”。
- [x] `AGENTS.md`：镜像说明改“tag+digest 双锁定”；managed components 改“以 `main/idf_component.yml` 为准”。
- [x] 提交（git cz）：`🐳 Chore(.cursor/Dockerfile): pin idf 镜像 digest 并刷新平台包清单说明`。

## Task 2: PR3 代码修复

**Files:** Modify `main/lvgl_qemu_rgb/lvgl_qemu_rgb.c`

- [x] PR3-2：`lv_display_create` / `xSemaphoreCreateRecursiveMutex` / `xTaskCreate` 加 `ESP_RETURN_ON_FALSE` 返回值检查。
- [x] PR3-4：flush 回调用 `esp_err_t err = esp_lcd_panel_draw_bitmap(...)`，失败 `ESP_LOGW(... esp_err_to_name(err))`，仍 `lv_display_flush_ready`。
- [x] 提交（git cz）：`🐛 Fix(main/lvgl_qemu_rgb/lvgl_qemu_rgb.c): 资源创建加失败检查并记录 flush 错误`。

## Task 3: PR5 代码修复

**Files:** Modify `main/lvgl_qemu_rgb/lvgl_qemu_rgb.c`

- [x] PR5-2：`qemu_rgb_lvgl_setup_buffers` 返回 `esp_err_t`（Partial `ESP_RETURN_ON_FALSE(ESP_ERR_NO_MEM)`、Dedicated FB `ESP_RETURN_ON_ERROR`，末尾 `return ESP_OK`）；调用处 `ESP_RETURN_ON_ERROR` 承接；移除不再使用的 `<assert.h>`。
- [x] 提交（git cz）：`♻️ Refactor(main/lvgl_qemu_rgb/lvgl_qemu_rgb.c): setup_buffers 返回 esp_err_t 统一错误处理`。

## Task 4: PR6 修复

**Files:** Modify `main/i2c_app/i2c_app.h`、`main/i2c_app/i2c_app.cpp`、`main/lvgl/hp28008.c`、`pc_simulator/CMakeLists.txt`、`main/ui_app/ui_app.c`、`main/ui_app/ui_app.h`、`.gitignore`、`AGENTS.md`、`pc_simulator/README.md`

- [x] PR6-1：`update_RTC_global_variable` 返回 `bool`，`strftime` 失败清空缓冲并返回 `false`；`real_clock_source` 据此返回 `NULL`。
- [x] PR6-2：`pc_simulator/CMakeLists.txt` LVGL `GIT_TAG` 由 `v9.5.0` 改为 commit `85aa60d18b3d5e5588d7b247abf90198f07c8a63  # v9.5.0`。
- [x] PR6-3：`ui_app_create` 加运行时防重复守卫（`if (s_clock_label != NULL) { LV_LOG_WARN; return; }`），同步头文件注释。
- [x] PR6-4：`.gitignore` `build` → `build/`。
- [x] PR6-5：`AGENTS.md` 顶部补 `pc_simulator` 说明。
- [x] PR6-6：`pc_simulator/README.md` Linux 依赖补 `python3 python3-venv`。
- [x] 提交（git cz）：`🐛 Fix(main): PR6 后续修复（RTC 回退/LVGL pin/ui_app 防重复/gitignore/文档）`。

## Task 5: 验证（画面 + 日志）

**Files:** 无（仅构建/运行）

- [x] 构建 4 种配置：默认（Partial/PSRAM off）、`build-ppsram`（Partial/PSRAM on）、`build-dedic`（Dedicated/PSRAM off）、`build-dpsram`（Dedicated/PSRAM on）；PSRAM on 用 `CONFIG_SPIRAM=y` + `SPIRAM_MALLOC_ALWAYSINTERNAL=0`。均通过。
- [x] QEMU `--graphics` 跑 4 格矩阵（Partial/Dedicated × PSRAM off/on），**截桌面**（确保含顶部 label 与底部诊断 label、画面完整）+ 日志核对（`[DIAG]` 落点、无 Invalid）。
- [x] pc_simulator：`apt-get install libsdl2-dev` → `cmake -S pc_simulator -B pc_simulator/build` → `cmake --build ... --parallel` → 运行截图；确认 LVGL 检出 commit = 所 pin。
- [x] hp28008 真机入口：临时切 `main/CMakeLists.txt` 的 SRCS 到 `main_lvgl.c`，`idf.py -B build-hp build` 编译校验后 `git checkout` 还原；运行需真机。

## Task 6: superpowers 文档

**Files:** Create `docs/superpowers/specs/2026-07-04-review-followup-fixes-design.md`、`docs/superpowers/plans/2026-07-04-review-followup-fixes.md`；Modify `docs/superpowers/plans/2026-07-01-lvgl-qemu-rgb.md`、`docs/superpowers/specs/2026-07-01-lvgl-qemu-rgb-design.md`、`docs/superpowers/plans/2026-07-02-lvgl-qemu-rgb-dedicated-fb.md`、`docs/superpowers/specs/2026-07-02-lvgl-qemu-rgb-dedicated-fb-design.md`、`docs/superpowers/plans/2026-07-03-hp28008-lvgl-pc-sim.md`

- [x] PR3-1/PR3-3：在 `2026-07-01-*` plan/spec 相应位置加“纠正注记”，指向 PR5 的最终实现（方案乙，不重写首版代码）；**随 Task 2（PR3）提交**（旧文档纠正并入相关 PR 的修复提交）。
- [x] PR5-1：`2026-07-02` spec 与 plan 均同步——spec §7.2 的 `malloc(...)` 改为 `heap_caps_malloc(..., MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)`、§8 补 `ESP_RETURN_ON_*` 错误处理表述；plan Task 2 加同源纠正注记；**随 Task 3（PR5）提交**。
- [x] PR6（07-03 附带）：`2026-07-03` plan 修复重复的 `---` 分隔线并统一段间空行为单个空行；**随 Task 4（PR6）提交**。
- [x] 新建本 spec/plan（含 assert/ESP_RETURN 结论、PR6-1 边界、PR6-3 方案 2/3、验证结果），作为**最后一个提交、只含 07-04 两个新文件**：`📝 Docs(docs/superpowers): 新增 07-04 复审修复 spec 与实现计划`。

---

## 提交与 PR 汇总

- 分支：`cursor/review-fixes-pr1356-4d47`（自 `dev`）。
- 提交结构（用户指定）：4 个 PR 对应提交（PR1 / PR3 / PR5 / PR6），其中对旧文档的纠正并入相关 PR 提交（07-01 → PR3、07-02 → PR5、07-03 → PR6）；再加最后 1 个只含新增 07-04 spec/plan 的文档提交，共 5 个提交。
- PR：#7（base `dev`，draft）。
- PR5-3（改已合并 PR5 标题）为 GitHub 元数据操作，分支之外单独执行。

## 执行结果（2026-07-04）

- 5 个提交按序完成；4 种配置 `idf.py build` 均通过。
- QEMU `--graphics` 四格矩阵（均截桌面、画面完整、`Invalid color content` 为 0、`Returned from app_main()` 正常）：

  | 渲染模式 | PSRAM | `[DIAG]` buffer 落点 | Invalid | 画面 |
  |---|---|---|---|---|
  | Partial | off | `buf@0x3fce9a94 INTERNAL` | 0 | 正常 |
  | Partial | on | `buf@0x3fce9a40 INTERNAL` | 0 | 正常（fix 关键格：开 PSRAM 仍落 INTERNAL） |
  | Dedicated FB | off | `buf@0x20000000 OTHER`（vram） | 0 | 正常 |
  | Dedicated FB | on | `buf@0x20000000 OTHER`（vram） | 0 | 正常 |

- pc_simulator：构建/运行通过，UI 正常（本机时钟 `2026-07-04 05:24:32`）；LVGL 检出 commit = `85aa60d18b3d5e5588d7b247abf90198f07c8a63`（= 所 pin）。
- hp28008 真机入口：编译/链接通过（`build-hp`）；运行需真机。
- 临时构建目录 `build-ppsram`/`build-dedic`/`build-dpsram`/`build-hp` 已清理；`sdkconfig`/`managed_components`/`main/images/esp_logo.c`/`dependencies.lock` 等生成物未入库（仅显式 `git add` 目标文件）。
