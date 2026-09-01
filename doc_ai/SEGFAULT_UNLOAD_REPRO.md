# SEGFAULT 攻坚记录：unload→free 崩溃 = 非 editor 模式 artifact（2026-09-01 实测 + 源码修正）

> 结论先行（**本文件修正了 2026-09-01 早前的错误结论**）：
> **「普通退出」segfault 已确认干净；「unload→free 崩溃」我早前说「带 54864dd 修复仍然崩」，这结论是错的——那是非 editor 模式测试造成的假崩溃。真正根因是 GDExtension 可重载（reloadable）被引擎门控为「仅 editor 模式」，非 editor 模式根本不追踪实例，所以 `_clear_extension` 清了空气，`p.free()` 才会撞已卸载 DLL。这不是引擎 bug，是「运行时 unload 扩展 + 有存活实例」这个用法本身不被支持。**
>
> 归属：这条 `unload→free` 崩路径**不是**另一 lane 拿到的退出 segfault。另一 lane 的「普通退出」segfault 已确认修复（见场景 A）。

## 一、两条崩溃务必区分（本表修正）

| 场景 | 操作 | 官方 rc3 | fork 构建（含 54864dd） | 状态 |
|---|---|---|---|---|
| A. 普通退出 | load + instantiate + quit（不 unload） | exit 0 ×5 | exit 0 ×5 | ✅ 干净（已修） |
| B. unload→free | `unload_extension()` + `p.free()`（存活实例） | exit 139 | exit 139 | ⚠️ 非 editor 模式 artifact，非引擎 bug |

- 场景 A = 另一 lane 验收信号里那只「任何注册方法后退出的 GDExtension」，现在两个二进制都不复现。
- 场景 B 的崩溃**只在我用 `--headless --path`（非 editor）跑时出现**。换成 editor 模式（`--editor`）会不同——见下方根因。

## 二、真根因（源码链，非猜测）

崩溃链（`--headless --path` = 非 editor 模式）：

```
1. main.cpp:2222-2224  —— set_extension_reloading_enabled(true) 只在 editor 分支里调用
   if (editor) { set_editor_hint(true); set_extension_reloading_enabled(true); ... }

2. gdextension_library_loader.cpp:217 —— reloadable = manifest_reloadable && is_extension_reloading_enabled()
   => 非 editor 模式 is_extension_reloading_enabled()==false => set_reloadable(false)

3. gdextension.cpp:557-560 —— reloadable==false 时 track_instance/untrack_instance 被置 nullptr
   => extension->instances 恒空（从不 _track_instance）

4. gdextension.cpp:744 —— unload 时 _unregister_extension_class 调 _clear_extension
   _clear_extension(:972-989) 遍历 instances => 空 => 什么都不清
   => 存活实例 P 的 _extension/_extension_instance 仍指向（已被卸载/释放的）ObjectGDExtension

5. object.cpp:188-196 —— p.free() 时 _predelete 见 _extension 非空
   => 调 untrack_instance + free_instance，读 tracking_userdata/函数指针 => 进已卸载 DLL => use-after-free => signal 11
```

## 三、为什么这不是「修复没盖住」

- `54864dd` 把 `_clear_extension` 从析构挪到 `_unregister_extension_class`（:738-744），修的是「**editor 模式**关机/重载时 autoload 泄漏实例被清成原生类」这条。editor 模式 `is_extension_reloading_enabled()==true` → reloadable → 追踪开 → `_clear_extension` 有实例可清 → 干净（JOURNEY Era 20 「编辑器 3x 干净」）。
- 我早前复现的崩是**非 editor 模式**跑出来的：reloadable 根本没开，追踪是空的，`_clear_extension` 当然清不了任何东西。这是**用法问题**（运行期卸载含活实例的扩展），不是引擎修复漏掉的路径。

## 四、对另一 lane 的最终建议

1. **另一 lane 的验收（普通退出 segfault）已达成**：最小复现 `--headless --quit` 在官方 rc3 与 fork 构建上都 exit 0（5/5）。
2. **不需要给 gdextension.cpp 打最小补丁**：非 editor 模式的 unload→free 崩是引擎设计边界（reloadable 仅 editor，main.cpp:2222-2224），不是可修的最小漏洞。硬要支持「运行期 unload + 有存活实例」属于功能/设计变更，不是 bug fix。
3. 若要继续查，方向不是「绕过非 reloadable 无追踪」，而是先明确**你想要 editor 模式还是运行期的哪种行为**，再谈改法。千万别拿非 editor 复现去推「修复失败」。

## 五、实测环境记录

- 引擎 A：`D:/GitRepo-My/godot/.godot-bin/Godot_v4.7-rc3_win64_console.exe`（官方 rc3，无 fork 修复）
- 引擎 B：`D:/GitRepo-My/godot/bin/godot.windows.editor.x86_64.console.exe`（fork 自定义 release-editor，commit 4cd93fa，含 54864dd）
- GDExtension：`gdsl/example/self_rule.gd|.gdextension|.dll`（Player + 注册方法 Die + 状态字段 hp）
- 复现脚本 `gdsl/toolchain/repro_unload_crash.ps1` 复现的是**非 editor 模式的 artifact**，仅供确认源码链用，不作为「引擎 bug」验收。
- 测试日期：2026-09-01；临时工程在 `%LOCALAPPDATA%\Temp\gdsl_unload_repro`（脚本自建）
