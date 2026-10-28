/* Azimuth Mod Loader: dinput8.dll next to azimuth.exe (dinput8_proxy.c). When Azimuth has mapped the
   game, finds its image (azimuth_locate.c), then loads mods\*.dll (beside azimuth.exe) in name
   order and calls each AzimuthModLoad. Writes mods\AzimuthModLoader.log. Kernel32 only. */
#include "azimuth_locate.h"
#include "azimuth_mod.h"
#include "mod_games.h"
#include "support.h"

#define MAX_MODS 64
static const char MOD_LOADED[] = ": loaded\r\n";

static wchar_t game_exe[PATH_CAPACITY], mods_dir[PATH_CAPACITY];
static wchar_t pattern[PATH_CAPACITY], mod_path[PATH_CAPACITY], log_path[PATH_CAPACITY];
static wchar_t data_dir[PATH_CAPACITY], data_name[MAX_PATH + 1];
static unsigned game; // AZIMUTH_GAME_* of the game Azimuth mapped
static wchar_t names[MAX_MODS][MAX_PATH];
static HANDLE log_file = INVALID_HANDLE_VALUE;

static void log_text(const char *text) {
    DWORD written;
    if (log_file != INVALID_HANDLE_VALUE) {
        WriteFile(log_file, text, (DWORD)text_length(text), &written, NULL);
    }
}

/* Starts a line "[HH:MM:SS] [LEVEL] [ModLoader]: ", the same layout as the mods' logs. */
static void log_line(const char *level) {
    SYSTEMTIME t;
    GetLocalTime(&t);
    char clock[12] = {'[', (char)('0' + t.wHour / 10), (char)('0' + t.wHour % 10), ':', (char)('0' + t.wMinute / 10),
                      (char)('0' + t.wMinute % 10), ':', (char)('0' + t.wSecond / 10), (char)('0' + t.wSecond % 10), ']', ' ', 0};
    log_text(clock);
    log_text("[");
    log_text(level);
    log_text("] [ModLoader]: ");
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

/* Loads names[i]; returns the rest of its log line. */
static const char *load_mod(AzimuthModHost *host, unsigned i, unsigned game) {
    size_t n = 0; // data folder: the DLL name without ".dll", plus a separator
    while (names[i][n]) {
        data_name[n] = names[i][n];
        ++n;
    }
    n -= 4;
    data_name[n++] = L'\\';
    data_name[n] = 0;
    if (!path_join(mod_path, mods_dir, names[i]) || !path_join(data_dir, mods_dir, data_name)) {
        return ": path too long, skipped\r\n";
    }
    unsigned games = mod_declared_games(mod_path);
    if (!((games ? games : AZIMUTH_GAME_FORZA_HORIZON_2) & game)) {
        return ": made for another game, not loaded\r\n";
    }
    HMODULE module = LoadLibraryExW(mod_path, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!module) {
        return ": could not be loaded\r\n";
    }
    AzimuthModLoadFn load = (AzimuthModLoadFn)(void *)GetProcAddress(module, "AzimuthModLoad");
    if (!load) {
        FreeLibrary(module);
        return ": not an Azimuth mod (no AzimuthModLoad export), unloaded\r\n";
    }
    return load(host) ? MOD_LOADED : ": loaded, but reported a failure (see its own log)\r\n";
}

static void load_mods(unsigned char *image, unsigned game) {
    if (!path_join(pattern, mods_dir, L"*.dll")) {
        log_line("ERROR");
        log_text("paths too long; no mods loaded\r\n");
        return;
    }
    unsigned count = 0;
    WIN32_FIND_DATAW found;
    HANDLE search = FindFirstFileW(pattern, &found);
    if (search != INVALID_HANDLE_VALUE) {
        do {
            if (!(found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && mod_file_name(found.cFileName) && count < MAX_MODS) {
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
        log_line("INFO");
        log_text("no mods in the mods folder\r\n");
        return;
    }
    AzimuthModHost host = {sizeof(host), AZIMUTH_MOD_API_VERSION, image, game_exe, mods_dir, data_dir, game};
    for (unsigned i = 0; i < count; ++i) {
        const char *result = load_mod(&host, i, game);
        log_line(result == MOD_LOADED ? "INFO" : "WARN");
        log_wide(names[i]);
        log_text(result);
    }
}

/* The import slot azimuth.exe calls `name` from `dll` through, or NULL. */
static void **import_slot(unsigned char *base, const char *dll, const char *name) {
    IMAGE_NT_HEADERS64 *nt = (IMAGE_NT_HEADERS64 *)(base + ((IMAGE_DOS_HEADER *)base)->e_lfanew);
    IMAGE_DATA_DIRECTORY *directory = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!directory->VirtualAddress) {
        return 0;
    }
    for (IMAGE_IMPORT_DESCRIPTOR *d = (IMAGE_IMPORT_DESCRIPTOR *)(base + directory->VirtualAddress); d->Name; ++d) {
        if (!d->OriginalFirstThunk || !text_equal_ignore_case((const char *)base + d->Name, dll)) {
            continue;
        }
        IMAGE_THUNK_DATA64 *names = (IMAGE_THUNK_DATA64 *)(base + d->OriginalFirstThunk);
        for (unsigned i = 0; names[i].u1.AddressOfData; ++i) {
            if (!IMAGE_SNAP_BY_ORDINAL64(names[i].u1.Ordinal) &&
                text_equal_ignore_case(((IMAGE_IMPORT_BY_NAME *)(base + names[i].u1.AddressOfData))->Name, name)) {
                return (void **)(base + d->FirstThunk) + i;
            }
        }
    }
    return 0;
}

typedef HANDLE(WINAPI *SnapshotFn)(DWORD, DWORD);
static SnapshotFn original_snapshot;
static void **snapshot_slot;
static volatile LONG loading, finished;

static int set_slot(void *value) {
    DWORD protection, ignored;
    if (!VirtualProtect(snapshot_slot, sizeof(void *), PAGE_READWRITE, &protection)) {
        return 0;
    }
    *snapshot_slot = value;
    VirtualProtect(snapshot_slot, sizeof(void *), protection, &ignored);
    return 1;
}

static void close_log(void) {
    if (log_file != INVALID_HANDLE_VALUE) {
        CloseHandle(log_file);
        log_file = INVALID_HANDLE_VALUE;
    }
}

/* Azimuth takes a thread snapshot only to bind a static TLS template, the game's once it has
   mapped the game and before any game code runs. The first call that finds the game image loads
   the mods, ahead of the snapshot, so threads the mods start get the game's TLS as well. */
static HANDLE WINAPI on_snapshot(DWORD flags, DWORD process) {
    if (!finished && !InterlockedCompareExchange(&loading, 1, 0)) {
        unsigned char *image = azimuth_find_game(game_exe, &game);
        if (image) {
            log_line("INFO");
            log_text("game: ");
            log_text(azimuth_game_name(game));
            log_text("\r\n");
            load_mods(image, game);
            close_log();
            finished = 1;
            set_slot((void *)(uintptr_t)original_snapshot);
        } else {
            log_line("INFO");
            log_text(status_get());
            log_text("; waiting for the next TLS binding\r\n");
        }
        loading = 0; // no game yet (a library's TLS, or the launcher's): wait for the next snapshot
    }
    return original_snapshot(flags, process);
}

static void loader_attach(void) {
    if (!azimuth_mods_dir(mods_dir)) {
        return; // not azimuth.exe: only the dinput8 exports work
    }
    CreateDirectoryW(mods_dir, NULL); // the log goes there even before any mod is installed
    if (path_join(log_path, mods_dir, L"AzimuthModLoader.log")) {
        // emptied, then reopened append-only and shared: a mod may add its own line while it loads
        // (each write lands at the end, so neither side overwrites the other); shared for writing
        // as well, since azimuth.exe restarts itself (back to the launcher) while the old one has it
        log_file = CreateFileW(log_path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS,
                               FILE_ATTRIBUTE_NORMAL, NULL);
        if (log_file != INVALID_HANDLE_VALUE) {
            CloseHandle(log_file);
            log_file = CreateFileW(log_path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
                                   FILE_ATTRIBUTE_NORMAL, NULL);
        }
    }
    log_line("INFO");
    log_text("Azimuth Mod Loader " LOADER_VERSION " (API 2) by Bullet4fun: https://github.com/Bullet4fun/Azimuth_ModLoader\r\n");
    snapshot_slot = import_slot((unsigned char *)GetModuleHandleW(NULL), "KERNEL32.dll", "CreateToolhelp32Snapshot");
    if (!snapshot_slot) {
        log_line("ERROR");
        log_text("azimuth.exe takes no thread snapshots (an unsupported Azimuth version); no mods loaded\r\n");
        close_log();
        return;
    }
    original_snapshot = (SnapshotFn)*snapshot_slot;
    if (!set_slot((void *)(uintptr_t)on_snapshot)) {
        log_line("ERROR");
        log_text("cannot hook azimuth.exe's thread snapshot import; no mods loaded\r\n");
        close_log();
    }
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) {
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        loader_attach();
    }
    return TRUE;
}
