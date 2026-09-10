#include "platform/log.h"

#include <cstdarg>
#include <cstdio>

namespace {
bool g_verbose_log_enabled = false;
}

void set_verbose_log_enabled(bool enabled) {
    g_verbose_log_enabled = enabled;
}

void log_warning(const char *format, ...) {
    if (!g_verbose_log_enabled) {
        return;
    }

    va_list args;
    va_start(args, format);
    std::vfprintf(stderr, format, args);
    va_end(args);
}
