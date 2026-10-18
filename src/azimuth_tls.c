#include "game.h"

/* Azimuth 0.9.5's helper ABI, reimplemented without its binary or source:
   one 336-byte zero TLS template, alignment 16, no callbacks/zero fill;
   two data exports, the native TLS index and process-attach thread ID.
   The loader maps/relocates FH2 before loading this helper. No host addresses,
   remote injection, worker thread or LoadLibrary is needed.
   Unchanged through Azimuth 0.9.10 (its helper differs from 0.9.9's only in the timestamp). */
__declspec(dllexport) DWORD azimuth_tls_index = TLS_OUT_OF_INDEXES;
__declspec(dllexport) DWORD azimuth_tls_attached;
// in .rdata like Azimuth's own helper: a separate .tls section in a small DLL is what antivirus
// heuristics flag (seen: Avast/AVG/Avira "Evo-gen" on a build with only this file); the loader
// reads the template through the TLS directory, wherever it is
__attribute__((aligned(16))) static const unsigned char azimuth_tls_template[0x150] = {0};
static const PIMAGE_TLS_CALLBACK azimuth_tls_callbacks[1] = {0};
const IMAGE_TLS_DIRECTORY64 _tls_used = {
    (ULONGLONG)(uintptr_t)azimuth_tls_template,
    (ULONGLONG)(uintptr_t)(azimuth_tls_template + sizeof(azimuth_tls_template)),
    (ULONGLONG)(uintptr_t)&azimuth_tls_index,
    (ULONGLONG)(uintptr_t)azimuth_tls_callbacks,
    0,
    0x00500000};
