# Godot 源「LLM 可读性」审计：哪些是 native/二进制、怎么转文本、方向对不对

> 一句话：Godot 源码 99%+ 本来就是 LLM 可读文本；那 107 个二进制文件（12.3 MB）全是**数据资产**（字体/图标/查表/ICU 数据表/2 个 jar），不是「native 代码」。把它们转成文本是**方向错的任务**——LLM 不需要读字体或 ICU 表。真正要「native → 文本」的那一层，Godot 已经自己生成了：`extension_api.json`（6.96 MB 全 ClassDB 导出）+ `core/extension/gdextension_interface.json`（GDExtension ABI 的 JSON 形式）+ `doc/classes/*.xml`（1219 份 API 文档）。GDSL 的方向（文本进 → native 出，LLM 只见文本）是对的；「把二进制资产转文本」是伪命题。

---

## 1. 审计方法（一手来源 = 本仓库实扫）

- `git ls-files` 全量 14,291 个 tracked 文件，对每个文件读前 4 KB 做 NUL 字节检测（binary 启发式）+ 已知二进制扩展名白名单，双路交叉。
- 结果：**107 个二进制文件，合计 12,330,021 字节**（约 12.3 MB）；其余 14,184 个文件全是文本（C++/GLSL/Java/GDScript/`.tscn`/`.tres`/`.xml`/SCons 等）。
- 按扩展名（实扫值）：

| 扩展名 | 数量 | 典型内容 |
|---|---|---|
| `.webp` | 32 | Android 启动图标 / 通知图标（`platform/android/java/.../mipmap-*`） |
| `.woff2` | 28 | 编辑器内嵌回退字体（`thirdparty/fonts/*`） |
| `.png` | 16 | logo、splash、SMAA AreaTex/SearchTex、test fixtures |
| `.icns` | 7 | macOS 图标（`misc/dist/macos_tools.app`） |
| `.bin` | 7 | glTF buffer + 端序/半精度浮点测试向量 |
| `.jar` | 2 | gradle wrapper + mono Android 加密 native 库 |
| `.ico` / `.dds` / `.jpg` | 2 each | Windows 图标 / LTC 查表 / 测试图 |
| `.car`/`.ctex`/`.keystore`/`.bmp`/`.exr`/`.tga`/`.glb`/`.zip`/`.dat` | 1 each | 见下 |

- **没有**任何预编译 C/C++ 原生库（`.a`/`.so`/`.dll`/`.lib`）——Godot 的 iOS/Android 预编译库是 SCons 构建时下载的，不进 git（iOS `.xcframework` 只有 0 字节 `empty` 占位目录）。

## 2. 逐类盘点（Q1：哪些模块不是 LLM 可读）

按「是否 native 代码」分四类，结论先行：**没有 native 代码，只有数据资产。**

| 类别 | 文件（路径） | 大小 | 性质 |
|---|---|---|---|
| 预编译数据表 | `thirdparty/icu4c/icudt_godot.dat` | 4.8 MB（最大单项） | ICU Unicode 数据表，非代码 |
| 字体 | `thirdparty/fonts/*.woff2`（28） | ~1.6 MB | 字形轮廓，二进制 |
| 图片/纹理 | `misc/logo/*.png`、`misc/dist/macos_tools.app/*.icns`+`Assets.car`、`platform/android/.../*.webp`、`platform/windows/*.ico`、`servers/rendering/storage/ltc/ltc_lut{1,2}.dds`、`thirdparty/smaa/AreaTex.png`、`tests/data/images/*`、`main/splash.png` | ~3.5 MB | 位图/压缩纹理/查表 |
| 二进制容器 | `platform/android/.../gradle-wrapper.jar`、`modules/mono/thirdparty/libSystem.Security.Cryptography.Native.Android.jar`、`.../debug.keystore`、`tests/data/models/suzanne.glb`、`tests/data/models/cube.bin`、`modules/zip/tests/data/test.zip` | ~0.6 MB | 2 个 Java 字节码 + 密钥 + 二进制 glTF + 测试包 |

按顶层目录分布（实扫）：`thirdparty` 31 文件 7.5 MB（ICU dat + 字体 + SMAA）、`misc` 14 文件 2.5 MB（macOS 图标/logo）、`tests` 10 文件 1.7 MB（图像/模型 fixtures）、`platform` 39 文件 455 KB（Android jar/keystore/图标）、`servers` 2 文件 131 KB（LTC dds）、`main` 2 文件 18 KB（splash/app icon）、`modules` 5 文件 13.5 KB（mono jar + gltf/zip 测试）。

## 3. 怎么转 LLM 友好文本（Q2）

逐类「能否转」+「转了有没有用」：

| 类别 | 转文本路径 | 可无损？ | 对 LLM 有用？ |
|---|---|---|---|
| ICU `.dat` | 上游就是 CLDR（文本/XML）源；要文本用 CLDR，别逆编 `.dat` | 是（回上游） | ❌ LLM 不读 Unicode 表 |
| `.woff2` 字体 | 无有意义文本（字形轮廓） | 否 | ❌ |
| 位图 → `.svg` | logo/图标本是矢量，仓库已有 **1,185 个 `.svg`**；位图可 base64 | 矢量时是 | ❌（LLM 看 base64 无用） |
| `.glb` → `.gltf` | 同一 glTF 规范：`.gltf`=JSON 文本 + base64 buffer。仓库已现成示范：`tests/data/models/cube.gltf`(文本)+`cube.bin`(buffer) vs `suzanne.glb`(二进制) | **是（无损）** | ⚠️ 仅结构描述有用 |
| `.dds` LTC 查表 | 源是 Epic LTC 已知数据，可写 `.h` float 数组 | 是 | ❌ |
| `.jar` | 解包 + `javap`/反编译，或直接用上游源（gradle wrapper 是标准件、mono 库有源） | 是 | ❌ |
| `.keystore` | base64 / PKCS12 | 是 | ❌ |
| `.car` | 源是仓库里已有的 `.xcassets` JSON（`misc/dist/apple_embedded_xcode/.../Images.xcassets/*/Contents.json`） | 是（回源） | ❌ |

关键结论：**唯一能「无损转文本」且是同一数据的，是 `.glb→.gltf`（二进制→JSON）**，而 Godot 自家资源格式的「native↔文本」转换也早已内置——二进制 `.res/.scn` ↔ 文本 `.tres/.tscn` 由 `core/io/resource_format_binary.cpp`（二进制 loader/saver）与 `scene/resources/resource_format_text.cpp`（文本 loader/saver）成对实现。设计文档 §2 的声明式层正是走这条文本路（`ResourceFormatLoaderText`）。

## 4. 方向评估（Q3：转这些「对不对」）

**结论：把 107 个二进制资产转文本 = 方向错的任务（red herring）。但 GDSL 本身的方向是对的。**

1. **它们全是数据，不是逻辑。** GDSL 的目标是「让 LLM 写配方 + 规则」（设计文档第 3 行），LLM 写玩法时**从不**需要读字体/图标/ICU 表/LTC 查表。转这些不增加任何能力，纯浪费 token 和工程量。

2. **LLM 真正需要的 Godot「native 面」，已经是文本，且 Godot 自己生成了：**
   - `extension_api.json`（6.96 MB）—— 全 ClassDB 导出（`--dump-extension-api` 产物），就是「native API → JSON 文本」。
   - `core/extension/gdextension_interface.json`（328 KB）—— GDExtension ABI 的 JSON 形式，GDSL codegen 已经直接消费它（cerebrum D3/D5 逐符号核对）。
   - `doc/classes/*.xml`（1,219 份）—— 每个脚本暴露方法的 API 文档。
   
   这三样就是「native 代码 → LLM 友好文本」的正确答案，且已经存在，不用「转」，只需「用」。

3. **GDSL 管线里 native 层的正确位置是「输出端」而非「输入端」。** LLM 写 `.gdsl`（文本）+ `.tscn`（文本）→ 编译器产出 native C → `.dll`（不透明）。native 层本来就该不透明——LLM 读它毫无意义。这条边界现状（STATUS.md / 设计 §2）已经是对的，没有「要把 native 转成文本」的缺口。

4. **唯一「native↔文本」真正有意义的地方是 Godot 资源格式**，而 Godot 已内置该转换（§3 的 binary/text 成对 loader/saver），设计文档也已在声明式层用文本格式。这个本能已经满足。

**一句话方向判断：** 「让 Godot 对 LLM 友好」的正确抓手是 (a) 消费已生成的 JSON/XML API 描述（extension_api / gdextension_interface / doc classes），(b) 保持 DSL 文本进、native 出。**不是**去转那 107 个二进制资产文件。

## 5. 数据出处

- 文件计数/大小/扩展名：本仓库 `git ls-files` 实扫（2026-09-01），14,291 tracked / 107 binary / 12,330,021 bytes。
- 二进制分类依据：NUL 字节检测（前 4 KB）+ 扩展名白名单，双路交叉。
- API 文本面：`extension_api.json`（仓库根，6,965,049 bytes）、`core/extension/gdextension_interface.json`（328,021 bytes）、`doc/classes/*.xml`（1,219 个）。
- 资源 native↔文本：`core/io/resource_format_binary.cpp`、`scene/resources/resource_format_text.cpp`。
- glb/gltf 成对示范：`tests/data/models/cube.gltf` + `cube.bin` vs `tests/data/models/suzanne.glb`。
- 方向对照：`doc_ai/GODOT_LLM_DSL_DESIGN.md`（§2 两层架构、§4 直连路径）、`.wolf/STATUS.md`（route B 已跑通）、`.wolf/cerebrum.md`（D3/D5 逐符号核对 ABI json）。
- 设计层配套（8 条「不 AI native」设计 + 对策）：`doc_ai/LLM_UNFRIENDLY_DESIGNS.md`。
