// ============================================================================
// Logger.h — Simple thread-safe logging utility
// ============================================================================
#pragma once
#include <windows.h>
#include <stdio.h>

inline void Log(const char* fmt, ...)
{
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    FILE* f = nullptr;
    // Log to the root of C: to ensure accessibility from Session 0 (LOCAL SERVICE)
    if (fopen_s(&f, "C:\\BestCam.log", "a") == 0)
    {
        fprintf(f, "[%lu] %s\n", GetCurrentThreadId(), buf);
        fclose(f);
    }
}
