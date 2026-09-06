include(FetchContent)

set(FETCHCONTENT_QUIET OFF)

# raylib - rendering, windowing, input
FetchContent_Declare(
  raylib
  GIT_REPOSITORY https://github.com/raysan5/raylib.git
  GIT_TAG        6.0
  GIT_SHALLOW    TRUE
)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

if (UNIX AND NOT APPLE)
  # Linux: enables both GLFW backends to work under both X11 and
  # Wayland sessions (chosen at runtime).
  set(GLFW_BUILD_WAYLAND ON CACHE BOOL "" FORCE)
  set(GLFW_BUILD_X11 ON CACHE BOOL "" FORCE)
endif()

# md4c - Markdown parsing (we only use the "md4c" target; no md2html executable)
FetchContent_Declare(
  md4c
  GIT_REPOSITORY https://github.com/mity/md4c.git
  GIT_TAG        release-0.5.3
  GIT_SHALLOW    TRUE
)
set(BUILD_MD2HTML_EXECUTABLE OFF CACHE BOOL "" FORCE)

# nixie - parses Mermaid-style diagram text and renders it to SVG/PNG/ASCII
# (we only use the "nixie" target; no nixie-tool CLI, no test suite). No
# tags published yet, pinned to a known-good commit instead.
FetchContent_Declare(
  nixie
  GIT_REPOSITORY https://github.com/welldias/nixie-lib.git
  GIT_TAG        4de60ccea8fa3ae05fdcfeae8287763cfc2dd6cd
  GIT_SHALLOW    FALSE
)
set(NIXIE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(NIXIE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(NIXIE_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(NIXIE_BUILD_STATIC ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(raylib md4c nixie)

# Workaround: nixie-lib's own src/CMakeLists.txt derives ALL of its include
# paths (its own public include/, plus the vendored stb_image/stb_truetype
# headers) from CMAKE_SOURCE_DIR (the outermost project's root, i.e.
# astral's own root once nixie is pulled in as a subproject here) instead
# of CMAKE_CURRENT_SOURCE_DIR (nixie's own root) — that mistake makes both
# "#include <nixie/nixie.h>" and the internal stb_image_write.h/
# stb_truetype.h includes unresolvable, even inside nixie's own .c files,
# once it's consumed via FetchContent/add_subdirectory rather than built
# standalone. Adding the correct absolute paths here fixes it without
# needing to patch the fetched source.
target_include_directories(nixie_static PUBLIC ${nixie_SOURCE_DIR}/include)
target_include_directories(nixie_static PRIVATE ${nixie_SOURCE_DIR}/external/stb_image ${nixie_SOURCE_DIR}/external/stb_truetype)
if(TARGET nixie_shared)
  target_include_directories(nixie_shared PUBLIC ${nixie_SOURCE_DIR}/include)
  target_include_directories(nixie_shared PRIVATE ${nixie_SOURCE_DIR}/external/stb_image ${nixie_SOURCE_DIR}/external/stb_truetype)
endif()

# raylib also vendors its own copies of stb_image_write.h/stb_truetype.h
# (see raylib's src/external/) and compiles them with the default
# extern-linkage implementation, same as nixie's own copies — linking both
# static libraries into the same "astral" executable then fails with
# "multiple definition of stbi_write_png"/stb_truetype symbols, since both
# archives export the exact same global symbol names. STB_IMAGE_WRITE_STATIC/
# STBTT_STATIC are the libraries' own documented mechanism for exactly this
# situation: they make nixie's copies use internal (static) linkage instead,
# so they no longer collide with raylib's.
target_compile_definitions(nixie_static PRIVATE STB_IMAGE_WRITE_STATIC STBTT_STATIC)
if(TARGET nixie_shared)
  target_compile_definitions(nixie_shared PRIVATE STB_IMAGE_WRITE_STATIC STBTT_STATIC)
endif()
