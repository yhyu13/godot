#!/usr/bin/env python3
"""Generate gdsl/godot_classes.h from the engine's extension_api.json.

Godot dumps extension_api.json via `--dump-extension-api`; in this fork it is
checked in at the repo root. This script extracts every ClassDB-registered class
name into a sorted static C array + binary-search lookup, so the gdsl typechecker
can reject a hallucinated/typo'd `@extends <base>` at COMPILE time (Gap B) instead
of letting it crash at runtime in classdb_construct_object.

Usage (from repo root):
    python3 gdsl/gen_godot_classes.py

Regenerate whenever the engine class list changes (version bump). The header is
generated -> do not hand-edit.
"""
import json
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "extension_api.json")
OUT = os.path.join(ROOT, "gdsl", "godot_classes.h")


def main() -> None:
    with open(SRC, encoding="utf-8") as f:
        api = json.load(f)
    classes = sorted({c["name"] for c in api["classes"]})

    lines = [
        "// gdsl/godot_classes.h — generated from extension_api.json (Godot 4.7) by gdsl/gen_godot_classes.py.",
        "// Do not hand-edit. ClassDB-registered Godot class names, for validating '@extends <base>'.",
        "#pragma once",
        "#include <string>",
        "namespace gdsl {",
        "static const char *const k_godot_classes[] = {",
    ]
    line = ""
    for c in classes:
        tok = '"%s",' % c
        if len(line) + len(tok) > 118:
            lines.append(line.rstrip())
            line = "  " + tok + " "
        else:
            line += tok + " "
    if line.strip():
        lines.append(line.rstrip())
    lines += [
        "};",
        "static const int k_godot_class_count = %d;" % len(classes),
        "inline bool is_godot_class(const std::string &name) {",
        "  int lo = 0, hi = k_godot_class_count - 1;",
        "  while (lo <= hi) {",
        "    int mid = lo + (hi - lo) / 2;",
        "    const std::string &s = k_godot_classes[mid];",
        "    if (s == name) return true;",
        "    if (s < name) lo = mid + 1; else hi = mid - 1;",
        "  }",
        "  return false;",
        "}",
        "} // namespace gdsl",
        "",
    ]
    with open(OUT, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))
    print("wrote %s (%d classes)" % (OUT, len(classes)))


if __name__ == "__main__":
    main()
