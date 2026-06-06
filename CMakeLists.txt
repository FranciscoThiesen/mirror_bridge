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

# Check for clang with reflection support
if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    message(WARNING "Mirror Bridge requires clang-p2996 with reflection support. "
                    "Current compiler: ${CMAKE_CXX_COMPILER_ID}")
endif()

# C++26 reflection flags for clang-p2996
# Note: We use -std=c++2c directly instead of CMAKE_CXX_STANDARD because
# CMake versions < 3.25 don't recognize C++26
set(MIRROR_BRIDGE_CXX_FLAGS
    -std=c++2c
    -freflection
    -freflection-latest
    -stdlib=libc++
)

# Add C++26 and reflection flags globally — but only when building
# mirror_bridge itself. As a dependency (FetchContent/add_subdirectory)
# the flags travel on the mirror_bridge INTERFACE target instead, so the
# parent project's own targets are untouched.
if(MIRROR_BRIDGE_IS_TOP_LEVEL)
    add_compile_options(${MIRROR_BRIDGE_CXX_FLAGS})
    add_link_options(-stdlib=libc++)
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
# Restricted to Clang since only clang-p2996 understands -freflection.
# Quoted so the ;-separated flag list stays inside one generator
# expression instead of splitting into broken arguments.
target_compile_options(mirror_bridge INTERFACE
    "$<$<COMPILE_LANG_AND_ID:CXX,Clang>:${MIRROR_BRIDGE_CXX_FLAGS}>"
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

# Function to create a Python binding module
# Usage: mirror_bridge_python_module(module_name source.cpp [HEADERS header1.hpp ...])
function(mirror_bridge_python_module TARGET_NAME)
    cmake_parse_arguments(ARG "" "" "SOURCES;HEADERS;INCLUDE_DIRS" ${ARGN})

    if(NOT ARG_SOURCES)
        set(ARG_SOURCES ${ARGN})
        list(FILTER ARG_SOURCES EXCLUDE REGEX "^(HEADERS|INCLUDE_DIRS)$")
    endif()

    Python3_add_library(${TARGET_NAME} MODULE ${ARG_SOURCES})

    target_link_libraries(${TARGET_NAME} PRIVATE mirror_bridge)
    target_include_directories(${TARGET_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${ARG_INCLUDE_DIRS}
    )

    # Use PCH if enabled (3-4x faster builds)
    if(MIRROR_BRIDGE_USE_PCH AND TARGET mirror_bridge_pch_python)
        target_precompile_headers(${TARGET_NAME} REUSE_FROM mirror_bridge_pch_python)
    endif()

    # Set output name without lib prefix
    set_target_properties(${TARGET_NAME} PROPERTIES
        PREFIX ""
        OUTPUT_NAME ${TARGET_NAME}
    )
endfunction()

# Function to create a Lua binding module
# Usage: mirror_bridge_lua_module(module_name source.cpp [HEADERS header1.hpp ...])
function(mirror_bridge_lua_module TARGET_NAME)
    cmake_parse_arguments(ARG "" "" "SOURCES;HEADERS;INCLUDE_DIRS" ${ARGN})

    if(NOT ARG_SOURCES)
        set(ARG_SOURCES ${ARGN})
        list(FILTER ARG_SOURCES EXCLUDE REGEX "^(HEADERS|INCLUDE_DIRS)$")
    endif()

    add_library(${TARGET_NAME} MODULE ${ARG_SOURCES})

    target_link_libraries(${TARGET_NAME} PRIVATE mirror_bridge ${LUA_LIBRARIES})
    target_include_directories(${TARGET_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${LUA_INCLUDE_DIRS}
        ${ARG_INCLUDE_DIRS}
    )

    # Use PCH if enabled (3-4x faster builds)
    if(MIRROR_BRIDGE_USE_PCH AND TARGET mirror_bridge_pch_lua)
        target_precompile_headers(${TARGET_NAME} REUSE_FROM mirror_bridge_pch_lua)
    endif()

    # Set output name without lib prefix, with .so extension
    # Output to lua/ subdirectory to avoid conflicts with Python modules
    set_target_properties(${TARGET_NAME} PROPERTIES
        PREFIX ""
        SUFFIX ".so"
        OUTPUT_NAME ${TARGET_NAME}
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/lua"
    )
endfunction()

# Function to create a JavaScript/Node.js binding module
# Usage: mirror_bridge_js_module(module_name source.cpp [HEADERS header1.hpp ...])
function(mirror_bridge_js_module TARGET_NAME)
    cmake_parse_arguments(ARG "" "" "SOURCES;HEADERS;INCLUDE_DIRS" ${ARGN})

    if(NOT ARG_SOURCES)
        set(ARG_SOURCES ${ARGN})
        list(FILTER ARG_SOURCES EXCLUDE REGEX "^(HEADERS|INCLUDE_DIRS)$")
    endif()

    add_library(${TARGET_NAME} MODULE ${ARG_SOURCES})

    target_link_libraries(${TARGET_NAME} PRIVATE mirror_bridge)
    target_include_directories(${TARGET_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${NAPI_INCLUDE_DIR}
        ${ARG_INCLUDE_DIRS}
    )

    # Use PCH if enabled (3-4x faster builds)
    if(MIRROR_BRIDGE_USE_PCH AND TARGET mirror_bridge_pch_js)
        target_precompile_headers(${TARGET_NAME} REUSE_FROM mirror_bridge_pch_js)
    endif()

    # Set output as .node file
    set_target_properties(${TARGET_NAME} PROPERTIES
        PREFIX ""
        SUFFIX ".node"
        OUTPUT_NAME ${TARGET_NAME}
    )
endfunction()

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
    FILE mirror_bridge-config.cmake
    NAMESPACE mirror_bridge::
    DESTINATION lib/cmake/mirror_bridge
)

# Version file so `find_package(mirror_bridge 0.2 REQUIRED)` can check
# compatibility. ARCH_INDEPENDENT because the library is header-only.
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/mirror_bridge-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMinorVersion
    ARCH_INDEPENDENT
)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/mirror_bridge-config-version.cmake"
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
