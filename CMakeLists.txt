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
    ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/default_font_macos.cpp)
elseif(APPLE)
  list(REMOVE_ITEM SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/default_font_linux.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/default_font_win.cpp)
else()
  list(REMOVE_ITEM SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/default_font_win.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/default_font_macos.cpp)
  # default_font_linux.cpp uses the fontconfig API (Fc...) to locate the
  # system's default fonts.
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(Fontconfig REQUIRED IMPORTED_TARGET fontconfig)
endif()

add_executable(astral ${SOURCES})

target_include_directories(astral PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(astral PRIVATE raylib md4c nixie)

if(NOT WIN32 AND NOT APPLE)
  target_link_libraries(astral PRIVATE PkgConfig::Fontconfig)
endif()
