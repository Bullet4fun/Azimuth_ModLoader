#ifndef AZIMUTH_LOADER_LOCATE_H
#define AZIMUTH_LOADER_LOCATE_H
#include "game.h"

/* Inside azimuth.exe: mods_dir = <azimuth folder>\mods\ (with trailing separator,
   PATH_CAPACITY characters). Returns 0 in any other process. */
int azimuth_mods_dir(wchar_t *mods_dir);

/* The game Azimuth has mapped into this process, whichever one was picked in the launcher: the
   image whose PE timestamp and size match the executable of a game data\azimuth.ini lists (its
   Mount\AppxManifest.xml names the file). Fills game_exe (PATH_CAPACITY characters) and *game
   (AZIMUTH_GAME_*). NULL if none is mapped (status_set() explains why). Call after
   azimuth_mods_dir(). */
unsigned char *azimuth_find_game(wchar_t *game_exe, unsigned *game);

/* "Forza Horizon 2" etc. for an AZIMUTH_GAME_* id. */
const char *azimuth_game_name(unsigned game);
#endif
