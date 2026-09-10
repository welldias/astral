#include "app/app_state.h"

#include <emscripten/emscripten.h>

extern "C" EMSCRIPTEN_KEEPALIVE void astral_open_document(const char *path) {
    if (g_app_state == nullptr || path == nullptr) {
        return;
    }
    try_open_document(*g_app_state, path);
}
