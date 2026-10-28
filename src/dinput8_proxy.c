#include "game.h"

/* azimuth.exe imports DirectInput8Create from dinput8.dll, so this DLL next to it is loaded at
   startup. Every dinput8 export goes on to the system dinput8.dll, loaded on first use (never
   inside DllMain). */
static HMODULE system_dinput8;

static FARPROC system_export(const char *name) {
    if (!system_dinput8) {
        static const wchar_t file[] = L"\\dinput8.dll";
        wchar_t path[MAX_PATH];
        UINT n = GetSystemDirectoryW(path, MAX_PATH);
        if (!n || n + sizeof(file) / sizeof(file[0]) > MAX_PATH) {
            return 0;
        }
        for (unsigned i = 0; i < sizeof(file) / sizeof(file[0]); ++i) {
            path[n + i] = file[i];
        }
        system_dinput8 = LoadLibraryExW(path, NULL, 0);
    }
    return system_dinput8 ? GetProcAddress(system_dinput8, name) : 0;
}

typedef HRESULT(WINAPI *Create)(HINSTANCE, DWORD, const void *, void **, void *);
typedef HRESULT(WINAPI *Plain)(void);
typedef HRESULT(WINAPI *ClassObject)(const void *, const void *, void **);
typedef const void *(WINAPI *Format)(void);

__declspec(dllexport) HRESULT WINAPI DirectInput8Create(HINSTANCE instance, DWORD version, const void *iid,
                                                        void **out, void *outer) {
    Create f = (Create)(void *)system_export("DirectInput8Create");
    return f ? f(instance, version, iid, out, outer) : E_FAIL;
}

__declspec(dllexport) HRESULT WINAPI DllCanUnloadNow(void) {
    return S_FALSE; // the loader and its mods stay
}

__declspec(dllexport) HRESULT WINAPI DllGetClassObject(const void *clsid, const void *iid, void **out) {
    ClassObject f = (ClassObject)(void *)system_export("DllGetClassObject");
    return f ? f(clsid, iid, out) : E_FAIL;
}

__declspec(dllexport) HRESULT WINAPI DllRegisterServer(void) {
    Plain f = (Plain)(void *)system_export("DllRegisterServer");
    return f ? f() : E_FAIL;
}

__declspec(dllexport) HRESULT WINAPI DllUnregisterServer(void) {
    Plain f = (Plain)(void *)system_export("DllUnregisterServer");
    return f ? f() : E_FAIL;
}

__declspec(dllexport) const void *WINAPI GetdfDIJoystick(void) {
    Format f = (Format)(void *)system_export("GetdfDIJoystick");
    return f ? f() : 0;
}
