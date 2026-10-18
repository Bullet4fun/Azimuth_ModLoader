/* FH2 mod loader: data\bin\azimuth-tls.dll. Provides Azimuth's helper ABI (azimuth_tls.c),
   finds the FH2 image Azimuth mapped (azimuth_locate.c), then loads data\bin\mods\*.dll in name
   order and calls each FH2ModLoad. Writes data\bin\FH2_ModLoader.log. Kernel32 only. */
#include "azimuth_locate.h"
#include "fh2_mod.h"
#include "mod_games.h"
#include "support.h"

#define MAX_MODS 64

extern DWORD azimuth_tls_attached;
static wchar_t settings_dir[PATH_CAPACITY], game_exe[PATH_CAPACITY], mods_dir[PATH_CAPACITY];
static wchar_t pattern[PATH_CAPACITY], mod_path[PATH_CAPACITY], log_path[PATH_CAPACITY];
static wchar_t data_dir[PATH_CAPACITY], data_name[MAX_PATH + 1];
static wchar_t names[MAX_MODS][MAX_PATH];
static HANDLE log_file = INVALID_HANDLE_VALUE;

static void log_text(const char *text) {
    DWORD written;
    if (log_file != INVALID_HANDLE_VALUE) {
        WriteFile(log_file, text, (DWORD)text_length(text), &written, NULL);
    }
}

static void log_wide(const wchar_t *text) { // mod file names: ASCII, anything else as '?'
    char buffer[MAX_PATH];
    size_t n = 0;
    for (; text[n] && n < sizeof(buffer) - 1; ++n) {
        buffer[n] = text[n] < 0x80 ? (char)text[n] : '?';
    }
    buffer[n] = 0;
    log_text(buffer);
}

static int name_before(const wchar_t *a, const wchar_t *b) { // case-insensitive, for load order
    for (;; ++a, ++b) {
        wchar_t x = *a >= L'A' && *a <= L'Z' ? *a + 32 : *a, y = *b >= L'A' && *b <= L'Z' ? *b + 32 : *b;
        if (x != y || !x) {
            return x < y;
        }
    }
}

static void load_mods(unsigned char *image, unsigned game) {
    if (!path_join(mods_dir, settings_dir, L"mods\\") || !path_join(pattern, mods_dir, L"*.dll")) {
        log_text("paths too long; no mods loaded\r\n");
        return;
    }
    unsigned count = 0;
    WIN32_FIND_DATAW found;
    HANDLE search = FindFirstFileW(pattern, &found);
    if (search != INVALID_HANDLE_VALUE) {
        do {
            if (!(found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && count < MAX_MODS) {
                unsigned i = count++;
                for (; i > 0 && name_before(found.cFileName, names[i - 1]); --i) {
                    memory_copy(names[i], names[i - 1], sizeof(names[i]));
                }
                memory_copy(names[i], found.cFileName, sizeof(names[i]));
            }
        } while (FindNextFileW(search, &found));
        FindClose(search);
    }
    if (!count) {
        log_text("no mods in data\\bin\\mods\\\r\n");
        return;
    }
    FH2ModHost host = {sizeof(host), FH2_MOD_API_VERSION, image, game_exe, settings_dir, mods_dir, data_dir, game};
    for (unsigned i = 0; i < count; ++i) {
        log_wide(names[i]);
        size_t n = 0; // data folder: the DLL name without ".dll", plus a separator
        while (names[i][n]) {
            data_name[n] = names[i][n];
            ++n;
        }
        n -= 4;
        data_name[n++] = L'\\';
        data_name[n] = 0;
        if (!path_join(mod_path, mods_dir, names[i]) || !path_join(data_dir, mods_dir, data_name)) {
            log_text(": path too long, skipped\r\n");
            continue;
        }
        unsigned games = mod_declared_games(mod_path);
        if (!((games ? games : FH2_GAME_FORZA_HORIZON_2) & game)) {
            log_text(": made for another game, not loaded\r\n");
            continue;
        }
        HMODULE module = LoadLibraryExW(mod_path, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
        if (!module) {
            log_text(": could not be loaded\r\n");
            continue;
        }
        FH2ModLoadFn load = (FH2ModLoadFn)(void *)GetProcAddress(module, "FH2ModLoad");
        if (!load) {
            log_text(": not an FH2 mod (no FH2ModLoad export), unloaded\r\n");
            FreeLibrary(module);
            continue;
        }
        log_text(load(&host) ? ": loaded\r\n" : ": loaded, but reported a failure (see its own log)\r\n");
    }
}

static void loader_attach(HINSTANCE instance) {
    azimuth_tls_attached = GetCurrentThreadId();
    if (!azimuth_resolve_game_paths(instance, settings_dir, game_exe)) {
        return; // not running under azimuth.exe (or no folder): behave like the stock helper
    }
    if (path_join(log_path, settings_dir, L"FH2_ModLoader.log")) {
        log_file = CreateFileW(log_path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS,
                               FILE_ATTRIBUTE_NORMAL, NULL);
    }
    log_text("FH2 mod loader (API 1)\r\n");
    unsigned game = 0;
    unsigned char *image = azimuth_find_game_image(&game);
    if (!image) {
        log_text(status_get());
        log_text("\r\nno mods loaded\r\n");
    } else {
        log_text(game == FH2_GAME_FAST_AND_FURIOUS ? "game: Forza Horizon 2 Presents Fast & Furious\r\n"
                                                    : "game: Forza Horizon 2\r\n");
        load_mods(image, game);
    }
    if (log_file != INVALID_HANDLE_VALUE) {
        CloseHandle(log_file);
        log_file = INVALID_HANDLE_VALUE;
    }
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) {
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        loader_attach(instance);
    }
    return TRUE;
}
