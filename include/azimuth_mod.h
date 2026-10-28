#ifndef AZIMUTH_MOD_H
#define AZIMUTH_MOD_H
#include <windows.h>

/* Azimuth Mod Loader API. The loader (dinput8.dll beside azimuth.exe) waits for Azimuth to map
   the game picked in its launcher, then loads every mods\*.dll (mods beside azimuth.exe) in name
   order and calls its AzimuthModLoad export. */
#define AZIMUTH_MOD_API_VERSION 2

/* AzimuthModHost.game: which of the games Azimuth runs is running. Each is a different build:
   every RVA differs. The values are bit flags, so AZIMUTH_MOD_GAMES below can name several.
   Comments: Azimuth's title id, the executable's PE timestamp. */
#define AZIMUTH_GAME_FORZA_HORIZON_2 1     /* 1a96545b, 0x565501ed */
#define AZIMUTH_GAME_FAST_AND_FURIOUS 2    /* 5a9771a4, 0x55b62fde: Forza Horizon 2 Presents Fast & Furious */
#define AZIMUTH_GAME_FORZA_HORIZON_3 4     /* 4ce1dfab */
#define AZIMUTH_GAME_FORZA_MOTORSPORT_5 8  /* 7b35d917, 0x53d8d1f2 */
#define AZIMUTH_GAME_FORZA_MOTORSPORT_6 16 /* 64f9397e */

typedef struct {
    unsigned size;               /* sizeof(AzimuthModHost) the loader was built with */
    unsigned api_version;        /* AZIMUTH_MOD_API_VERSION */
    unsigned char *game_image;   /* the game executable as mapped by Azimuth */
    const wchar_t *game_exe;     /* that executable on disk */
    const wchar_t *mods_dir;     /* <azimuth folder>\mods\ (trailing separator) */
    /* This mod's own files: mods\<DLL name without .dll>\ (trailing separator). The
       loader does not create it; a mod that keeps files creates it, others never touch it. */
    const wchar_t *data_dir;
    unsigned game; /* AZIMUTH_GAME_*: a mod built for one game must leave the others alone */
} AzimuthModHost;

/* The games a mod is made for, at file scope in one of its source files:
       AZIMUTH_MOD_GAMES(AZIMUTH_GAME_FORZA_HORIZON_2);
   The loader reads this export without running any of the mod's code, and does not load a mod
   made for another game at all. A mod without it counts as Forza Horizon 2 only. */
#define AZIMUTH_MOD_GAMES(games) __declspec(dllexport) const unsigned AzimuthModGames = (games)

/* Exported by every mod as AzimuthModLoad. Runs on Azimuth's thread once the game is mapped,
   before any game code runs (the mod's own DllMain has returned): patch synchronously, start
   threads. Return nonzero on success. */
typedef int (*AzimuthModLoadFn)(const AzimuthModHost *host);
#endif
