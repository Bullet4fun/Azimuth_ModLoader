#include "azimuth_locate.h"
#include "fh2_mod.h"
#include "support.h"

/* The game builds Azimuth runs, told apart by their PE headers. */
static const struct {
    DWORD timestamp, image_size;
    unsigned game;
} known_games[] = {
    {0x565501ed, 0x3a18000, FH2_GAME_FORZA_HORIZON_2},
    {0x55b62fde, 0x3a89000, FH2_GAME_FAST_AND_FURIOUS},
};

static int azimuth_region_is_readable(const MEMORY_BASIC_INFORMATION *m) {
    DWORD p = m->Protect & 0xff;
    return m->State == MEM_COMMIT && !(m->Protect & PAGE_GUARD) &&
           (p == PAGE_READONLY || p == PAGE_READWRITE || p == PAGE_WRITECOPY ||
            p == PAGE_EXECUTE_READ || p == PAGE_EXECUTE_READWRITE || p == PAGE_EXECUTE_WRITECOPY);
}

/* Index into known_games, or -1. */
static int azimuth_game_headers(const unsigned char *base) {
    IMAGE_DOS_HEADER dos;
    IMAGE_NT_HEADERS64 nt;
    if (!memory_read(base, &dos, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE ||
        dos.e_lfanew < 64 || dos.e_lfanew > 4096 ||
        !memory_read(base + dos.e_lfanew, &nt, sizeof(nt))) {
        return -1;
    }
    if (nt.Signature != IMAGE_NT_SIGNATURE || nt.FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 ||
        (nt.FileHeader.Characteristics & IMAGE_FILE_DLL) || nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        return -1;
    }
    for (int i = 0; i < (int)(sizeof(known_games) / sizeof(known_games[0])); ++i) {
        if (nt.FileHeader.TimeDateStamp == known_games[i].timestamp &&
            nt.OptionalHeader.SizeOfImage == known_games[i].image_size) {
            return i;
        }
    }
    return -1;
}

static int azimuth_image_is_complete(unsigned char *base, DWORD image_size) {
    uintptr_t p = (uintptr_t)base, end = p + image_size;
    if (end < p) {
        return 0;
    }
    while (p < end) {
        MEMORY_BASIC_INFORMATION m;
        if (VirtualQuery((void *)p, &m, sizeof(m)) != sizeof(m) ||
            !azimuth_region_is_readable(&m) || m.AllocationBase != base) {
            return 0;
        }
        uintptr_t next = (uintptr_t)m.BaseAddress + m.RegionSize;
        if (next <= p) {
            return 0;
        }
        p = next;
    }
    return 1;
}

unsigned char *azimuth_find_game_image(unsigned *game) {
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    uintptr_t p = (uintptr_t)info.lpMinimumApplicationAddress,
              max = (uintptr_t)info.lpMaximumApplicationAddress;
    unsigned char *found = 0;
    while (p <= max) {
        MEMORY_BASIC_INFORMATION m;
        if (VirtualQuery((void *)p, &m, sizeof(m)) != sizeof(m)) {
            break;
        }
        int known = m.BaseAddress == m.AllocationBase && (m.Type == MEM_PRIVATE || m.Type == MEM_IMAGE) &&
                            azimuth_region_is_readable(&m) && m.RegionSize >= sizeof(IMAGE_DOS_HEADER)
                        ? azimuth_game_headers(m.AllocationBase)
                        : -1;
        if (known >= 0) {
            if (!azimuth_image_is_complete(m.AllocationBase, known_games[known].image_size)) {
                status_set("REFUSED: mapped FH2 image is incomplete or inaccessible. "
                           "No patches applied.");
                return 0;
            }
            if (found) {
                status_set("REFUSED: more than one supported FH2 image is mapped. No "
                           "patches applied.");
                return 0;
            }
            found = m.AllocationBase;
            *game = known_games[known].game;
        }
        uintptr_t next = (uintptr_t)m.BaseAddress + m.RegionSize;
        if (next <= p) {
            break;
        }
        p = next;
    }
    if (!found) {
        status_set("REFUSED: Azimuth has not mapped a supported game executable. "
                   "No patches applied.");
    }
    return found;
}

static wchar_t azimuth_folder[PATH_CAPACITY], file_path[PATH_CAPACITY], title_section[64];

static void azimuth_copy_path(wchar_t *dst, const wchar_t *src) {
    size_t n = 0;
    do {
        dst[n] = src[n];
    } while (src[n++]);
}

int azimuth_resolve_game_paths(HINSTANCE instance, wchar_t *module_dir, wchar_t *game_exe) {
    DWORD n = GetModuleFileNameW(0, game_exe, PATH_CAPACITY);
    if (!n || n >= PATH_CAPACITY) {
        return 0;
    }
    wchar_t *name = game_exe;
    for (wchar_t *p = game_exe; *p; ++p) {
        if (*p == L'\\' || *p == L'/') {
            name = p + 1;
        }
    }
    const wchar_t expected[] = L"azimuth.exe";
    for (unsigned i = 0;; ++i) {
        wchar_t c = name[i];
        if (c >= L'A' && c <= L'Z') {
            c += L'a' - L'A';
        }
        if (c != expected[i]) {
            return 0;
        }
        if (!c) {
            break;
        }
    }
    n = GetModuleFileNameW(instance, module_dir, PATH_CAPACITY);
    if (!n || n >= PATH_CAPACITY) {
        return 0;
    }
    while (n && module_dir[n - 1] != L'\\' && module_dir[n - 1] != L'/') {
        --n;
    }
    module_dir[n] = 0;
    n = (DWORD)(name - game_exe);
    game_exe[n] = 0; /* Launcher directory. */
    if (!path_join(file_path, game_exe, L"data\\azimuth.ini")) {
        goto path_error;
    }
    /* The title being launched: [launcher] game=<id> (1a96545b Forza Horizon 2, 5a9771a4 Fast &
       Furious), its folder in [title-<id>]. */
    const wchar_t prefix[] = L"title-";
    memory_copy(title_section, prefix, sizeof(prefix));
    n = GetPrivateProfileStringW(L"launcher", L"game", L"1a96545b", title_section + 6,
                                 (DWORD)(sizeof(title_section) / sizeof(title_section[0]) - 6), file_path);
    n = GetPrivateProfileStringW(title_section, L"folder", L"", azimuth_folder, PATH_CAPACITY,
                                 file_path);
    if (!n) {
        n = GetPrivateProfileStringW(L"launcher", L"game_folder", L"", azimuth_folder,
                                     PATH_CAPACITY, file_path);
    }
    if (!n || n >= PATH_CAPACITY - 1) {
        goto path_error;
    }
    /* Relative game folders are based on the launcher directory. */
    if (!((azimuth_folder[0] && azimuth_folder[1] == L':') || azimuth_folder[0] == L'\\' ||
          azimuth_folder[0] == L'/')) {
        if (!path_join(file_path, game_exe, azimuth_folder)) {
            goto path_error;
        }
    } else {
        azimuth_copy_path(file_path, azimuth_folder);
    }
    n = GetFullPathNameW(file_path, PATH_CAPACITY, azimuth_folder, 0);
    if (!n || n >= PATH_CAPACITY) {
        goto path_error;
    }
    if (!path_join(game_exe, azimuth_folder, L"Mount\\forza_durango_release_final.exe")) {
        goto path_error;
    }
    return 1;
path_error:
    status_set("REFUSED: cannot resolve the game folder from "
               "data/azimuth.ini. No patches applied.");
    return 0;
}
