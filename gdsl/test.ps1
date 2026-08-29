# gdsl/test.ps1 — 编译并运行 gdsl 单元测试（MSVC，秒级 red-green 循环）
$ErrorActionPreference = 'Stop'
$vcvars = 'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat'
if (-not (Test-Path -LiteralPath $vcvars)) { throw "vcvars64.bat not found: $vcvars" }

cmd /c "`"$vcvars`" >nul 2>&1 && cl /nologo /utf-8 /EHsc /std:c++17 /I`"..\thirdparty\doctest`" test_parser.cpp test_declarative.cpp test_typecheck.cpp test_codegen.cpp parser.cpp codegen_declarative.cpp json.cpp scene_json.cpp typecheck.cpp codegen_logic.cpp /Fe:test_gdsl.exe"
if ($LASTEXITCODE -ne 0) { throw "compile failed (exit $LASTEXITCODE)" }

& ".\test_gdsl.exe"
exit $LASTEXITCODE
