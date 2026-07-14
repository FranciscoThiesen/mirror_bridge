# MirrorBridgeModules.cmake
# ==========================
#
# The mirror_bridge_<lang>_module() helper functions. This file is used in
# two contexts and must keep working in both:
#
#   1. include()d by the root CMakeLists.txt (add_subdirectory/FetchContent
#      consumers and mirror_bridge's own test suite)
#   2. installed to lib/cmake/mirror_bridge and pulled in by
#      mirror_bridge-config.cmake (find_package consumers)
#
# In context 1 the root list file has already located Python/Lua/Node, so
# the lazy finders below are no-ops. In context 2 the finders supply the
# dependencies on first use.

include_guard(GLOBAL)

function(_mirror_bridge_find_python)
    if(NOT Python3_FOUND)
        find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
    endif()
endfunction()

macro(_mirror_bridge_find_lua)
    # Macro, not function: the LUA_* results must land in the caller's scope.
    if(NOT LUA_FOUND)
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(LUA QUIET lua5.4)
            if(NOT LUA_FOUND)
                pkg_check_modules(LUA QUIET lua)
            endif()
        endif()
        if(NOT LUA_FOUND)
            find_path(LUA_INCLUDE_DIR lua.h
                PATHS /usr/include/lua5.4 /usr/include/lua /usr/local/include
            )
            find_library(LUA_LIBRARY NAMES lua5.4 lua
                PATHS /usr/lib /usr/local/lib
            )
            if(LUA_INCLUDE_DIR AND LUA_LIBRARY)
                set(LUA_FOUND TRUE)
                set(LUA_INCLUDE_DIRS ${LUA_INCLUDE_DIR})
                set(LUA_LIBRARIES ${LUA_LIBRARY})
            endif()
        endif()
    endif()
endmacro()

macro(_mirror_bridge_find_napi)
    if(NOT NAPI_INCLUDE_DIR)
        find_path(NAPI_INCLUDE_DIR node_api.h
            PATHS /usr/include/node /usr/local/include/node
        )
    endif()
endmacro()

# Shared argument handling: sources may be given positionally or via
# SOURCES. OUTPUT_DIRECTORY overrides where the module file is written
# (default: the calling directory's binary dir).
#
# Usage: mirror_bridge_python_module(module_name source.cpp
#            [HEADERS ...] [INCLUDE_DIRS ...] [OUTPUT_DIRECTORY dir])
function(mirror_bridge_python_module TARGET_NAME)
    cmake_parse_arguments(ARG "" "OUTPUT_DIRECTORY" "SOURCES;HEADERS;INCLUDE_DIRS" ${ARGN})
    if(NOT ARG_SOURCES)
        set(ARG_SOURCES ${ARG_UNPARSED_ARGUMENTS})
    endif()

    _mirror_bridge_find_python()
    Python3_add_library(${TARGET_NAME} MODULE ${ARG_SOURCES})

    target_link_libraries(${TARGET_NAME} PRIVATE mirror_bridge::mirror_bridge)
    target_include_directories(${TARGET_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${ARG_INCLUDE_DIRS}
    )

    if(MIRROR_BRIDGE_USE_PCH AND TARGET mirror_bridge_pch_python)
        target_precompile_headers(${TARGET_NAME} REUSE_FROM mirror_bridge_pch_python)
    endif()

    set_target_properties(${TARGET_NAME} PROPERTIES
        PREFIX ""
        OUTPUT_NAME ${TARGET_NAME}
    )
    if(ARG_OUTPUT_DIRECTORY)
        set_target_properties(${TARGET_NAME} PROPERTIES
            LIBRARY_OUTPUT_DIRECTORY "${ARG_OUTPUT_DIRECTORY}"
        )
    endif()
endfunction()

function(mirror_bridge_lua_module TARGET_NAME)
    cmake_parse_arguments(ARG "" "OUTPUT_DIRECTORY" "SOURCES;HEADERS;INCLUDE_DIRS" ${ARGN})
    if(NOT ARG_SOURCES)
        set(ARG_SOURCES ${ARG_UNPARSED_ARGUMENTS})
    endif()

    _mirror_bridge_find_lua()
    if(NOT LUA_FOUND)
        message(FATAL_ERROR "mirror_bridge_lua_module(${TARGET_NAME}): Lua development files not found")
    endif()

    add_library(${TARGET_NAME} MODULE ${ARG_SOURCES})

    target_link_libraries(${TARGET_NAME} PRIVATE mirror_bridge::mirror_bridge ${LUA_LIBRARIES})
    target_include_directories(${TARGET_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${LUA_INCLUDE_DIRS}
        ${ARG_INCLUDE_DIRS}
    )

    if(MIRROR_BRIDGE_USE_PCH AND TARGET mirror_bridge_pch_lua)
        target_precompile_headers(${TARGET_NAME} REUSE_FROM mirror_bridge_pch_lua)
    endif()

    # Lua's require() expects a plain .so on every platform
    set_target_properties(${TARGET_NAME} PROPERTIES
        PREFIX ""
        SUFFIX ".so"
        OUTPUT_NAME ${TARGET_NAME}
    )
    if(ARG_OUTPUT_DIRECTORY)
        set_target_properties(${TARGET_NAME} PROPERTIES
            LIBRARY_OUTPUT_DIRECTORY "${ARG_OUTPUT_DIRECTORY}"
        )
    endif()
endfunction()

function(mirror_bridge_js_module TARGET_NAME)
    cmake_parse_arguments(ARG "" "OUTPUT_DIRECTORY" "SOURCES;HEADERS;INCLUDE_DIRS" ${ARGN})
    if(NOT ARG_SOURCES)
        set(ARG_SOURCES ${ARG_UNPARSED_ARGUMENTS})
    endif()

    _mirror_bridge_find_napi()
    if(NOT NAPI_INCLUDE_DIR)
        message(FATAL_ERROR "mirror_bridge_js_module(${TARGET_NAME}): Node.js N-API headers (node_api.h) not found")
    endif()

    add_library(${TARGET_NAME} MODULE ${ARG_SOURCES})

    target_link_libraries(${TARGET_NAME} PRIVATE mirror_bridge::mirror_bridge)
    target_include_directories(${TARGET_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${NAPI_INCLUDE_DIR}
        ${ARG_INCLUDE_DIRS}
    )

    if(MIRROR_BRIDGE_USE_PCH AND TARGET mirror_bridge_pch_js)
        target_precompile_headers(${TARGET_NAME} REUSE_FROM mirror_bridge_pch_js)
    endif()

    set_target_properties(${TARGET_NAME} PROPERTIES
        PREFIX ""
        SUFFIX ".node"
        OUTPUT_NAME ${TARGET_NAME}
    )
    if(ARG_OUTPUT_DIRECTORY)
        set_target_properties(${TARGET_NAME} PROPERTIES
            LIBRARY_OUTPUT_DIRECTORY "${ARG_OUTPUT_DIRECTORY}"
        )
    endif()
endfunction()
