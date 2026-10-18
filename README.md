# FH2 Mod Loader

A mod loader for **Forza Horizon 2** and **Forza Horizon 2 Presents Fast & Furious** on the Azimuth
translation layer. It loads every DLL from `data\bin\mods\` before the game starts. Several mods are available in the [FH2 Mods Collection](https://github.com/Bullet4fun/FH2_ModsCollection) repo.

## Install

1. Back up `data\bin\azimuth-tls.dll`.
2. Replace it with the loader's `azimuth-tls.dll`.
3. Put mods in `data\bin\mods\`.

`data\bin\FH2_ModLoader.log` shows which mods loaded. To uninstall, restore the original
`azimuth-tls.dll`.

## Writing a mod

A mod is a 64-bit DLL that exports `FH2ModLoad`:

```c
#include "fh2_mod.h"

FH2_MOD_GAMES(FH2_GAME_FORZA_HORIZON_2); // games the mod supports; without it: Forza Horizon 2 only

__declspec(dllexport) int FH2ModLoad(const FH2ModHost *host) {
    if (!host || host->size < sizeof(*host) || host->api_version != FH2_MOD_API_VERSION) {
        return 0;
    }
    // host->game_image + RVA = a game address (image base 0x140000000 in your disassembler)
    // host->data_dir = data\bin\mods\<DLL name>\ for your files (create it yourself)
    return 1;
}
```

The full API is in `include/fh2_mod.h`; `examples/hello_mod/` is a complete example. A mod for
another game is never loaded.

Keep the game alive:

- `FH2ModLoad` runs inside `DllMain`: patch and return, never wait on a thread there.
- Check the bytes you are about to overwrite and skip the patch if they differ.
- Call FMOD's low-level API only from the game's own threads.

## Build

Python 3.10+ and [Zig 0.15.2](https://ziglang.org/download/):

```text
python build.py path\to\zig.exe              # out\azimuth-tls.dll
python build.py path\to\zig.exe --example    # out\FH2_HelloMod.dll
python tests\run.py path\to\zig.exe           # tests
```
## AI usage disclosure

Claude was used in this project to create the mod loader.

## License

GPL-3.0. The Azimuth helper and game location code come from
[RokkuDayo's ForzaHorizon2_ExtraOptions](https://github.com/RokkuDayo/ForzaHorizon2_ExtraOptions).
