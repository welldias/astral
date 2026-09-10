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

if(EMSCRIPTEN)
  # Tells raylib's own CMakeLists.txt to build against the Emscripten HTML5
  # API (GRAPHICS_API_OPENGL_ES2) instead of its desktop GLFW backend.
  set(PLATFORM "Web" CACHE STRING "" FORCE)
endif()

if (UNIX AND NOT APPLE AND NOT EMSCRIPTEN)
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

# nixie - parses Mermaid-style diagram
FetchContent_Declare(
  nixie
  GIT_REPOSITORY https://github.com/welldias/nixie-lib.git
  GIT_TAG        0.2.1
  GIT_SHALLOW    TRUE
)
set(NIXIE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(NIXIE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(NIXIE_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(NIXIE_BUILD_STATIC ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(raylib md4c nixie)

if(EMSCRIPTEN)
  set_property(TARGET raylib PROPERTY INTERFACE_LINK_OPTIONS "")
endif()

target_include_directories(nixie_static PUBLIC ${nixie_SOURCE_DIR}/include)
target_include_directories(nixie_static PRIVATE ${nixie_SOURCE_DIR}/external/stb_image ${nixie_SOURCE_DIR}/external/stb_truetype)
if(TARGET nixie_shared)
  target_include_directories(nixie_shared PUBLIC ${nixie_SOURCE_DIR}/include)
  target_include_directories(nixie_shared PRIVATE ${nixie_SOURCE_DIR}/external/stb_image ${nixie_SOURCE_DIR}/external/stb_truetype)
endif()

target_compile_definitions(nixie_static PRIVATE STB_IMAGE_WRITE_STATIC STBTT_STATIC)
if(TARGET nixie_shared)
  target_compile_definitions(nixie_shared PRIVATE STB_IMAGE_WRITE_STATIC STBTT_STATIC)
endif()
