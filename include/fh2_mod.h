#ifndef FH2_MOD_H
#define FH2_MOD_H
#include <windows.h>

/* FH2 mod loader API. The loader (data\bin\azimuth-tls.dll) stands in for Azimuth's helper,
   then loads every data\bin\mods\*.dll in name order and calls its FH2ModLoad export. */
#define FH2_MOD_API_VERSION 1

/* FH2ModHost.game: which game runs. Same engine, different builds: every RVA differs. The values
   are bit flags, so FH2_MOD_GAMES below can name several. */
#define FH2_GAME_FORZA_HORIZON_2 1   /* forza_durango_release_final.exe, timestamp 0x565501ed */
#define FH2_GAME_FAST_AND_FURIOUS 2  /* Forza Horizon 2 Presents Fast & Furious, timestamp 0x55b62fde */

typedef struct {
    unsigned size;               /* sizeof(FH2ModHost) the loader was built with */
    unsigned api_version;        /* FH2_MOD_API_VERSION */
    unsigned char *game_image;   /* forza_durango_release_final.exe as mapped by Azimuth */
    const wchar_t *game_exe;     /* that executable on disk */
    const wchar_t *settings_dir; /* data\bin\ (trailing separator), the loader's own folder */
    const wchar_t *mods_dir;     /* data\bin\mods\ (trailing separator) */
    /* This mod's own files: data\bin\mods\<DLL name without .dll>\ (trailing separator). The
       loader does not create it; a mod that keeps files creates it, others never touch it. */
    const wchar_t *data_dir;
    unsigned game; /* FH2_GAME_*: a mod built for one game must leave the other alone */
} FH2ModHost;

/* The games a mod is made for, at file scope in one of its source files:
       FH2_MOD_GAMES(FH2_GAME_FORZA_HORIZON_2);
   The loader reads this export without running any of the mod's code, and does not load a mod
   made for another game at all. A mod without it counts as Forza Horizon 2 only. */
#define FH2_MOD_GAMES(games) __declspec(dllexport) const unsigned FH2ModGames = (games)

/* Exported by every mod as FH2ModLoad. Runs inside the loader's DllMain, before the game
   starts: patch synchronously, start threads, never wait on them. Return nonzero on success. */
typedef int (*FH2ModLoadFn)(const FH2ModHost *host);
#endif
