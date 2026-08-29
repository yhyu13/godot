# resources（scene）

> 一句话：`scene/resources` 是 Godot 的「可存盘资产仓库」——所有能写进 `.tres`/`.res`/`.scn` 文件、能被按路径重新加载的数据类型（Texture、Mesh、Material、Shader、PackedScene…）都住在这里，本质是 `core/io/Resource` 的一堆子类。

**结论**：`scene/resources` 定义引擎里所有「可序列化资产」的数据结构与存/取协议（约 130 个源文件、175 处 `GDCLASS` 类声明），为上层 Node 提供数据、把真正的渲染/物理计算交给 `servers` 层；代价是每个资源类都是「哑数据容器」，必须自己实现序列化与校验，还多一层「逻辑资源 → 服务器 RID」的惰性映射。

## 是什么 / 不是什么

`scene/resources` 负责「资产长什么样、怎么落盘、怎么读回」，也就是数据容器与序列化入口；它不负责「怎么画」——`Texture::draw()` 只把像素数据转成 RID 交给 RenderingServer（`scene/resources/texture.h:74`），真正的采样、混合、光栅化在 `servers/rendering/`。物理形状（`Shape2D`/`Shape3D`）在这里存数据，碰撞检测交给 `servers/physics_2d`/`physics_3d`。引用计数与内存回收也不在这层，那是 `core/object/ref_counted.h` 的 `RefCounted` 干的活。

## 在引擎里的位置

```mermaid
flowchart TB
    CORE["core/io<br/>Resource / RefCounted<br/>ResourceLoader / ResourceSaver"]
    SR["scene/resources<br/>Texture · Mesh · Material<br/>Shader · PackedScene · AudioStreamWAV · …"]
    NODE["scene/main · scene/2d · scene/3d<br/>Node 树（Sprite3D、MeshInstance3D…）"]
    SRV["servers<br/>RenderingServer / PhysicsServer<br/>AudioServer"]

    CORE -->|继承 + 序列化协议| SR
    SR -->|持有 Ref&lt;Resource&gt;| NODE
    SR -->|get_rid() 惰性提交| SRV
    NODE -->|运行期引用| SRV
```

资源类继承链是单向的：`RefCounted` ← `Resource` ← 具体资产类；资产类从不反向依赖 Node，只通过 `Ref<>` 被 Node 引用。

## 关键概念

- **Resource 本体**：所有资产的根。像「能自己存进保险箱并凭条取回的文件」——基类 `Resource : public RefCounted`（`core/io/resource.h:52`）持有 `path_cache`（路径）、`name`（资源名），并提供 `duplicate()`/`duplicate_deep()`（`resource.h:154-155`）、`emit_changed()`（变更通知）和 `reload_from_file()`（热重载）。
- **按扩展名找读写器**：`ResourceLoader::load()`（`core/io/resource_loader.h:259`）像「按文件后缀派单」，把 `.tres` 交给 `ResourceFormatLoaderText`、把 `.shader` 交给 `ResourceFormatLoaderShader`。这些 loader/saver 在 `register_scene_types()` 里用 `ResourceLoader::add_resource_format_loader()` 注册（`scene/register_scene_types.cpp:414-434`）。
- **RID 惰性映射**：资源类只存 CPU 侧数据和一个 `RID` 句柄，真正的 GPU/服务器对象等到 `get_rid()` 被调用时才向服务器提交。比如 `Material` 持有 `mutable RID material`（`scene/resources/material.h:43`），`get_shader_rid()` 是虚函数由子类实现。
- **场景也是资源**：`PackedScene : public Resource`（`scene/resources/packed_scene.h:246`）把一棵节点树拍平成 `SceneState`（名字表 + 变体表 + 节点表），`instantiate()` 再按表重建节点树——这是 `.tscn` 与 `.scn` 的同一套骨架。
- **场景局部资源**：`local_to_scene` 标记的资源（`core/io/resource.h:88`）不共享，每个实例化场景拿到自己的副本，靠 `duplicate_for_local_scene()` 完成（`resource.h:158`）。

## 核心文件（按阅读顺序）

1. `core/io/resource.h` — `Resource` 基类：引用计数、路径缓存、复制/热重载、`RES_BASE_EXTENSION` 宏（第 41 行）定义每类资源的文件后缀。
2. `scene/resources/texture.h` — 纹理层级：`Texture` → `Texture2D` / `TextureLayered` / `Texture3D`（第 37/41/86/117 行）。
3. `scene/resources/material.h` — 材质层级：`Material` → `ShaderMaterial`、`BaseMaterial3D`（含 `StandardMaterial3D`/`ORMMaterial3D`）、`PlaceholderMaterial`。
4. `scene/resources/mesh.h` — 网格层级：`Mesh` → `ArrayMesh`、`PlaceholderMesh`；`3d/primitive_meshes.h` 里是 `PrimitiveMesh` 家族。
5. `scene/resources/shader.h` — `Shader`：持有着色器源码 + 编译缓存 RID。
6. `scene/resources/packed_scene.h` — `SceneState` 与 `PackedScene`：场景序列化的数据表与实例化逻辑。
7. `scene/resources/audio_stream_wav.h` — `AudioStreamWAV`（继承 `servers/audio/audio_stream.h:157` 的 `AudioStream`）：一种具体音频资产的写法示范。
8. `scene/resources/resource_format_text.h` — `ResourceFormatLoaderText`/`ResourceFormatSaverText`：`.tres` 文本格式的读写器。
9. `scene/register_scene_types.cpp` — 注册入口：`register_scene_types()` 挂载 loader/saver 并用 `GDREGISTER_CLASS`/`GDREGISTER_VIRTUAL_CLASS` 注册类（第 390 行起）。

## 数据流 / 调用链

以「加载一个 `.tres` 材质」为例，`ResourceLoader` 走一遍「派单 → 解析 → 缓存」：

```mermaid
sequenceDiagram
    participant G as GDScript / Node
    participant RL as ResourceLoader
    participant FL as ResourceFormatLoaderText
    participant RC as ResourceCache

    G->>RL: load("res://mat.tres")
    RL->>RL: 按扩展名选 loader（已注册）
    RL->>FL: load(path)
    FL->>FL: 解析文本 → 构造 Material 并 set 属性
    FL-->>RL: 返回 Ref&lt;Material&gt;
    RL->>RC: get_ref(path) 缓存并 set_path
    RL-->>G: Ref&lt;Material&gt;
    G->>G: 使用 / emit_changed 通知
```

保存是反向的：`ResourceSaver::save()` 选 `ResourceFormatSaverText`，把资源属性递归写成 `.tres` 文本。

## 中文口诀

- 资产皆 Resource，继承 RefCounted 数引用。
- 扩展名派单，Loader 读来 Saver 写。
- 数据在资源，像素在服务器，中间只隔一个 RID。
- 场景是资源，节点拍成表，实例化再重建。
- 局部资源不共享，duplicate 各拿各的。
- 改属性记得 emit_changed，热重载靠 reload_from_file。

## 练习（15 分钟）

1. 打开 `scene/resources/texture.h`，画出 `Texture → Texture2D → ImageTexture` 的继承链，确认每一级 `GDCLASS` 的父类名。
2. 在 `scene/register_scene_types.cpp` 找 `add_resource_format_loader`，列出本模块注册的 4 组 loader/saver 各对应什么文件后缀。
3. 读 `scene/resources/material.h` 第 43-45 行，说明 `material`、`next_pass`、`render_priority` 三个成员各是什么用途。
4. 读 `scene/resources/packed_scene.h` 的 `SceneState` 成员（第 41-46 行），说明它是怎么用「表」来表达一棵节点树的。

## 自测

- [ ] `Resource` 的引用计数来自哪个基类？`RES_BASE_EXTENSION` 宏做的事，与 `ResourceLoader` 的派单逻辑如何串起来？
- [ ] `Texture2D::draw()` 为什么不直接画像素？它最终把工作交给了谁？
- [ ] `PackedScene::instantiate()` 与 `SceneState::instantiate()` 的分工是什么（谁存表、谁建树）？

## 一句话总结

> `scene/resources` 是 Godot 的资产数据层：定义「可存盘」的数据结构与存/取协议，让上层 Node 与 `servers` 服务器各干各的，中间只靠 `Resource` + `Ref<>` + `RID` 三件套衔接。
