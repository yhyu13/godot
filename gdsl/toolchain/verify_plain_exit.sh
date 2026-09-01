#!/usr/bin/env bash
# verify_plain_exit.sh - assert the segfault fix: all 5 GDSL samples exit clean.
#
# Loads each sample's .gdextension, instantiates its Player class, quits, and
# REQUIRES the engine exit code to be 0. Any non-zero = FAIL (the fix regressed).
# This closes acceptance signal #5 of the Era-21 segfault bar (a committed,
# re-runnable "assert exit 0" repro).
#
# Usage: bash gdsl/toolchain/verify_plain_exit.sh [-engine <path>]
#   Default engine = fork release-editor build (contains the 54864dd fix).
set -u

EXAMPLE="/d/GitRepo-My/godot/gdsl/example"
ENGINE="${ENGINE:-D:/GitRepo-My/godot/bin/godot.windows.editor.x86_64.console.exe}"
BASE="$LOCALAPPDATA/Temp/gdsl_verify_$$"
samples="self_rule target_only emit_only combo minimal"
fail=0

make_project() {
  local s="$1"; local dir="$BASE/$s"
  mkdir -p "$dir"
  cp "$EXAMPLE/$s.dll" "$dir/"
  if [ -f "$EXAMPLE/$s.gdextension" ]; then
    cp "$EXAMPLE/$s.gdextension" "$dir/$s.gdextension"
  else
    printf '[configuration]\n\nentry_symbol = "gdsl_library_init"\ncompatibility_minimum = "4.7"\nreloadable = true\n\n[libraries]\n\nwindows.x86_64 = "res://%s.dll"\n' "$s" > "$dir/$s.gdextension"
  fi
  printf '; Engine config\nconfig_version=5\n\n[application]\n\nconfig/name="gdsl verify %s"\nconfig/features=PackedStringArray("4.7")\nrun/main_scene="res://main.tscn"\n' "$s" > "$dir/project.godot"
  printf '[gd_scene load_steps=3 format=3]\n\n[ext_resource type="GDExtension" path="res://%s.gdextension" id="1"]\n[ext_resource type="Script" path="res://main.gd" id="2"]\n\n[node name="Root" type="Node2D"]\nscript = ExtResource("2")\n\n[node name="P" type="Player" parent="."]\n' "$s" > "$dir/main.tscn"
  printf 'extends Node2D\n\nfunc _ready():\n\tprint("CREATED player hp=", $P.hp)\n\tget_tree().quit()\n' > "$dir/main.gd"
}

echo "== Engine: $ENGINE =="
for s in $samples; do
  make_project "$s"
  OUT=$("$ENGINE" --headless --path "$BASE/$s" 2>&1)
  CODE=$?
  CRE=$(printf '%s' "$OUT" | grep -c CREATED)
  echo "  $s: exit=$CODE created=$CRE"
  if [ "$CODE" != "0" ] || [ "$CRE" != "1" ]; then fail=1; fi
done

echo "=== RESULT: $([ "$fail" = "0" ] && echo 'PASS - all 5 samples exit 0' || echo 'FAIL - at least one sample crashed') ==="
exit "$fail"
