#!/usr/bin/env python3
"""FR-011 playtest runner — 验证一个 .gdsl 配方在引擎里「真的跑对」。

输入一个配方 .gdsl + 一个 GDScript 断言脚本，走完整管线：
    .gdsl → (gdslc) → .c → (cl /LD) → .dll → (.gdextension manifest) → Godot 引擎
    → 加载扩展 + 跑断言脚本 → 打印引擎输出（含评测 assert 结果）。

用词说明：这是「运行时正确」的判据 —— 区别于 010 的「编译期有效」(gdslc parse+typecheck)。
用途：FR-011 playtest 反馈闭环的「跑」这一半 —— 看一个编译通过的配方，规则到底有没有真的执行。

用法（仓库根）：
    python3 gdsl/toolchain/playtest.py --gdsl <recipe.gdsl> --name <ext> --script <playtest.gd>
"""
import argparse
import os
import shutil
import subprocess
import sys
import tempfile

REPO = r"D:\GitRepo-My\godot"
GDSLC = os.path.join(REPO, "gdsl", "toolchain", "gdslc.exe")
ENGINE = os.path.join(REPO, ".godot-bin", "Godot_v4.7-rc3_win64_console.exe")
VCRUNTIME = r"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
GDSL_TOOLCHAIN = os.path.join(REPO, "gdsl", "toolchain")  # cl 的 -I 指向 stub 头目录


def run(cmd, cwd=None, timeout=180):
    r = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True,
                       encoding="utf-8", errors="replace", timeout=timeout)
    return r


def build_dll(gdsl_path, workdir):
    """gdslc logic → .c, 然后 cl /LD → .dll."""
    src = os.path.basename(gdsl_path)
    stem = os.path.splitext(src)[0]
    c_path = os.path.join(workdir, stem + ".c")
    dll_path = os.path.join(workdir, stem + ".dll")
    r = run([GDSLC, "logic", gdsl_path, c_path])
    if r.returncode != 0:
        return None, "gdslc failed: " + (r.stderr + r.stdout)[:400]
    # cl 需要 vcvars —— 用临时 ps1 包一层（cmd //c 在此 shell 不可靠）
    ps1 = os.path.join(workdir, "_cl.ps1")
    with open(ps1, "w") as f:
        # 路径无空格 → cl 参数裸写，避免双层引号打断 PS 字符串（唯一引号是 `"$vcvars`" 那对）
        f.write('$vcvars = "%s"\ncmd /c "`"$vcvars`" >nul 2>&1 && cl /nologo /utf-8 /LD /I%s %s /Fe:%s /Fo:%s.obj"\nif ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }\n'
                % (VCRUNTIME, GDSL_TOOLCHAIN, c_path, dll_path, dll_path))
    r2 = subprocess.run(["powershell", "-ExecutionPolicy", "Bypass", "-File", ps1],
                        cwd=REPO, capture_output=True, text=True,
                        encoding="utf-8", errors="replace", timeout=180)
    if r2.returncode != 0:
        return None, "cl /LD failed: " + ((r2.stdout or "") + (r2.stderr or ""))[:400]
    return dll_path, None


def run_engine(project_dir):
    r = subprocess.run([ENGINE, "--editor", "--headless", "--path", project_dir],
                       capture_output=True, text=True,
                       encoding="utf-8", errors="replace", timeout=240)
    return (r.stdout or "") + (r.stderr or "")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--gdsl", required=True, help="配方 .gdsl 路径")
    ap.add_argument("--name", required=True, help="扩展名/类型前缀（用作 .dll/.gdextension 名）")
    ap.add_argument("--script", required=True, help="GDScript 断言脚本路径")
    args = ap.parse_args()

    with tempfile.TemporaryDirectory(prefix="gdslplay_") as wd:
        dll_path, err = build_dll(args.gdsl, wd)
        if err:
            print("BUILD FAIL:", err)
            return 1
        stem = args.name
        # build_dll 产出 wd/<stem>.dll（与 manifest res://<stem>.dll 同名），无需再 copy
        with open(args.script) as f:
            playtest_gd = f.read()
        # 写 project（manifest 引用 res://<stem>.dll，跟之前 string_test 一致）
        with open(os.path.join(wd, stem + ".gdextension"), "w") as f:
            f.write("[configuration]\n\nentry_symbol = \"gdsl_library_init\"\ncompatibility_minimum = \"4.7\"\n"
                    "reloadable = true\n\n[libraries]\n\nwindows.x86_64 = \"res://%s.dll\"\n" % stem)
        with open(os.path.join(wd, "project.godot"), "w") as f:
            f.write('config_version=5\n\n[application]\n\nconfig/name="gdsl playtest"\n'
                    'config/features=PackedStringArray("4.7")\n\n[autoload]\n\nPT="*res://playtest.gd"\n')
        with open(os.path.join(wd, "playtest.gd"), "w") as f:
            f.write(playtest_gd)
        with open(os.path.join(wd, stem + ".gdsl"), "w") as f:
            f.write(open(args.gdsl).read())
        print("=== running engine playtest (ext=%s) ===" % stem)
        out = run_engine(wd)
        # 过滤出关键行
        for line in out.splitlines():
            ls = line.strip()
            if any(key in ls for key in ("hp=", "speed=", "nickname=", "PASS", "FAIL",
                                         "assert", "ERROR", "error", "after", "initial")):
                print(ls[:200])
    return 0


if __name__ == "__main__":
    sys.exit(main())
