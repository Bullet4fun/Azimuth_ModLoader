#ifndef FH2_AZIMUTH_LOCATE_H
#define FH2_AZIMUTH_LOCATE_H
#include "game.h"

/* Paths for a DLL loaded by azimuth.exe: module_dir = this DLL's folder (with trailing
   separator), game_exe = <game folder>\Mount\forza_durango_release_final.exe read from
   data\azimuth.ini. Both buffers hold PATH_CAPACITY characters. On failure status_set()
   explains why; module_dir is filled whenever the DLL path itself could be read. */
int azimuth_resolve_game_paths(HINSTANCE instance, wchar_t *module_dir, wchar_t *game_exe);

/* The game image Azimuth mapped into this process, or NULL (status_set() explains why); *game
   receives its FH2_GAME_* id. */
unsigned char *azimuth_find_game_image(unsigned *game);
#endif
