#ifndef FH2_MOD_GAMES_H
#define FH2_MOD_GAMES_H
#include "game.h"
#include "support.h"

/* The FH2_GAME_* flags a mod DLL exports as FH2ModGames (see fh2_mod.h), read without running any of
   its code; 0 when it exports none or cannot be read. */
unsigned mod_declared_games(const wchar_t *path);
#endif
