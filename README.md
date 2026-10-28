# Azimuth Mod Loader

A mod loader for the games the [Azimuth](https://almost-there-games.itch.io/azimuth) translation
layer runs: **Forza Horizon 2**, **Forza Horizon 2 Presents Fast & Furious**, **Forza Horizon 3**,
**Forza Motorsport 5** and **Forza Motorsport 6**. It loads every DLL from the `mods` folder next
to `azimuth.exe` once Azimuth has loaded the game picked in its launcher, before any game code
runs. Mods for Forza Horizon 2 are in the [FH2 Mods Collection](https://github.com/Bullet4fun/FH2_ModsCollection) repo.

Requires Azimuth 1.0.3 or newer.

## Install

1. Put `dinput8.dll` next to `azimuth.exe`.
2. Put mods in the `mods` folder next to it.

`mods\AzimuthModLoader.log` shows the game and which mods loaded. To uninstall, delete
`dinput8.dll`.

On Linux (Proton or Wine), add this to the game's launch options so the loader's `dinput8.dll`
is used instead of Wine's own: `WINEDLLOVERRIDES="dinput8=n,b" %command%`

## Writing a mod

A mod is a 64-bit DLL that exports `AzimuthModLoad`:

```c
#include "azimuth_mod.h"

AZIMUTH_MOD_GAMES(AZIMUTH_GAME_FORZA_HORIZON_2); // games the mod supports; without it: Forza Horizon 2 only

__declspec(dllexport) int AzimuthModLoad(const AzimuthModHost *host) {
    if (!host || host->size < sizeof(*host) || host->api_version != AZIMUTH_MOD_API_VERSION) {
        return 0;
    }
    // host->game_image + RVA = a game address (image base 0x140000000 in your disassembler)
    // host->data_dir = mods\<DLL name>\ for your files (create it yourself)
    return 1;
}
```

The full API is in `include/azimuth_mod.h`; `examples/hello_mod/` is a complete example. A mod
declares the games it is made for with `AZIMUTH_MOD_GAMES`; it is never loaded in another game.
A mod that declares nothing counts as Forza Horizon 2 only.

Keep the game alive:

- `AzimuthModLoad` runs on Azimuth's thread while it loads the game: patch and return, never wait on
  a thread there.
- Check the bytes you are about to overwrite and skip the patch if they differ.
- Call FMOD's low-level API only from the game's own threads.

## Build

Python 3.10+ and [Zig 0.15.2](https://ziglang.org/download/):

```text
python build.py path\to\zig.exe              # out\dinput8.dll
python build.py path\to\zig.exe --example    # out\HelloMod.dll
python tests\run.py path\to\zig.exe           # tests
```
## AI usage disclosure

Claude was used in this project to create the mod loader.

## License

GPL-3.0. The game location code comes from
[RokkuDayo's ForzaHorizon2_ExtraOptions](https://github.com/RokkuDayo/ForzaHorizon2_ExtraOptions).
