# OpenWolf

@.wolf/OPENWOLF.md

This project uses OpenWolf for context management. Read and follow .wolf/OPENWOLF.md every session. Check .wolf/cerebrum.md before generating code. Check .wolf/anatomy.md before reading files.


# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

The [Godot Engine](https://godotengine.org) source code — a cross-platform 2D/3D game engine (C++17 core, MIT license). The editor is itself a Godot application built on the same engine. This checkout is the `4.7` development branch.

## Build system (SCons)

Godot builds with **SCons** (Python-based), not CMake/Make. The build graph is described by `SConstruct` at the root plus `SCsub` files scattered throughout the tree, with helper logic in the root-level `methods.py`, `platform_methods.py`, and `*_builders.py` files. Python tooling is type-checked/linted with mypy + ruff (see `pyproject.toml`).

Common commands (from the repo root):

```bash
# Editor build (default target), Windows example — use your own platform
scons platform=windows target=editor -j8

# Common flags
scons platform=windows target=editor dev_build=yes          # DEV_ENABLED, more assertions
scons platform=windows target=editor tests=yes              # build the unit-test binary too
scons platform=windows target=editor module_<name>_enabled=no  # disable a module
scons platform=windows target=editor compiledb=yes          # emit compile_commands.json (for clangd/clang-tidy)
scons platform=windows target=editor ninja=yes              # use the ninja backend
```

Key options: `platform` (default `""` = auto-detect host), `target` ∈ `editor` / `template_debug` / `template_release`, `arch`, `dev_build`, `tests`, `deprecated`, `module_<name>_enabled`, `vulkan`/`opengl3`/`d3d12`/`metal`, `compiledb`, `ninja`. `dev_mode=yes` is an alias for `verbose=yes warnings=extra werror=yes tests=yes strict_checks=yes`.

Run the built binary from `bin/` (the exact name encodes platform/target/arch, e.g. `bin/godot.windows.editor.x86_64.exe`).

## Tests

Unit tests use **doctest**, compiled into a test binary via `tests=yes`. Tests live in `tests/` (mirroring the `core/`, `scene/`, `servers/` layout) and are registered with `TEST_CASE` / `TEST_SUITE` macros defined in [tests/test_macros.h](tests/test_macros.h); `tests/SCsub` + `tests/create_test.py` wire them up. `tests/test_main.cpp` is the doctest runner.

```bash
# Build with tests, then run the test binary with --test
scons platform=windows target=editor tests=yes
./bin/godot.windows.editor.x86_64.exe --test

# doctest CLI flags are passed through — filter a single case/suite (wildcards supported)
./bin/godot.windows.editor.x86_64.exe --test --test-case="String*"
./bin/godot.windows.editor.x86_64.exe --test --test-suite="core/math"
```

## Linting & formatting

Formatting/lint runs through **pre-commit** (`.pre-commit-config.yaml`): `clang-format` (C/C++/GLSL), `ruff` + `mypy` (Python/SCons), `codespell`, plus a JSON-schema check for the GDExtension interface. Run `pre-commit run --all-files` (or `pre-commit install` to set up git hooks). `clang-tidy` is registered at the `manual` stage only because it needs an up-to-date `compile_commands.json`:

```bash
scons platform=windows target=editor compiledb=yes
pre-commit run --hook-stage manual clang-tidy
```

## Architecture

The engine is a layered stack. Understanding this layering is the key to navigating the code:

- **`core/`** — the foundation and the C++↔scripting boundary. `Variant`, `String`/string types, `Object` (refcounting, signals), math (`core/math/`), containers, `Ref<T>`/`RID`, and the class binding system (`core_bind.*`, `ClassDB`) that exposes C++ to GDScript/C#/GDExtension. Almost nothing else compiles without this layer.
- **`scene/`** — the high-level game API: `Node`/`SceneTree`, `Resource` and its subtypes, 2D/3D nodes, GUI controls. This is where most user-facing engine types live.
- **`servers/`** — the **"servers" pattern**. Servers are singleton backends (rendering, physics, audio, navigation, etc.) that scene nodes talk to, holding the actual data and doing the heavy lifting. `servers/rendering/` contains the `RenderingDevice` abstraction (Vulkan/D3D12/Metal/GLES3). New systems are typically split: a scene-side frontend node + a `servers/` backend.
- **`editor/`** — the editor, built as a Godot application on top of the engine (node-based docks, inspectors, importers).
- **`modules/`** — optional, self-contained features compiled in/out via `module_<name>_enabled`: GDScript, Mono/C#, physics engines (GodotPhysics + Jolt), image/audio importers, networking, etc. Each module registers itself via `register_module_types.h` and its own `SCsub`.
- **`drivers/`** — low-level backend drivers (rendering APIs, audio, input, platform-specific).
- **`platform/`** — OS/platform porting and export code (windows, linuxbsd, macos, android, web, etc.).
- **`main/`** — engine entry point and the main loop ([main/main.cpp](main/main.cpp)).
- **`thirdparty/`** — vendored dependencies (never edit directly).

## Contribution conventions

From [CONTRIBUTING.md](CONTRIBUTING.md) — these are enforced, not advisory:

- Commit messages: imperative first line under 72 chars ("Fix …", "Add …", optionally prefixed like "Core: …"), extended description wrapped at 80 chars. Fixes use GitHub closing keywords in the PR description, not the commit title.
- **Any PR adding/changing scripting-exposed methods, properties, or signals must update the class reference XML** in `doc/classes/` (generated via the compiled binary's `--doctool`, then hand-described).
- Code style and C++ usage rules live in the Godot contributing docs (linked from CONTRIBUTING.md); `clang-format` config is `.clang-format`.
- Bug reports require a Minimal Reproduction Project; feature proposals go to `godotengine/godot-proposals`, not the main issue tracker.

## GDScript binding specifics

Engine classes exposed to scripts are registered through `ClassDB::bind_method(...)` and macros like `BIND_ENUM_CONSTANT`. When you add or change a class's script-visible surface, the generated bindings (`core/core_bind.*`, `scene/...`, `.gen.inc` / `*.compat.inc` files) are regenerated at build time and the `doc/classes/*.xml` reference must be updated with `./bin/godot.* --doctool`.

## Module teaching docs (144, in Chinese)

Every one of Godot's 144 subsystems (`core/`, `scene/`, `servers/`, `editor/`, `modules/`, `drivers/`, `platform/`) has a Chinese teaching doc — conclusion-first, source-anchored (`file:line`), with mermaid diagrams and 口诀/练习/自测. Index: [`docs/INDEX.md`](docs/INDEX.md) (grouped by layer, clickable); writing spec: [`docs/DOC_SPEC.md`](docs/DOC_SPEC.md). Check the index before reading a module's source — the doc may already answer it.
