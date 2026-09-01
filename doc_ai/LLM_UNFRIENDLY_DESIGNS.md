# Godot 设计层「不 AI native」清单与对策（8 条）

> 一句话：Godot 对「人」友好（动态类型、字符串寻址、GUI 拖拽、二进制省空间），对「LLM」处处是坑——LLM 要的是静态、强类型、有 schema、名字可查、文本可 diff。这 8 条是**设计层**的坑（不是上一份 `LLM_READABILITY_AUDIT.md` 讲的二进制资产层），每条给「问题 → 对策 → 状态（已落地 / 建议）」。核心结论：对策不是「给 Godot 打补丁」，而是 GDSL 在 LLM 侧反着这些设计走——**LLM 只见文本 + 强类型 + 固定 ontology，native/字符串/哈希全由编译器吃掉**。

---

## 总表

| # | Godot 的不 AI native 设计 | 对策 | 状态 |
|---|---|---|---|
| 1 | GDScript 默认动态类型（Variant 满天飞） | DSL 强类型字段 + 静态 typecheck，编译不过即拒 | 已落地 |
| 2 | signal/call/get_node 字符串寻址 | 引用由 typecheck 解析成类型字段；字符串→哈希由编译器算，LLM 不碰 | 已落地（哈希）/ 部分（name 校验） |
| 3 | GDScript 命令式语言（LLM 错率高） | 声明式规则 when/then + 固定 effect ontology，删 while/递归/闭包 | 已落地 |
| 4 | Variant 隐式类型转换 | DSL 无 Variant，字段静态类型，隐式转换收窄到显式 | 建议（政策未钉死） |
| 5 | `.tscn`/`.tres` 无 schema（issue #7102） | 声明式层输入用 JSON Schema，`.tscn` 只当编译器输出 | 已落地 |
| 6 | ClassDB 三份 API 表示会漂移（C++ 宏 / extension_api.json / doc XML） | 定**单一事实源** `extension_api.json`，DSL 类型表/ontology 从它生成 | 建议（当前手写 + 手核） |
| 7 | MethodBind 按结构哈希匹配、不按名字 | 编译器从源码算哈希（`gdsl/abihash.h`），LLM 只写名字 | 已落地 |
| 8 | 资源默认二进制 + 路径/UID 引用脆弱 | 声明式层只吐文本 `.tscn/.tres`；悬空引用已校验（`scene_json.cpp:173-193`） | 已落地 |

---

## 逐条

### 1. 动态类型 → 强类型 DSL + 静态 typecheck

- **问题**：GDScript 类型标注可选，默认 `Variant`；LLM 生成的代码没有编译器兜底抓类型错。
- **对策**：`.gdsl` 状态字段强类型（`hp: int = 3`、`speed: float = 400.0`），`gdsl/typecheck` 静态拒绝「重复类型名 / 未知字段类型 / 重复字段 / 未知触发类型」。类型系统即约束解码器——LLM 输出必须过 typecheck 才能编译，错在编译期被拒，不是运行时炸。
- **状态**：已落地（83 用例 / 335 断言，`.wolf/STATUS.md`）。

### 2. 字符串寻址 → typecheck 解析 + 编译器算哈希

- **问题**：`emit_signal("hit")` / `connect("name")` / `$Node/Child` 全字符串，拼错 = 运行时炸。D5 是最狠证明：signal 名运行时被 hash 成结构哈希，LLM 预测不了。
- **对策**（两层）：
  - DSL 层：`target.hp` 是类型字段访问，typecheck 按 owner 类型校验字段存在（S6.2c 已拒绝「未知字段」「引 target 却无 target 子句」），不是运行时字符串查找。
  - ABI 层：字符串→哈希由**编译器**算——`gdsl/abihash.h` 移植 `hash_murmur3_one_32`+`hash_fmix32`（`core/templates/hashfuncs.h:112-122,144-150`）+ `MethodInfo::get_compatibility_hash`（`core/object/method_info.cpp:87-116`），LLM 只写 signal 名，哈希值编译器产、黄金值钉死防漂移（4047867050，真机 `--dump-extension-api` 对拍）。
- **状态**：哈希已落地；「signal/method 名走固定 ontology 闭集、typo 在 typecheck 拒」依赖第 3 条的 ontology，见下。

### 3. 命令式 → 声明式规则 + 固定 ontology

- **问题**：while/递归/闭包/隐式转换，LLM 在命令式控制流上错率远高于声明式。
- **对策**：DSL 是 `rule <Name> by <Owner> [target <Type>]: when <guard> then <effect>` 声明式规则；effect 动词是**有限闭集**（`set_field`/`emit_signal`/…，ontology v1 见 cerebrum D4），LLM 不能凭空发明符号——每个符号都能 grep 对 ClassDB 或 DSL 类型表核实（对齐 `docs/DOC_SPEC.md`「禁止编造符号」）。语法小到能进 tree-sitter CFG，才可编译成约束解码（设计文档 §3.2）。
- **状态**：已落地（S6.2a–c codegen 全链路）。

### 4. 隐式转换 → DSL 无 Variant，转换收窄到显式（建议钉死政策）

- **问题**：Variant 的 int→float→String 隐式 coercion 规则不显然，LLM 无法静态推理「值到底啥类型」。
- **对策**：DSL 源层无 Variant，字段 `int`/`float`/`Named` 静态定型；codegen 落到 C 原语（`int64_t`/`double`，STATUS S6.1），Variant 只出现在 ABI 封送边界（生成代码、确定性，非 LLM 手写）。**待钉死**：字面量与字段类型的兼容政策要显式写进 typecheck——建议「无隐式转换，仅允许 int→float 显式/窄化」，否则 typecheck 现在可能静默放行 `hp = 3.0` 这类。
- **状态**：建议（typecheck 已有，但字面量兼容政策未在 spec/测试里显式钉死）。

### 5. `.tscn` 无 schema → 声明式层走 JSON Schema，`.tscn` 只当输出

- **问题**：`.tscn` 文本无正式规范（设计文档引 issue #7102），`Vector2(1,2)` 字面量、路径引用全靠约定；JSON Schema 校验不了 Godot 自定义文本格式。
- **对策**：声明式层输入是 **JSON + JSON Schema**（`scene_from_json` 带 schema 校验，STATUS），`.tscn` 是编译器**输出**（`emit_tscn` 确定性生成、golden 逐字节测试），LLM 从不直接手写 `.tscn`。逻辑层走 GDExtension，也不手写 C。
- **状态**：已落地。

### 6. 三份 API 漂移 → 定单一事实源 `extension_api.json`（建议）

- **问题**：同一方法三张脸——C++ `ClassDB::bind_method` 宏绑定 → `extension_api.json`（6.96 MB 机器导出）→ `doc/classes/*.xml`（1,219 份，人肉填描述），靠人同步，会漂。
- **对策**：GDSL 编译器把 **`extension_api.json` 当唯一 ClassDB 事实源**，从它生成 DSL 的类型表 / `@extends` 基类 / ontology 符号白名单，而不是手写平行清单。这样「LLM 不能发明符号」从口头纪律变成**机械可执行**：DSL 引用的每个类/字段/signal 都能 grep 回 extension_api.json。当前状态是「ontology 手写 + ABI 符号手核 `core/extension/gdextension_interface.json`」（cerebrum D3），会随引擎版本漂。
- **状态**：建议（当前手写 + 手核，未从 extension_api.json 自动生成）。

### 7. 结构哈希 → 编译器从源码算，LLM 不碰

- **问题**：MethodBind 按 `MethodInfo::get_compatibility_hash()` 结构哈希匹配，跟名字无关；LLM「名字对就行」不成立，必须算哈希。
- **对策**：哈希由编译器算（`gdsl/abihash.h`，见第 2 条 ABI 层），LLM 只写名字。黄金值 4047867050 钉成回归测试，防未来改动漂移（D11 已从 2866548813 纠正）。
- **状态**：已落地。

### 8. 二进制默认 + 路径脆弱 → 只吐文本 + 配方内声明 ID（已落地）

- **问题**：导入/保存默认 `.res/.scn/.ctex/.translation` 二进制；节点引用靠 NodePath 字符串 + `.uid` 文件 + `.godot/` 缓存，重命名即断。
- **对策**：
  - 声明式层只吐文本 `.tscn/.tres`，走 `ResourceFormatLoaderText`（设计文档 §2 route C，二进制层审计里 Godot 已内置 binary↔text 转换）。
  - 节点引用在配方内用声明 ID/名字（schema 校验），编译器在 emit 时落成实际 `$Node/Child` 路径——LLM 不手写路径串。
  - 悬空引用已校验：`scene_json.cpp:173-193` 收集全树节点名，connection 的 `from`/`to` 必须命中已存在节点，否则拒绝（FR-004）。
- **状态**：已落地（文本层 + 悬空引用校验都在）。

---

## 数据出处

- 设计文档 `doc_ai/GODOT_LLM_DSL_DESIGN.md`（§2 两层架构、§3 语法、§4 直连路径；issue #7102）。
- 现状 `.wolf/STATUS.md`（S6.1–S6.3 已落地项）、`.wolf/cerebrum.md`（D3/D4/D5/D11 决策与哈希纠正）。
- 二进制层配套：`doc_ai/LLM_READABILITY_AUDIT.md`。
- API 文本面：`extension_api.json`（6.96 MB）、`core/extension/gdextension_interface.json`（328 KB）、`doc/classes/*.xml`（1,219 份）。
