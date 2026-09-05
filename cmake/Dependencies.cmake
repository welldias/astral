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

FetchContent_MakeAvailable(raylib md4c)
