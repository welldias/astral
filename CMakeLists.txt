cmake_minimum_required(VERSION 4.0)
project(astral CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

include(cmake/Dependencies.cmake)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp)

# Each OS has its own implementation of src/platform/default_font_*.cpp
# (see src/platform/default_font.h); only the one for the build's target
# OS is included in the compilation.
if(WIN32)
  list(REMOVE_ITEM SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/default_font_linux.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/default_font_macos.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/default_font_wasm.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/wasm_bridge.cpp)
elseif(APPLE)
  list(REMOVE_ITEM SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/default_font_linux.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/default_font_win.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/default_font_wasm.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/wasm_bridge.cpp)
elseif(EMSCRIPTEN)
  # No OS font store to query in the browser (no fontconfig/CoreText/
  # registry) — default_font_wasm.cpp's stubs always fall back to
  # AppConfig::bundled_font_path (see resolve_font_paths), and
  # wasm_bridge.cpp is the JS<->C++ bridge used to load a user-selected
  # .md file into the app (see src/main.cpp).
  list(REMOVE_ITEM SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/default_font_linux.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/default_font_win.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/default_font_macos.cpp)
else()
  list(REMOVE_ITEM SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/default_font_win.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/default_font_macos.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/default_font_wasm.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/wasm_bridge.cpp)
  # default_font_linux.cpp uses the fontconfig API (Fc...) to locate the
  # system's default fonts.
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(Fontconfig REQUIRED IMPORTED_TARGET fontconfig)
endif()

add_executable(astral ${SOURCES})

target_include_directories(astral PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(astral PRIVATE raylib md4c nixie)

if(NOT WIN32 AND NOT APPLE AND NOT EMSCRIPTEN)
  target_link_libraries(astral PRIVATE PkgConfig::Fontconfig)
endif()

if(EMSCRIPTEN)
  set_target_properties(astral PROPERTIES
    SUFFIX ".html"
    # --shell-file/--embed-file below are linker flags pointing at files
    # CMake otherwise has no reason to track — without this, editing
    # web/shell.html or the bundled font doesn't trigger a relink.
    LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html;${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/default.ttf")
  target_link_options(astral PRIVATE
    "-sALLOW_MEMORY_GROWTH=1"
    # Astral's own C++ file I/O (ifstream/fopen) only pulls in the minimal
    # syscall shims — the full JS-side FS object (mkdir/writeFile, used by
    # web/shell.html's file picker to populate the virtual filesystem from
    # outside the running program) needs to be forced in explicitly.
    "-sFORCE_FILESYSTEM=1"
    "-sEXPORTED_RUNTIME_METHODS=[\"ccall\",\"cwrap\",\"FS\"]"
    "-sEXPORTED_FUNCTIONS=[\"_main\",\"_astral_open_document\"]"
    "--shell-file=${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html"
    "--embed-file=${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/default.ttf@assets/fonts/default.ttf"
    # raylib's own USE_GLFW=3 (glfw3-compatible input/context management,
    # see raylib/src/CMakeLists.txt) — re-added here because
    # cmake/Dependencies.cmake strips raylib's propagated link options to
    # stop them from overriding EXPORTED_RUNTIME_METHODS above.
    "-sUSE_GLFW=3")
endif()
