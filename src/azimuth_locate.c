#include "azimuth_locate.h"
#include "azimuth_mod.h"
#include "support.h"

/* The games Azimuth runs, by the title id data\azimuth.ini keeps each one's folder under. */
static const struct {
    const wchar_t *title;
    unsigned game;
    const char *name;
} known_games[] = {
    {L"1a96545b", AZIMUTH_GAME_FORZA_HORIZON_2, "Forza Horizon 2"},
    {L"5a9771a4", AZIMUTH_GAME_FAST_AND_FURIOUS, "Forza Horizon 2 Presents Fast & Furious"},
    {L"4ce1dfab", AZIMUTH_GAME_FORZA_HORIZON_3, "Forza Horizon 3"},
    {L"7b35d917", AZIMUTH_GAME_FORZA_MOTORSPORT_5, "Forza Motorsport 5"},
    {L"64f9397e", AZIMUTH_GAME_FORZA_MOTORSPORT_6, "Forza Motorsport 6"},
};
#define KNOWN_COUNT (sizeof(known_games) / sizeof(known_games[0]))

const char *azimuth_game_name(unsigned game) {
    for (unsigned i = 0; i < KNOWN_COUNT; ++i) {
        if (known_games[i].game == game) {
            return known_games[i].name;
        }
    }
    return "unknown game";
}

static wchar_t launcher_dir[PATH_CAPACITY], ini_path[PATH_CAPACITY], file_path[PATH_CAPACITY];
static wchar_t folder[PATH_CAPACITY], mount[PATH_CAPACITY], title_section[16];
static wchar_t candidate_exe[KNOWN_COUNT][PATH_CAPACITY];
static IMAGE_NT_HEADERS64 candidate_headers[KNOWN_COUNT];
static unsigned char file_head[4096];
static char manifest[256 * 1024];

int azimuth_mods_dir(wchar_t *mods_dir) {
    DWORD n = GetModuleFileNameW(0, launcher_dir, PATH_CAPACITY);
    if (!n || n >= PATH_CAPACITY) {
        return 0;
    }
    wchar_t *name = launcher_dir;
    for (wchar_t *p = launcher_dir; *p; ++p) {
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
    *name = 0; // the launcher folder, with its trailing separator
    return path_join(mods_dir, launcher_dir, L"mods\\") &&
           path_join(ini_path, launcher_dir, L"data\\azimuth.ini");
}

static int azimuth_region_is_readable(const MEMORY_BASIC_INFORMATION *m) {
    DWORD p = m->Protect & 0xff;
    return m->State == MEM_COMMIT && !(m->Protect & PAGE_GUARD) &&
           (p == PAGE_READONLY || p == PAGE_READWRITE || p == PAGE_WRITECOPY ||
            p == PAGE_EXECUTE_READ || p == PAGE_EXECUTE_READWRITE || p == PAGE_EXECUTE_WRITECOPY);
}

/* The 64-bit executable's headers at base (in memory, or a file's first bytes), or 0. */
static int azimuth_exe_headers(const unsigned char *base, IMAGE_NT_HEADERS64 *nt) {
    IMAGE_DOS_HEADER dos;
    return memory_read(base, &dos, sizeof(dos)) && dos.e_magic == IMAGE_DOS_SIGNATURE && dos.e_lfanew >= 64 &&
           dos.e_lfanew <= 4096 - (LONG)sizeof(*nt) && memory_read(base + dos.e_lfanew, nt, sizeof(*nt)) &&
           nt->Signature == IMAGE_NT_SIGNATURE && nt->FileHeader.Machine == IMAGE_FILE_MACHINE_AMD64 &&
           !(nt->FileHeader.Characteristics & IMAGE_FILE_DLL) && nt->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC;
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

/* Reads up to size bytes of path into buffer; returns the count, 0 on failure. */
static DWORD azimuth_read_file(const wchar_t *path, void *buffer, DWORD size) {
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD count = 0;
    if (file != INVALID_HANDLE_VALUE) {
        if (!ReadFile(file, buffer, size, &count, NULL)) {
            count = 0;
        }
        CloseHandle(file);
    }
    return count;
}

static void azimuth_copy_path(wchar_t *dst, const wchar_t *src) {
    size_t n = 0;
    do {
        dst[n] = src[n];
    } while (src[n++]);
}

/* mount + the Executable of AppxManifest.xml's first Application, into exe. */
static int azimuth_manifest_exe(const wchar_t *mount_dir, wchar_t *exe) {
    if (!path_join(file_path, mount_dir, L"AppxManifest.xml")) {
        return 0;
    }
    DWORD size = azimuth_read_file(file_path, manifest, sizeof(manifest) - 1);
    const char *application = size ? text_find(manifest, size, "<Application ") : 0;
    const char *value = application ? text_find(application, size - (size_t)(application - manifest), "Executable=\"") : 0;
    if (!value) {
        return 0;
    }
    value += 12;
    wchar_t name[MAX_PATH];
    unsigned n = 0;
    for (; value < manifest + size && *value != '"'; ++value) {
        if (n >= MAX_PATH - 1 || *value < 0x20 || *value > 0x7e || *value == '\\' || *value == '/' || *value == ':') {
            return 0; // a plain file name in Mount, nothing else
        }
        name[n++] = (wchar_t)*value;
    }
    name[n] = 0;
    return n && value < manifest + size && path_join(exe, mount_dir, name);
}

/* The executable of title i as data\azimuth.ini places it, with its headers; 0 if it has none. */
static int azimuth_title_exe(unsigned i) {
    const wchar_t prefix[] = L"title-";
    memory_copy(title_section, prefix, sizeof(prefix));
    for (unsigned c = 0; c < 9; ++c) {
        title_section[6 + c] = known_games[i].title[c];
    }
    DWORD n = GetPrivateProfileStringW(title_section, L"folder", L"", folder, PATH_CAPACITY, ini_path);
    if (!n || n >= PATH_CAPACITY - 1) {
        return 0;
    }
    // a relative folder is based on the launcher folder
    if (!((folder[0] && folder[1] == L':') || folder[0] == L'\\' || folder[0] == L'/')) {
        if (!path_join(file_path, launcher_dir, folder)) {
            return 0;
        }
    } else {
        azimuth_copy_path(file_path, folder);
    }
    n = GetFullPathNameW(file_path, PATH_CAPACITY, folder, 0);
    return n && n < PATH_CAPACITY && path_join(mount, folder, L"Mount\\") &&
           azimuth_manifest_exe(mount, candidate_exe[i]) &&
           azimuth_read_file(candidate_exe[i], file_head, sizeof(file_head)) == sizeof(file_head) &&
           azimuth_exe_headers(file_head, &candidate_headers[i]);
}

unsigned char *azimuth_find_game(wchar_t *game_exe, unsigned *game) {
    int titles[KNOWN_COUNT], any = 0;
    for (unsigned i = 0; i < KNOWN_COUNT; ++i) {
        any |= titles[i] = azimuth_title_exe(i);
    }
    if (!any) {
        status_set("REFUSED: data/azimuth.ini names no game folder with a readable executable. No patches applied.");
        return 0;
    }
    unsigned char *self = (unsigned char *)GetModuleHandleW(NULL), *found = 0;
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    uintptr_t p = (uintptr_t)info.lpMinimumApplicationAddress, max = (uintptr_t)info.lpMaximumApplicationAddress;
    while (p <= max) {
        MEMORY_BASIC_INFORMATION m;
        IMAGE_NT_HEADERS64 nt;
        if (VirtualQuery((void *)p, &m, sizeof(m)) != sizeof(m)) {
            break;
        }
        // a game as Azimuth mapped it: an executable's headers at an allocation of its own
        if (m.BaseAddress == m.AllocationBase && (unsigned char *)m.AllocationBase != self &&
            (m.Type == MEM_PRIVATE || m.Type == MEM_IMAGE) && azimuth_region_is_readable(&m) &&
            m.RegionSize >= sizeof(IMAGE_DOS_HEADER) && azimuth_exe_headers(m.AllocationBase, &nt)) {
            for (unsigned i = 0; i < KNOWN_COUNT; ++i) {
                if (!titles[i] || nt.FileHeader.TimeDateStamp != candidate_headers[i].FileHeader.TimeDateStamp ||
                    nt.OptionalHeader.SizeOfImage != candidate_headers[i].OptionalHeader.SizeOfImage) {
                    continue;
                }
                if (!azimuth_image_is_complete(m.AllocationBase, nt.OptionalHeader.SizeOfImage)) {
                    status_set("REFUSED: the mapped game image is incomplete or inaccessible. No patches applied.");
                    return 0;
                }
                if (found) {
                    status_set("REFUSED: more than one game is mapped. No patches applied.");
                    return 0;
                }
                found = m.AllocationBase;
                *game = known_games[i].game;
                azimuth_copy_path(game_exe, candidate_exe[i]);
            }
        }
        uintptr_t next = (uintptr_t)m.BaseAddress + m.RegionSize;
        if (next <= p) {
            break;
        }
        p = next;
    }
    if (!found) {
        status_set("no game mapped yet");
    }
    return found;
}
