"""Build the loader and the example mod, then run the unit tests against them.
Usage: python tests/run.py path/to/zig.exe"""
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parent.parent


def main():
    zig = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="azimuth-loader-tests-") as tmp:
        tmp = Path(tmp)
        loader, example = tmp / "dinput8.dll", tmp / "HelloMod.dll"
        subprocess.run([sys.executable, str(ROOT / "build.py"), str(zig), "--output", str(loader)], check=True,
                       capture_output=True)
        subprocess.run([sys.executable, str(ROOT / "build.py"), str(zig), "--example", "--output", str(example)],
                       check=True, capture_output=True)
        exe = tmp / "test_mod_games.exe"
        subprocess.run([str(zig), "cc", "-I", str(ROOT / "src"), "-I", str(ROOT / "include"), "-Wall", "-Wextra", "-fms-extensions",
                        str(ROOT / "tests/test_mod_games.c"), str(ROOT / "src/mod_games.c"), str(ROOT / "src/support.c"),
                        "-o", str(exe)], check=True)
        result = subprocess.run([str(exe), str(example), str(loader)], capture_output=True, text=True)
        print("test_mod_games:", result.stdout.strip().splitlines()[-1] if result.stdout.strip() else "(no output)")
        sys.exit(result.returncode)


if __name__ == "__main__":
    main()
