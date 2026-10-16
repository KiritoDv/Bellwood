#include <libultraship.h>
#include <fast/interpreter.h>
#include "port/Engine.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <string>

static bool sReportEnabled = true;

static std::string FormatV(const char* fmt, va_list args) {
    char buf[1024];
    va_list copy;
    va_copy(copy, args);
    int n = vsnprintf(buf, sizeof(buf), fmt, copy);
    va_end(copy);
    if (n < 0) {
        return std::string();
    }
    if ((size_t)n < sizeof(buf)) {
        std::string s(buf, n);
        while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) {
            s.pop_back();
        }
        return s;
    }
    std::string s(n + 1, '\0');
    vsnprintf(s.data(), s.size(), fmt, args);
    s.resize(n);
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) {
        s.pop_back();
    }
    return s;
}

extern "C" {

void OSVReport(const char* fmt, va_list list) {
    if (!sReportEnabled || fmt == nullptr) {
        return;
    }
    std::string s = FormatV(fmt, list);
    if (!s.empty()) {
        SPDLOG_DEBUG("[game] {}", s);
    }
}

void OSReport(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    OSVReport(fmt, args);
    va_end(args);
}

void OSPanic(const char* file, int line, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    std::string s = FormatV(fmt, args);
    va_end(args);
    SPDLOG_CRITICAL("[game] panic at {}:{}: {}", file != nullptr ? file : "?", line, s);
    spdlog::default_logger()->flush();
    abort();
}

void OSReportEnable(void) {
    sReportEnabled = true;
}

void OSReportDisable(void) {
    sReportEnabled = false;
}

void AC_DebugPrint(int x, int y, int show_count, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    std::string s = FormatV(fmt, args);
    va_end(args);
    SPDLOG_TRACE("[game print {},{}] {}", x, y, s);
}

void AC_TexInvalidateRange(const void* addr, size_t size) {
    if (addr == nullptr || size == 0) {
        return;
    }
    Fast::Interpreter* interpreter = GameEngine_GetInterpreter();
    if (interpreter == nullptr) {
        return;
    }
    interpreter->TextureCacheDeleteRange((const uint8_t*)addr, size);
}
}
