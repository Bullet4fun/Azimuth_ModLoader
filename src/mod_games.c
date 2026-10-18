#include "mod_games.h"

static DWORD read32(const unsigned char *p) {
    DWORD value;
    memory_copy(&value, p, sizeof(value)); // PE tables need not be aligned (lld put an export directory at 0x...151)
    return value;
}

static WORD read16(const unsigned char *p) {
    WORD value;
    memory_copy(&value, p, sizeof(value));
    return value;
}

/* The mod's FH2ModGames export, read from the DLL mapped as an image resource: the system maps its
   sections but runs nothing (no DllMain, no imports, no TLS callbacks). Every offset is checked
   against the image size first. */
unsigned mod_declared_games(const wchar_t *path) {
    HMODULE mapped = LoadLibraryExW(path, NULL, LOAD_LIBRARY_AS_IMAGE_RESOURCE);
    if (!mapped) {
        return 0;
    }
    const unsigned char *base = (const unsigned char *)((uintptr_t)mapped & ~(uintptr_t)3); // low bits flag the mapping kind
    unsigned games = 0;
    DWORD nt = read32(base + offsetof(IMAGE_DOS_HEADER, e_lfanew));
    const unsigned char *optional = base + nt + 4 + sizeof(IMAGE_FILE_HEADER);
    if (read16(base) == IMAGE_DOS_SIGNATURE && nt > 0 && nt < 4096 && read32(base + nt) == IMAGE_NT_SIGNATURE &&
        read16(optional) == IMAGE_NT_OPTIONAL_HDR64_MAGIC &&
        read32(optional + offsetof(IMAGE_OPTIONAL_HEADER64, NumberOfRvaAndSizes)) > IMAGE_DIRECTORY_ENTRY_EXPORT) {
        DWORD image_size = read32(optional + offsetof(IMAGE_OPTIONAL_HEADER64, SizeOfImage));
        const unsigned char *entry = optional + offsetof(IMAGE_OPTIONAL_HEADER64, DataDirectory) +
                                     IMAGE_DIRECTORY_ENTRY_EXPORT * sizeof(IMAGE_DATA_DIRECTORY);
        DWORD dir = read32(entry), dir_size = read32(entry + 4);
        if (dir && dir_size >= sizeof(IMAGE_EXPORT_DIRECTORY) && dir < image_size && dir_size <= image_size - dir) {
            const unsigned char *exports = base + dir;
            DWORD functions = read32(exports + offsetof(IMAGE_EXPORT_DIRECTORY, AddressOfFunctions));
            DWORD function_count = read32(exports + offsetof(IMAGE_EXPORT_DIRECTORY, NumberOfFunctions));
            DWORD names = read32(exports + offsetof(IMAGE_EXPORT_DIRECTORY, AddressOfNames));
            DWORD ordinals = read32(exports + offsetof(IMAGE_EXPORT_DIRECTORY, AddressOfNameOrdinals));
            DWORD count = read32(exports + offsetof(IMAGE_EXPORT_DIRECTORY, NumberOfNames));
            if (names < image_size && count <= (image_size - names) / 4 && ordinals < image_size &&
                count <= (image_size - ordinals) / 2 && functions < image_size &&
                function_count <= (image_size - functions) / 4) {
                static const char wanted[] = "FH2ModGames";
                for (DWORD i = 0; i < count; ++i) {
                    DWORD name = read32(base + names + 4 * i);
                    if (name >= image_size || image_size - name < sizeof(wanted) ||
                        !memory_equal(base + name, wanted, sizeof(wanted))) {
                        continue;
                    }
                    WORD ordinal = read16(base + ordinals + 2 * i);
                    DWORD rva = ordinal < function_count ? read32(base + functions + 4 * ordinal) : 0;
                    int forwarder = rva >= dir && rva - dir < dir_size;
                    if (rva && !forwarder && rva <= image_size - sizeof(unsigned)) {
                        memory_copy(&games, base + rva, sizeof(games));
                    }
                    break;
                }
            }
        }
    }
    FreeLibrary(mapped);
    return games;
}
