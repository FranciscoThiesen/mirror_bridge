# Mirror Bridge - CMake Build System
# ===================================
#
# This CMake configuration provides a modern, cross-platform build system
# for Mirror Bridge, replacing the bash-based scripts.
#
# FAST BUILD (Recommended):
#   cmake --preset fast
#   cmake --build --preset fast
#
# Standard Usage:
#   cmake --preset default
#   cmake --build --preset default --parallel
#   ctest --preset default
#
# Manual Usage (without presets):
#   mkdir build && cd build
#   cmake -GNinja .. -DCMAKE_CXX_COMPILER=clang++
#   cmake --build . --parallel $(nproc)
#   ctest
#
# Options:
#   -DMIRROR_BRIDGE_BUILD_TESTS=ON   Build tests (default: ON)
#   -DMIRROR_BRIDGE_BUILD_PYTHON=ON  Build Python bindings (default: ON)
#   -DMIRROR_BRIDGE_BUILD_LUA=ON     Build Lua bindings (default: ON)
#   -DMIRROR_BRIDGE_BUILD_JS=ON      Build JavaScript bindings (default: ON)
#   -DMIRROR_BRIDGE_USE_PCH=ON       Use precompiled headers (default: OFF)

cmake_minimum_required(VERSION 3.20)

project(mirror_bridge
    VERSION 0.3.0
    DESCRIPTION "Automatic C++ bindings using C++26 reflection"
    LANGUAGES CXX
)

# Whether this is the top-level project or consumed via FetchContent /
# add_subdirectory. Drives defaults: as a dependency we must not build
# tests or push global compile flags into the parent project.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(MIRROR_BRIDGE_IS_TOP_LEVEL ON)
else()
    set(MIRROR_BRIDGE_IS_TOP_LEVEL OFF)
endif()

# ============================================================================
# Options
# ============================================================================

option(MIRROR_BRIDGE_BUILD_TESTS "Build tests" ${MIRROR_BRIDGE_IS_TOP_LEVEL})
option(MIRROR_BRIDGE_BUILD_PYTHON "Build Python bindings" ON)
option(MIRROR_BRIDGE_BUILD_LUA "Build Lua bindings" ON)
option(MIRROR_BRIDGE_BUILD_JS "Build JavaScript/Node.js bindings" ON)
option(MIRROR_BRIDGE_USE_PCH "Use precompiled headers for faster builds" OFF)

# ============================================================================
# Compiler Configuration (C++26 with Reflection)
# ============================================================================

# Two reflection compilers are supported: Bloomberg clang-p2996 and stock
# GCC 16+. Flags differ (clang-p2996 needs its bundled libc++; GCC uses
# plain -std=c++26 -freflection with libstdc++).
# Note: -std flags are passed directly instead of CMAKE_CXX_STANDARD
# because CMake versions < 3.25 don't recognize C++26.
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(MIRROR_BRIDGE_CXX_FLAGS -std=c++2c -freflection -freflection-latest -stdlib=libc++)
    set(MIRROR_BRIDGE_LINK_FLAGS -stdlib=libc++)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 16)
    set(MIRROR_BRIDGE_CXX_FLAGS -std=c++26 -freflection)
    set(MIRROR_BRIDGE_LINK_FLAGS "")
else()
    message(WARNING "Mirror Bridge requires a C++26 reflection compiler: "
                    "Bloomberg clang-p2996 or stock GCC 16+. "
                    "Current compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
    set(MIRROR_BRIDGE_CXX_FLAGS -std=c++2c -freflection -freflection-latest -stdlib=libc++)
    set(MIRROR_BRIDGE_LINK_FLAGS -stdlib=libc++)
endif()

# Add C++26 and reflection flags globally — but only when building
# mirror_bridge itself. As a dependency (FetchContent/add_subdirectory)
# the flags travel on the mirror_bridge INTERFACE target instead, so the
# parent project's own targets are untouched.
if(MIRROR_BRIDGE_IS_TOP_LEVEL)
    add_compile_options(${MIRROR_BRIDGE_CXX_FLAGS})
    add_link_options(${MIRROR_BRIDGE_LINK_FLAGS})
endif()

# Set up library paths for libc++
if(EXISTS "/usr/local/lib/x86_64-unknown-linux-gnu")
    link_directories("/usr/local/lib/x86_64-unknown-linux-gnu")
    set(CMAKE_INSTALL_RPATH "/usr/local/lib/x86_64-unknown-linux-gnu")
elseif(EXISTS "/usr/local/lib/aarch64-unknown-linux-gnu")
    link_directories("/usr/local/lib/aarch64-unknown-linux-gnu")
    set(CMAKE_INSTALL_RPATH "/usr/local/lib/aarch64-unknown-linux-gnu")
endif()

# ============================================================================
# Header-Only Library Target
# ============================================================================

add_library(mirror_bridge INTERFACE)
add_library(mirror_bridge::mirror_bridge ALIAS mirror_bridge)

# The install include root mirrors the repo layout (core/, python/, ...)
# so the headers' repo-root-relative includes ("core/mirror_bridge_core.hpp",
# "../core/...") resolve identically from the build tree and an install.
target_include_directories(mirror_bridge INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/mirror_bridge>
)

# Usage requirements for consumers (find_package / FetchContent): linking
# mirror_bridge::mirror_bridge is enough to get the reflection flags.
# Both compiler branches are spelled out literally (not via
# MIRROR_BRIDGE_CXX_FLAGS) because the genex evaluates against the
# CONSUMER's compiler, which need not match the one configuring this
# project. Quoted so the ;-separated flag list stays inside one generator
# expression instead of splitting into broken arguments.
target_compile_options(mirror_bridge INTERFACE
    "$<$<COMPILE_LANG_AND_ID:CXX,Clang>:-std=c++2c;-freflection;-freflection-latest;-stdlib=libc++>"
    "$<$<COMPILE_LANG_AND_ID:CXX,GNU>:-std=c++26;-freflection>"
)
target_link_options(mirror_bridge INTERFACE
    "$<$<LINK_LANG_AND_ID:CXX,Clang>:-stdlib=libc++>"
)

# ============================================================================
# Precompiled Header Support (Optional - 3-4x faster builds)
# ============================================================================

if(MIRROR_BRIDGE_USE_PCH)
    message(STATUS "PCH enabled - builds will be significantly faster")

    # Create PCH target for Python bindings
    if(MIRROR_BRIDGE_BUILD_PYTHON)
        add_library(mirror_bridge_pch_python INTERFACE)
        target_precompile_headers(mirror_bridge_pch_python INTERFACE
            ${CMAKE_CURRENT_SOURCE_DIR}/mirror_bridge_pch.hpp
        )
    endif()

    # Create PCH target for Lua bindings
    if(MIRROR_BRIDGE_BUILD_LUA)
        add_library(mirror_bridge_pch_lua INTERFACE)
        target_precompile_headers(mirror_bridge_pch_lua INTERFACE
            ${CMAKE_CURRENT_SOURCE_DIR}/lua/mirror_bridge_lua_pch.hpp
        )
    endif()

    # Create PCH target for JavaScript bindings
    if(MIRROR_BRIDGE_BUILD_JS)
        add_library(mirror_bridge_pch_js INTERFACE)
        target_precompile_headers(mirror_bridge_pch_js INTERFACE
            ${CMAKE_CURRENT_SOURCE_DIR}/javascript/mirror_bridge_js_pch.hpp
        )
    endif()
endif()

# ============================================================================
# Find Dependencies
# ============================================================================

# Python
if(MIRROR_BRIDGE_BUILD_PYTHON OR MIRROR_BRIDGE_BUILD_TESTS)
    find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
    message(STATUS "Found Python ${Python3_VERSION}: ${Python3_EXECUTABLE}")
    message(STATUS "Python include: ${Python3_INCLUDE_DIRS}")
endif()

# Lua
if(MIRROR_BRIDGE_BUILD_LUA OR MIRROR_BRIDGE_BUILD_TESTS)
    find_package(PkgConfig)
    if(PkgConfig_FOUND)
        pkg_check_modules(LUA lua5.4)
        if(NOT LUA_FOUND)
            pkg_check_modules(LUA lua)
        endif()
    endif()

    if(NOT LUA_FOUND)
        # Manual search
        find_path(LUA_INCLUDE_DIR lua.h
            PATHS /usr/include/lua5.4 /usr/include/lua /usr/local/include
        )
        find_library(LUA_LIBRARY
            NAMES lua5.4 lua
            PATHS /usr/lib /usr/local/lib
        )
        if(LUA_INCLUDE_DIR AND LUA_LIBRARY)
            set(LUA_FOUND TRUE)
            set(LUA_INCLUDE_DIRS ${LUA_INCLUDE_DIR})
            set(LUA_LIBRARIES ${LUA_LIBRARY})
        endif()
    endif()

    if(LUA_FOUND)
        message(STATUS "Found Lua: ${LUA_INCLUDE_DIRS}")
    else()
        message(WARNING "Lua not found - Lua bindings will be disabled")
        set(MIRROR_BRIDGE_BUILD_LUA OFF)
    endif()
endif()

# Node.js / N-API
if(MIRROR_BRIDGE_BUILD_JS OR MIRROR_BRIDGE_BUILD_TESTS)
    find_path(NAPI_INCLUDE_DIR node_api.h
        PATHS /usr/include/node /usr/local/include/node
    )

    if(NAPI_INCLUDE_DIR)
        set(NAPI_FOUND TRUE)
        message(STATUS "Found Node.js N-API: ${NAPI_INCLUDE_DIR}")
    else()
        message(WARNING "Node.js N-API not found - JavaScript bindings will be disabled")
        set(MIRROR_BRIDGE_BUILD_JS OFF)
    endif()
endif()

# ============================================================================
# Helper Functions for Creating Bindings
# ============================================================================

# mirror_bridge_python_module() / mirror_bridge_lua_module() /
# mirror_bridge_js_module() live in a separate file because they are also
# installed with the package, so find_package consumers get the exact same
# helper API as add_subdirectory/FetchContent consumers.
include(cmake/MirrorBridgeModules.cmake)

# ============================================================================
# Build Tests
# ============================================================================

if(MIRROR_BRIDGE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ============================================================================
# Installation
# ============================================================================

install(TARGETS mirror_bridge
    EXPORT mirror_bridge-targets
)

# No trailing slashes: preserve the core/python/lua/javascript directory
# structure under include/mirror_bridge so inter-header includes resolve
# (a flattened install would break "core/mirror_bridge_core.hpp").
install(DIRECTORY core python lua javascript rust
    DESTINATION include/mirror_bridge
    FILES_MATCHING PATTERN "*.hpp"
)

install(FILES mirror_bridge.hpp mirror_bridge_pch.hpp
    DESTINATION include/mirror_bridge
)

install(EXPORT mirror_bridge-targets
    FILE mirror_bridge-targets.cmake
    NAMESPACE mirror_bridge::
    DESTINATION lib/cmake/mirror_bridge
)

# The config file chains the exported target with the module helper
# functions, so find_package delivers the same API as add_subdirectory.
include(CMakePackageConfigHelpers)
configure_package_config_file(
    cmake/mirror_bridge-config.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/mirror_bridge-config.cmake"
    INSTALL_DESTINATION lib/cmake/mirror_bridge
)

# Version file so `find_package(mirror_bridge 0.3 REQUIRED)` can check
# compatibility. ARCH_INDEPENDENT because the library is header-only.
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/mirror_bridge-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMinorVersion
    ARCH_INDEPENDENT
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/mirror_bridge-config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/mirror_bridge-config-version.cmake"
    cmake/MirrorBridgeModules.cmake
    DESTINATION lib/cmake/mirror_bridge
)

# ============================================================================
# Summary
# ============================================================================

message(STATUS "")
message(STATUS "Mirror Bridge Configuration:")
message(STATUS "  Version:     ${PROJECT_VERSION}")
message(STATUS "  C++ Standard: C++26 (via -std=c++2c)")
message(STATUS "  Compiler:    ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  Generator:   ${CMAKE_GENERATOR}")
message(STATUS "  Build Tests: ${MIRROR_BRIDGE_BUILD_TESTS}")
message(STATUS "  Python:      ${MIRROR_BRIDGE_BUILD_PYTHON}")
message(STATUS "  Lua:         ${MIRROR_BRIDGE_BUILD_LUA}")
message(STATUS "  JavaScript:  ${MIRROR_BRIDGE_BUILD_JS}")
message(STATUS "  PCH:         ${MIRROR_BRIDGE_USE_PCH}")
if(NOT CMAKE_GENERATOR MATCHES "Ninja")
    message(STATUS "")
    message(STATUS "  TIP: Use Ninja for faster builds:")
    message(STATUS "       cmake --preset default")
endif()
message(STATUS "")
