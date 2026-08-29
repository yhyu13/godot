# audio（scene）

> 一句话：`scene/audio` 是 Godot 的「点歌台」——三个播放节点（`AudioStreamPlayer` / `AudioStreamPlayer2D` / `AudioStreamPlayer3D`）像三台不同的音箱，用户点一首歌（`AudioStream`），它们负责把「要不要播、播多响、在哪个声道」翻译成后端能懂的指令，再交给 `servers/audio` 的 `AudioServer` 去真正混音出声。

**结论**：这个模块定义场景侧的「播放节点」——`AudioStreamPlayer`（不空间化，平铺播放）和它的共享状态机 `AudioStreamPlayerInternal`，外加在 `scene/2d`、`scene/3d` 里的两个空间化变体 `AudioStreamPlayer2D` / `AudioStreamPlayer3D`。它为游戏作者提供 `play()` / `stop()` / `volume_db` / `bus` 这类好用的节点接口；代价是它自己**不混音、不出声**，全部实活（开流、停流、声道音量、采样播放）都转调给 `AudioServer::get_singleton()`（`servers/audio` 后端）。

## 是什么 / 不是什么

- **是什么**：播放节点层。它把「一个 `AudioStream` 资源 + 一串播放参数（音量/音高/bus/复音数）」变成一次可管理、可暂停、可停止的播放会话（`AudioStreamPlayback`），并跟随节点生命周期自动暂停/恢复/清理。
- **不是什么**：它不是混音器，也不是音频驱动——混音、总线布局、声道映射都在 `servers/audio` 的 `AudioServer` 里；`AudioStream` 与 `AudioStreamPlayback` 这两个资源类也定义在 `servers/audio/audio_stream.h`（属于后端，本文只引用不展开）。
- 对比句 3 处：`AudioStreamPlayer` 是「点歌台的遥控器」，真正拨响喇叭的是 `AudioServer`；`AudioStreamPlayerInternal` 是「记歌单的脑子」，三个公开节点是「三张不同的脸」；`scene/audio` 管「什么时候播」，`servers/audio` 管「怎么混出声音」。

## 在引擎里的位置

```mermaid
flowchart TD
    subgraph CORE["core/ 基础层"]
        Node["scene/main/node.h<br/>Node"]
        Node2D["scene/2d/node_2d.h<br/>Node2D"]
        Node3D["scene/3d/node_3d.h<br/>Node3D"]
        Obj["core/object/object.h<br/>Object"]
    end

    subgraph SCENE["scene 侧播放节点（本模块）"]
        AP["scene/audio/audio_stream_player.h<br/>AudioStreamPlayer"]
        API["scene/audio/audio_stream_player_internal.h<br/>AudioStreamPlayerInternal"]
        AP2D["scene/2d/audio_stream_player_2d.h<br/>AudioStreamPlayer2D"]
        AP3D["scene/3d/audio_stream_player_3d.h<br/>AudioStreamPlayer3D"]
    end

    subgraph SERVER["servers/audio 后端（相邻模块，本文不展开）"]
        AS["servers/audio/audio_server.h<br/>AudioServer（单例）"]
        Stream["servers/audio/audio_stream.h<br/>AudioStream / AudioStreamPlayback"]
    end

    Node -->|继承 (audio_stream_player.h:41)| AP
    Node2D -->|继承 (audio_stream_player_2d.h:41)| AP2D
    Node3D -->|继承 (audio_stream_player_3d.h:46)| AP3D
    Obj -->|继承 (audio_stream_player_internal.h:42)| API

    AP -->|"持有 internal* (audio_stream_player.h:52)"| API
    AP2D -->|"持有 internal* (audio_stream_player_2d.h:51)"| API
    AP3D -->|"持有 internal* (audio_stream_player_3d.h:72)"| API

    AP -->|"start_playback_stream 等 (audio_stream_player.cpp:114)"| AS
    API -->|"stop / is_active / pitch (internal.cpp)"| AS
    AP2D -->|"start_playback_stream (2d.cpp:69)"| AS
    AP3D -->|"start_playback_stream + bus_map (3d.cpp:293)"| AS
    AS -->|播放/混音| Stream
```

三个公开节点都各自 `memnew` 一个 `AudioStreamPlayerInternal`（如 `audio_stream_player.cpp:303`），把真正的播放状态机藏进去；对外接口几乎一行一个地把活转给 `internal` 或 `AudioServer`。

## 关键概念

1. **播放节点（点歌台）**：`AudioStreamPlayer : public Node`（`audio_stream_player.h:41`）是不空间化的纯播放器，`play()` 是唯一入口（`audio_stream_player.cpp:109`）。它没有位置概念，音量对每个声道一样。
2. **共享状态机（记歌单的脑子）**：`AudioStreamPlayerInternal : public Object`（`audio_stream_player_internal.h:42`）存真正的播放状态——`stream_playbacks`（当前活跃的播放会话列表，`internal.h:69`）、`stream`、`active` 标志、`pitch_scale` / `volume_db` / `bus` / `max_polyphony`（`internal.h:72-78`）。三个公开节点共用它，是「一个脑子、三张脸」。
3. **播放会话（一张歌单条目）**：`AudioStreamPlayback`（定义在 `servers/audio/audio_stream.h`）代表「一次正在播的声音」。`play_basic()` 里 `stream->instantiate_playback()` 造出它（`internal.cpp:149`），再 `start_playback_stream` 交给后端。
4. **空间化变体（两台会定位的音箱）**：`AudioStreamPlayer2D : public Node2D`（`audio_stream_player_2d.h:41`）和 `AudioStreamPlayer3D : public Node3D`（`audio_stream_player_3d.h:46`）在基础播放上叠加距离衰减、声道平移（panning）、（3D 还有）多普勒与发射角度，靠 `_update_panning()` 把「位置」算成每声道音量再喂给后端。

## 核心文件（按阅读顺序）

1. `scene/audio/audio_stream_player.h` — `AudioStreamPlayer` 的公开接口 + `MixTarget` 枚举（Stereo/Surround/Center），119 行，是最薄的入口。
2. `scene/audio/audio_stream_player_internal.h` — 共享状态机 `AudioStreamPlayerInternal` 的声明，含 `stream_playbacks`、`active`、播放参数，以及 `_is_sample()` 内联判断（`internal.h:64-66`）。
3. `scene/audio/audio_stream_player_internal.cpp` — 状态机实现：`play_basic()` 造播放会话、`process()` 收尾并发射 `finished`、`notification()` 跟随进树/出树/暂停生命周期。
4. `scene/audio/audio_stream_player.cpp` — 门面实现：每个公开方法一行转给 `internal` 或 `AudioServer`；`play()`（`player.cpp:109`）和 `_get_volume_vector()`（`player.cpp:183`）是理解「如何调后端」的关键。
5. `scene/2d/audio_stream_player_2d.h` / `.cpp` — 2D 空间化：`max_distance`、`attenuation`、`panning_strength`（`audio_stream_player_2d.h:73-76`），用 `_update_panning()` 算平移音量。
6. `scene/3d/audio_stream_player_3d.h` / `.cpp` — 3D 空间化：`AttenuationModel`（`audio_stream_player_3d.h:50-55`）、`DopplerTracking`（`:57-61`）、发射角度/滤波（`:107-111`），最复杂也最丰富。
7. `scene/audio/SCsub` — 一行 `env.add_source_files(env.scene_sources, "*.cpp")`，说明本目录 2 个 `.cpp` 直接进 `scene_sources`，无独立注册文件。

## 数据流 / 调用链

一次 `play()` 的典型走向（以 `AudioStreamPlayer` 为例，2D/3D 只多一步空间化）：

```mermaid
sequenceDiagram
    participant U as 用户/脚本
    participant AP as AudioStreamPlayer<br/>(门面)
    participant I as AudioStreamPlayerInternal<br/>(状态机)
    participant AS as AudioServer<br/>(servers/audio 单例)

    U->>AP: play(from_pos=0.0)<br/>(audio_stream_player.cpp:109)
    AP->>I: play_basic()<br/>(internal.cpp:140)
    I->>I: stream->instantiate_playback()<br/>(internal.cpp:149)
    I->>I: 塞进 stream_playbacks + active.set()<br/>(internal.cpp:175-177)
    I-->>AP: 返回 Ref&lt;AudioStreamPlayback&gt;
    AP->>AS: start_playback_stream(playback, bus, volume_vector, from_pos, pitch)<br/>(audio_stream_player.cpp:114)
    AP->>I: ensure_playback_limit()<br/>(internal.cpp:87)

    Note over I,AS: 每帧 internal process：清掉已结束的会话<br/>(internal.cpp:66-84)
    I->>AS: is_playback_active(playback)<br/>(internal.cpp:69)
    I->>AP: 全部结束则 emit_signal(finished)<br/>(internal.cpp:83)

    Note over I,AS: 出树/暂停：跟随节点生命周期
    I->>AS: set_playback_paused(true/false)<br/>(internal.cpp:184)
```

三个节点唯一的差别在「喂给后端什么」：`AudioStreamPlayer` 传一个均匀的 `volume_vector`（`_get_volume_vector()`，`player.cpp:183`）；`AudioStreamPlayer2D` / `AudioStreamPlayer3D` 先在 `_update_panning()` 里按位置算出带方向、带衰减的每声道/每 bus 音量，再调 `set_playback_bus_volumes_linear`（`3d.cpp:583`）或带 `bus_map` 的 `start_playback_stream`（`3d.cpp:293`）。后端 `AudioServer` 拿到这些后，才轮到混音和音频驱动发声——那部分属于 `servers/audio`。

## 中文口诀

> 点歌台三个玩家：纯、2D、3D 各一张脸；
> 一个脑子藏后面，叫 Internal 状态机；
> 点歌就是 play，instantiate 造会话；
> 交给 Server 去混音，自己只报 finished；
> 出树自动静音，进树 autoplay 才响；
> 2D 3D 多一步，把位置算成声道量。

## 练习（15 分钟）

1. 打开 `scene/audio/audio_stream_player.cpp:109` 的 `play()`，逐行念出它调了 `internal` 的哪个方法、又调了 `AudioServer` 的哪个方法。
2. 跳到 `audio_stream_player_internal.cpp:66` 的 `process()`，回答「已结束的播放会话是怎么被发现的、之后发生了什么」。
3. 对照 `internal.cpp:94` 的 `notification()`，找出 `NOTIFICATION_ENTER_TREE` / `NOTIFICATION_EXIT_TREE` / `NOTIFICATION_PREDELETE` 各干了什么。
4. 在 `scene/3d/audio_stream_player_3d.cpp` 里 grep `start_playback_stream`，比较它和 2D、纯播放器的调用参数有何不同（提示：`bus_map`）。

## 自测

- [ ] `AudioStreamPlayer`、`AudioStreamPlayer2D`、`AudioStreamPlayer3D` 分别继承谁？在哪个文件、第几行能验证？
- [ ] `AudioStream` 和 `AudioStreamPlayback` 这两个类定义在 `scene/audio` 里吗？如果不在，它们在哪个文件？
- [ ] `play()` 内部是先 `start_playback_stream` 还是先 `instantiate_playback`？`finished` 信号是谁、在哪一行发出的？
- [ ] `AudioStreamPlayerInternal` 的 `physical` 构造参数决定了什么（看 `_set_process()`，`internal.cpp:39-45`）？
- [ ] 3D 播放器的 `start_playback_stream` 比 2D 多传了哪两个衰减/滤波参数（看 `3d.cpp:293`）？

## 一句话总结

> `scene/audio` 是 Godot 的「点歌台」：三个播放节点管「什么时候播、播多响、在哪个声道」，共享的 `AudioStreamPlayerInternal` 管「一次播放的生死」，而真正混音出声的脏活全部转给 `servers/audio` 的 `AudioServer` 单例。
