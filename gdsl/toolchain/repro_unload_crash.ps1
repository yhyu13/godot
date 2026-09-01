# repro_unload_crash.ps1 - deterministic unload_extension()+free() crash (exit 139) repro
#
# Usage:
#   powershell -File repro_unload_crash.ps1
#   powershell -File repro_unload_crash.ps1 -Engine D:/GitRepo-My/godot/bin/godot.windows.editor.x86_64.console.exe
#
# This reproduces: unload_extension() then free() a live instance -> signal 11 / exit 139.
# Plain-exit (load+instantiate+quit, no unload) is clean on both official rc3 and fork
# builds; this script targets the still-crashing unload->free path.
# Default engine is the official rc3 in .godot-bin; pass -Engine for the fork build.
param(
  [string]$Engine,
  [string]$OutDir
)

# Resolve defaults inside the body (works regardless of PS version).
if (-not $Engine) { $Engine = Join-Path $PSScriptRoot "..\..\.godot-bin\Godot_v4.7-rc3_win64_console.exe" }
if (-not $OutDir)  { $OutDir  = Join-Path $env:LOCALAPPDATA "Temp\gdsl_unload_repro" }

$ErrorActionPreference = 'Stop'
$Example = Join-Path $PSScriptRoot "..\example"

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
Copy-Item (Join-Path $Example "self_rule.dll")         $OutDir -Force
Copy-Item (Join-Path $Example "self_rule.gdextension") $OutDir -Force

# Write UTF-8 without BOM (a BOM can break Godot .gd/.tscn parsing).
function Write-Text($path, $text) {
  [System.IO.File]::WriteAllText($path, $text, (New-Object System.Text.UTF8Encoding($false)))
}

$proj = @'
; Engine configuration file.
config_version=5

[application]

config/name="gdsl unload repro"
config/features=PackedStringArray("4.7")
run/main_scene="res://main_unload.tscn"
'@
Write-Text (Join-Path $OutDir "project.godot") $proj

$scene = @'
[gd_scene load_steps=2 format=3]

[ext_resource type="GDExtension" path="res://self_rule.gdextension" id="1"]
[ext_resource type="Script" path="res://main_unload.gd" id="2"]

[node name="Root" type="Node2D"]
script = ExtResource("2")

[node name="P" type="Player" parent="."]
'@
Write-Text (Join-Path $OutDir "main_unload.tscn") $scene

$gd = @'
extends Node2D

func _ready():
	var p = get_node("P")
	print("GOT player hp=", p.hp)
	var gm = Engine.get_singleton("GDExtensionManager")
	var st = gm.unload_extension("res://self_rule.gdextension")
	print("unload status=", st)
	p.free()
	print("FREED player (unreachable if crash)")
	get_tree().quit()
'@
Write-Text (Join-Path $OutDir "main_unload.gd") $gd

Write-Host "== Repro: unload_extension + free(live instance) =="
Write-Host "== Engine: $Engine"
Write-Host "== Project: $OutDir"
& $Engine --headless --path $OutDir --scene res://main_unload.tscn
Write-Host "== Engine exit code: $LASTEXITCODE  (crash present => 0xC0000005 / -1073741819 = access violation / signal 11; bash shows 139; 0 = fixed)"
