/* mod_declared_games on real DLLs. Usage: test_mod_games <example mod> <loader dll>
   The example declares both games; the loader exports no FH2ModGames; a missing file reads as none. */
#include "mod_games.h"
#include "fh2_mod.h"
#include <stdio.h>

int main(int argc, char **argv) {
    if (argc < 3) {
        return 2;
    }
    wchar_t example[MAX_PATH], loader[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, argv[1], -1, example, MAX_PATH);
    MultiByteToWideChar(CP_UTF8, 0, argv[2], -1, loader, MAX_PATH);
    int fails = 0;
    unsigned both = mod_declared_games(example), none = mod_declared_games(loader);
    unsigned missing = mod_declared_games(L"C:\\no\\such\\mod.dll");
    if (both != (FH2_GAME_FORZA_HORIZON_2 | FH2_GAME_FAST_AND_FURIOUS)) {
        printf("FAIL: example mod reads as %u\n", both);
        ++fails;
    }
    if (none != 0 || missing != 0) {
        printf("FAIL: undeclared/missing read as %u/%u\n", none, missing);
        ++fails;
    }
    if (!mod_file_name(L"FH2_SpotifyRadio.dll") || !mod_file_name(L"MOD.DLL") || mod_file_name(L"FH2_SpotifyRadio.dll_old") ||
        mod_file_name(L"x.dllx") || mod_file_name(L".dll")) {
        printf("FAIL: mod file names\n");
        ++fails;
    }
    printf(fails ? "%d FAILED\n" : "all passed\n", fails);
    return fails != 0;
}
