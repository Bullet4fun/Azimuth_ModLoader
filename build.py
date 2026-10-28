"""Build the Azimuth Mod Loader (dinput8.dll) or the example mod with Zig 0.15.2.
Usage: python build.py /path/to/zig [--example] [--output PATH]
"""

import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile

SOURCE = Path(__file__).resolve().parent
LOADER_SOURCES = ["src/loader.c", "src/mod_games.c", "src/dinput8_proxy.c", "src/azimuth_locate.c", "src/support.c"]
EXAMPLE_SOURCES = ["examples/hello_mod/hello_mod.c"]
VERSION = "2.0.0"


def version_resource(compiler, directory, filename, description, product, version):
    """A VERSIONINFO resource for the DLL (file description, version, license), compiled by zig rc:
    an unsigned DLL without one looks more suspicious to antivirus heuristics."""
    numbers = ",".join(version.split(".") + ["0"])
    strings = {"CompanyName": "Bullet4fun", "FileDescription": description, "FileVersion": version,
               "InternalName": filename.rsplit(".", 1)[0], "LegalCopyright": "GPL-3.0-or-later",
               "OriginalFilename": filename, "ProductName": product, "ProductVersion": version}
    values = "\n".join(f'            VALUE "{key}", "{value}"' for key, value in strings.items())
    script = directory / "version.rc"
    script.write_text(f"""1 VERSIONINFO
FILEVERSION {numbers}
PRODUCTVERSION {numbers}
FILEOS 0x40004
FILETYPE 0x2
BEGIN
    BLOCK "StringFileInfo"
    BEGIN
        BLOCK "040904B0"
        BEGIN
{values}
        END
    END
    BLOCK "VarFileInfo"
    BEGIN
        VALUE "Translation", 0x409, 1200
    END
END
""", encoding="utf-8")
    resource = directory / "version.res"
    subprocess.run([str(compiler), "rc", "/fo", str(resource), str(script)], check=True)
    return str(resource)


def build(compiler, output, directory, example):
    # Every step runs in the build folder with relative names, and source paths are mapped to ".":
    # file paths end up in the PDB id, so where the repo or the build folder lives must not matter.
    objects = []
    for filename in EXAMPLE_SOURCES if example else LOADER_SOURCES:
        object_file = Path(filename).stem + ".obj"
        subprocess.run([str(compiler), "clang", "-target", "x86_64-w64-windows-gnu", "-c", "-ffreestanding",
                        "-fno-builtin", "-fno-stack-protector", "-fms-extensions", "-funwind-tables", "-O2",
                        "-Wall", "-Wextra", "-Werror", f'-DLOADER_VERSION="{VERSION}"', "-I", str(SOURCE / "src"), "-I", str(SOURCE / "include"),
                        "-isystem", str(compiler.parent / "lib/include"),
                        "-isystem", str(compiler.parent / "lib/libc/include/any-windows-any"),
                        f"-ffile-prefix-map={SOURCE}=.", f"-ffile-prefix-map={compiler.parent}=zig",
                        str(SOURCE / filename), "-o", object_file], check=True, cwd=directory)
        objects.append(object_file)
    libraries = []
    for name in ("kernel32", "ntdll"):  # import libraries from the .def files: no Windows SDK needed
        library = name + ".lib"
        subprocess.run([str(compiler), "dlltool", "-m", "i386:x86-64", "-d", str(SOURCE / (name + ".def")),
                        "-l", library], check=True, cwd=directory)
        libraries.append(library)
    resource = (version_resource(compiler, directory, output.name, "Azimuth Mod Loader example mod",
                                 "Azimuth Mod Loader", VERSION) if example else
                version_resource(compiler, directory, output.name, "Azimuth Mod Loader (dinput8.dll for azimuth.exe + mods)",
                                 "Azimuth Mod Loader", VERSION))
    # the last commit's time instead of the clock: the same commit gives the same bytes, so the file
    # scanned before a release is the file shipped (the public commit keeps the same time)
    timestamp = subprocess.run(["git", "-c", "safe.directory=*", "log", "-1", "--format=%ct"], cwd=SOURCE,
                               capture_output=True, text=True, check=True).stdout.strip()
    # /release (a PE checksum) and a debug directory keep antivirus ML engines quiet; the PDB
    # stays in the build folder. /Brepro: the PDB id comes from the content, not at random.
    link = [str(compiler), "lld-link", "/dll", "/entry:DllMain", "/machine:x64", "/nodefaultlib", "/dynamicbase",
            "/nxcompat", "/out:" + output.name, "/implib:out.lib", *objects, *libraries,
            Path(resource).name, "/release", "/debug", "/pdb:" + output.with_suffix(".pdb").name, "/pdbaltpath:%_PDB%",
            "/Brepro", "/timestamp:" + timestamp, "/pdbsourcepath:C:\\build"]  # not the random build folder
    subprocess.run(link, check=True, cwd=directory)
    shutil.copyfile(directory / output.name, output)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("zig", type=Path)
    parser.add_argument("--example", action="store_true", help="build examples/hello_mod instead of the loader")
    parser.add_argument("--output", type=Path)
    options = parser.parse_args()
    name = "HelloMod.dll" if options.example else "dinput8.dll"
    output = (options.output or SOURCE / "out" / name).resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="azimuth-mod-loader-build-") as temporary:
        build(options.zig.resolve(), output, Path(temporary), options.example)
    print(output)


if __name__ == "__main__":
    main()
