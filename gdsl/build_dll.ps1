# gdsl/build_dll.ps1 — 重建 gdslc.exe，重生成所有 .gdsl → .c，重编译 → .dll
$ErrorActionPreference = 'Stop'
$vcvars = 'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat'
if (-not (Test-Path -LiteralPath $vcvars)) { throw "vcvars64.bat not found: $vcvars" }

# 1. 重建 gdslc.exe（含 codegen 改动）
cmd /c "`"$vcvars`" >nul 2>&1 && cl /nologo /utf-8 /EHsc /std:c++17 toolchain\gdslc.cpp parser.cpp codegen_declarative.cpp json.cpp scene_json.cpp typecheck.cpp effect.cpp codegen_logic.cpp /Fe:toolchain\gdslc.exe"
if ($LASTEXITCODE -ne 0) { throw "gdslc compile failed (exit $LASTEXITCODE)" }

# 2. 重生成每个 .gdsl → .c
$gdslFiles = Get-ChildItem -Path example -Filter *.gdsl
foreach ($f in $gdslFiles) {
    $out = Join-Path 'example' ($f.BaseName + '.c')
    & .\toolchain\gdslc.exe logic $f.FullName $out
    if ($LASTEXITCODE -ne 0) { throw "gdslc logic failed for $($f.Name)" }
}

# 3. 重编译每个 .c → .dll
$cFiles = Get-ChildItem -Path example -Filter *.c
foreach ($f in $cFiles) {
    $out = Join-Path 'example' ($f.BaseName + '.dll')
    cmd /c "`"$vcvars`" >nul 2>&1 && cl /nologo /utf-8 /LD /Itoolchain $($f.FullName) /Fe:$out"
    if ($LASTEXITCODE -ne 0) { throw "dll compile failed for $($f.Name)" }
}

Write-Host "ALL DLLs rebuilt OK"
