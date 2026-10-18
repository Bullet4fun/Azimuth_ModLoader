/* The smallest FH2 mod: data\bin\mods\FH2_HelloMod.dll. On load it creates its own folder
   (data\bin\mods\FH2_HelloMod\), writes hello.log there, and checks the bytes of one game function
   the way a mod must before patching it. It patches nothing. Kernel32 only, no C runtime. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "fh2_mod.h"

// It runs in both games (the bytes it checks below are only checked in Forza Horizon 2).
FH2_MOD_GAMES(FH2_GAME_FORZA_HORIZON_2 | FH2_GAME_FAST_AND_FURIOUS);

static HANDLE log_file = INVALID_HANDLE_VALUE;

static void log_text(const char *text) {
    DWORD length = 0, written;
    while (text[length]) {
        ++length;
    }
    WriteFile(log_file, text, length, &written, NULL);
}

/* data_dir + name, or 0 when it does not fit */
static int path_join(wchar_t *out, size_t capacity, const wchar_t *dir, const wchar_t *name) {
    size_t n = 0;
    for (; *dir; ++dir) {
        if (n + 1 >= capacity) return 0;
        out[n++] = *dir;
    }
    for (; *name; ++name) {
        if (n + 1 >= capacity) return 0;
        out[n++] = *name;
    }
    out[n] = 0;
    return 1;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) {
    (void)instance;
    (void)reason;
    (void)reserved;
    return TRUE;
}

__declspec(dllexport) int FH2ModLoad(const FH2ModHost *host) {
    // Refuse a loader this mod was not built for: the struct may only grow, the version must match.
    if (!host || host->size < sizeof(*host) || host->api_version != FH2_MOD_API_VERSION || !host->game_image) {
        return 0;
    }
    static wchar_t path[MAX_PATH];
    CreateDirectoryW(host->data_dir, NULL); // only a mod that keeps files creates its folder
    if (!path_join(path, MAX_PATH, host->data_dir, L"hello.log")) {
        return 0;
    }
    log_file = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (log_file == INVALID_HANDLE_VALUE) {
        return 0;
    }
    log_text(host->game == FH2_GAME_FAST_AND_FURIOUS ? "Hello from FH2_HelloMod in Fast & Furious.\r\n"
                                                     : "Hello from FH2_HelloMod in Forza Horizon 2.\r\n");
    if (host->game != FH2_GAME_FORZA_HORIZON_2) {
        CloseHandle(log_file); // the address below is Forza Horizon 2's: every RVA differs in the other game
        return 1;
    }

    // Before patching game code, compare its bytes with what you reverse engineered; a different
    // game build or another mod's hook means: leave it alone. FMOD's System::playSound, at RVA
    // 0x199b0f0, starts with push rbx/rbp/rsi/rdi/r14/r15 / sub rsp,168h.
    static const unsigned char expected[16] = {0x40, 0x53, 0x55, 0x56, 0x57, 0x41, 0x56, 0x41,
                                               0x57, 0x48, 0x81, 0xec, 0x68, 0x01, 0x00, 0x00};
    const unsigned char *code = host->game_image + 0x199b0f0;
    int same = 1;
    for (int i = 0; i < 16; ++i) {
        same &= code[i] == expected[i];
    }
    log_text(same ? "System::playSound looks as expected: safe to hook.\r\n"
                  : "System::playSound differs (another mod hooked it?): a real mod would skip its hook.\r\n");
    CloseHandle(log_file);
    return 1;
}
