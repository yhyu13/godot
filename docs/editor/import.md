# import（editor）

> 一句话：把硬盘上的"外人文件"（PNG、WAV、GLTF……）翻译成引擎内部的 Godot 资源，并留一张`.import`小纸条记录翻译参数，下次不再重新翻译。

**结论**：这个模块是编辑器的资源进口口岸，负责把几十种第三方源格式转换成引擎原生资源（Texture2D、AudioStream、PackedScene 等），为编辑器预览和运行时加载服务；代价是它只在编辑器里工作（`TOOLS_ENABLED`），且每种格式都要写一个专门的导入器类。

## 是什么 / 不是什么

- **是什么**：一批具体的导入器（`ResourceImporter*`），每个负责认领一组文件后缀、声明导入选项、执行"源文件 → Godot 资源"的转换，并把转换产物写进 `.godot/imported/`。
- **不是什么**：它不负责**调度**"什么时候该重新导入"——那是 `EditorFileSystem`（`editor/file_system`）的事；它也不负责**运行时**读取导入结果——那是 `core/io` 里的 `ResourceFormatImporter` 读 `.import` 文件后交给 `ResourceLoader` 的活。它只干"翻译"这一件事。

## 在引擎里的位置

`ResourceImporter` 基类本身住在 `core/io`（`core/io/resource_importer.h:110`），本模块是它的"具体实现仓库"：每个导入器继承它，运行时靠 `ResourceFormatImporter` 查表调用。

```mermaid
flowchart TB
    subgraph editor["editor（本模块）"]
        RIT["ResourceImporterTexture<br/>ResourceImporterImage<br/>ResourceImporterSVG<br/>…（约 16 个）"]
        RIS["ResourceImporterScene<br/>ResourceImporterOBJ<br/>EditorSceneFormatImporter*"]
        EP["EditorImportPlugin<br/>（脚本可继承）"]
    end
    subgraph core["core/io"]
        RI["ResourceImporter<br/>（抽象基类，RefCounted）"]
        RFI["ResourceFormatImporter<br/>（ResourceFormatLoader，读 .import）"]
    end
    subgraph other["editor 相邻"]
        EFS["EditorFileSystem<br/>（调度重导入）"]
        ID["ImportDock<br/>（editor/docks）"]
    end

    RIT -->|继承| RI
    RIS -->|继承| RI
    EP -->|继承| RI
    EFS -->|调用 import()| RIT
    EFS -->|调用 import()| RIS
    RFI -->|get_importer_by_name| RI
    ID -->|编辑选项| RIT
```

要点：`ImportDock`（导入面板）在 `editor/docks/import_dock.h:45`，属于 `docks` 模块，不在这里；它只是拿着本模块导出的选项列表去画 UI。

## 关键概念

- **导入器（importer）**：一个"翻译官"，对应一种或一类源格式。基类抽象接口在 `core/io/resource_importer.h:110`，每个子类用 `get_importer_name()` 报上唯一名字，例如贴图导入器返回 `"texture"`（`resource_importer_texture.cpp:163`）。
- **导入选项（ImportOption）**：翻译时可调旋钮，用 `PropertyInfo` + 默认值描述（`core/io/resource_importer.h:130` 的 `ResourceImporter::ImportOption`）。由 `get_import_options()` 按文件路径和预设枚举出来（`core/io/resource_importer.h:152`）。
- **预设（preset）**：一组预调好的旋钮。贴图导入器有"检测 / 2D / 3D"三档（`ResourceImporterTexture::Preset`，`resource_importer_texture.h:107`）。
- **导入顺序（import order）**：决定谁先翻译。贴图要先于场景，所以 `ResourceImporterScene::get_import_order()` 返回 `IMPORT_ORDER_SCENE`（值 100，`core/io/resource_importer.h:143`），垫底执行。
- **`.import` 侧车文件**：每个源文件旁边一张文本小纸条，记着 `importer`、`type`、`uid`、`path`、`valid`、`metadata` 这些键（解析代码 `core/io/resource_importer.cpp:44-155`）。运行时靠它找到成品路径、校验是否需要重导。

## 核心文件（按阅读顺序）

1. `core/io/resource_importer.h` — 抽象基类 `ResourceImporter` 和运行时加载器 `ResourceFormatImporter` 的接口（先读它，再看实现）。
2. `editor/import/resource_importer_texture.h` — 最复杂的二维贴图导入器：压缩模式、通道重映射、粗糙度/法线/3D 探测。
3. `editor/import/resource_importer_layered_texture.h` — 2D 数组 / 立方体贴图 / 3D 体纹理的分层贴图导入器。
4. `editor/import/3d/resource_importer_scene.h` — 场景导入器，以及 `EditorSceneFormatImporter` / `EditorScenePostImportPlugin` 两个扩展点。
5. `editor/import/3d/resource_importer_obj.h` — OBJ 网格导入器，示范"注册一个格式导入器"的最小套路。
6. `editor/import/resource_importer_texture_atlas.h` — 图集导入器，是唯一实现 `import_group_file()` 的（多张源图合一张）。
7. `editor/import/editor_import_plugin.h` — `EditorImportPlugin`，把整套接口以虚方法形式暴露给脚本。
8. `editor/import/import_defaults_editor.h` — 编辑器里"设置某格式默认导入选项"的面板。

## 数据流 / 调用链

一次典型导入（以贴图为例）：`EditorFileSystem` 扫描到 `icon.png` → 按后缀找到 `ResourceImporterTexture` → 读出选项 → `import()` 产出压缩好的 `CompressedTexture2D` 与 `.import` 纸条 → 运行时按纸条加载。

```mermaid
sequenceDiagram
    participant FS as EditorFileSystem
    participant RFI as ResourceFormatImporter
    participant RIT as ResourceImporterTexture
    participant Disk as .godot/imported + .import

    FS->>RFI: get_importers_for_file("icon.png")
    RFI-->>FS: [ResourceImporterTexture]
    FS->>RIT: get_import_options(path, preset)
    RIT-->>FS: List&lt;ImportOption&gt;
    Note over FS: 用户在 ImportDock 调选项
    FS->>RIT: import(source_id, "icon.png", save_path, options, ...)
    RIT->>Disk: 写 CompressedTexture2D 到 .godot/imported/
    RIT-->>FS: r_platform_variants / r_gen_files
    Note over FS: 写 "icon.png.import" 纸条<br/>(importer=texture, type, uid, path.*)
    FS->>RFI: load("icon.png")（运行时）
    RFI->>Disk: 读 icon.png.import 拿 path
    RFI-->>FS: 返回加载好的资源
```

## 中文口诀

> 源文件进门先认领，后缀对上哪个官；<br>
> 选项旋钮列一串，预设三档随便翻；<br>
> 翻译一次落磁盘，`.import` 纸条记参数；<br>
> 贴图在前场景殿后，导入顺序一百垫底办；<br>
> 运行时不用再翻译，照着纸条把货搬。

## 练习（15 分钟）

1. 打开 `resource_importer_texture.cpp`，找到 `get_recognized_extensions()`，抄下它认领的扩展名列表。
2. 在 `resource_importer_texture.cpp` 里定位 `get_import_options()`，数一数 `PRESET_2D` 和 `PRESET_3D` 各注入了几条不同选项。
3. 打开 `resource_importer_scene.h`，找出 `get_import_order()` 的返回值，并解释为什么场景导入要排最后。
4. 找任意一个带 `.import` 的项目资源，用记事本打开，对照 `core/io/resource_importer.cpp:44-155` 辨认 `importer`、`type`、`path.*`、`valid` 各字段。

## 自测

- [ ] `ResourceImporterImage` 和 `ResourceImporterTexture` 都认领图片后缀，靠哪个虚方法的值决定"到底用谁"？（提示：`ResourceImporter::get_priority()`，`core/io/resource_importer.h:126`。）
- [ ] `ResourceImporterTextureAtlas` 为什么能"一张源图也不对应一个文件"，它和 `import()` 相比多实现了哪个方法？
- [ ] `EditorImportPlugin` 暴露给脚本的那组 `_get_*`/`_import` 虚方法，和 `ResourceImporter` 的纯虚方法是什么关系？

## 一句话总结

> `editor/import` 是编辑器的"翻译车间"：十几个 `ResourceImporter` 子类各管一摊源格式，把外来文件转成 Godot 资源并留下 `.import` 参数纸条，让运行时免翻译、照着纸条直接取货。
