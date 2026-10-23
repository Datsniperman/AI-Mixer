#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <cstring>
#include <windows.h>
#include <shlobj.h>

namespace aimixer {

inline void traceLog(const char* fmt, ...) {
    char appData[MAX_PATH] = { 0 };
    if (!SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appData))) {
        const char* env = getenv("APPDATA");
        if (env) {
            strncpy(appData, env, sizeof(appData) - 1);
        }
    }

    if (appData[0] == '\0') return;

    char dir[MAX_PATH];
    snprintf(dir, sizeof(dir), "%s\\AI Mixer", appData);
    CreateDirectoryA(dir, NULL);

    char filePath[MAX_PATH];
    snprintf(filePath, sizeof(filePath), "%s\\load-trace.log", dir);

    FILE* f = fopen(filePath, "a");
    if (!f) return;

    SYSTEMTIME st;
    GetLocalTime(&st);
    fprintf(f, "[%02d:%02d:%02d.%03d | PID %lu | TID %lu] ",
            st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
            static_cast<unsigned long>(GetCurrentProcessId()),
            static_cast<unsigned long>(GetCurrentThreadId()));

    va_list args;
    va_start(args, fmt);
    vfprintf(f, fmt, args);
    va_end(args);

    fprintf(f, "\n");
    fflush(f);
    fclose(f);
}

template <typename T>
inline auto traceLog(const T& juceStr) -> decltype(juceStr.toRawUTF8(), void()) {
    traceLog("%s", juceStr.toRawUTF8());
}

} // namespace aimixer
