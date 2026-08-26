# Mirror Bridge Tests - CMake Configuration
# ==========================================
#
# This file configures all test bindings and test runners.

# ============================================================================
# Python Binding Tests
# ============================================================================

if(MIRROR_BRIDGE_BUILD_PYTHON)

    # Cross-module test (Vertex and BoundingBox in separate modules)
    mirror_bridge_python_module(vertex
        cross_module_test/vertex_binding.cpp
        INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/cross_module_test
    )

    mirror_bridge_python_module(bbox
        cross_module_test/bbox_binding.cpp
        INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/cross_module_test
    )

    # Static method test (Vec3)
    mirror_bridge_python_module(vec3_test
        static_method_test/vec3_binding.cpp
        INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/static_method_test
    )

    # E2E Basic tests
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/e2e/basic/point2d)
        mirror_bridge_python_module(point2d
            e2e/basic/point2d/point2d_binding.cpp
            INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/e2e/basic/point2d
        )
    endif()

    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/e2e/basic/vector3)
        mirror_bridge_python_module(vector3
            e2e/basic/vector3/vector3_binding.cpp
            INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/e2e/basic/vector3
        )
    endif()

    # E2E Methods test (Calculator)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/e2e/methods/calculator)
        mirror_bridge_python_module(calculator
            e2e/methods/calculator/calculator_binding.cpp
            INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/e2e/methods/calculator
        )
    endif()

    # E2E Containers test
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/e2e/templates)
        mirror_bridge_python_module(container
            e2e/templates/container_binding.cpp
            INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/e2e/templates
        )
    endif()

    # E2E Nesting tests
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/e2e/nesting/person)
        mirror_bridge_python_module(person
            e2e/nesting/person/person_binding.cpp
            INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/e2e/nesting/person
        )
    endif()

    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/e2e/nesting/student)
        mirror_bridge_python_module(student
            e2e/nesting/student/student_binding.cpp
            INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/e2e/nesting/student
        )
    endif()

    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/e2e/nesting/company)
        mirror_bridge_python_module(company
            e2e/nesting/company/company_binding.cpp
            INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/e2e/nesting/company
        )
    endif()

    # E2E Advanced tests
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/e2e/advanced/constructors)
        mirror_bridge_python_module(rectangle
            e2e/advanced/constructors/rectangle_binding.cpp
            INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/e2e/advanced/constructors
        )
    endif()

    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/e2e/advanced/overloading)
        mirror_bridge_python_module(printer
            e2e/advanced/overloading/printer_binding.cpp
            INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/e2e/advanced/overloading
        )
    endif()

    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/e2e/advanced/variadic)
        mirror_bridge_python_module(math_ops
            e2e/advanced/variadic/math_ops_binding.cpp
            INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/e2e/advanced/variadic
        )
    endif()

    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/e2e/advanced/smart_ptrs)
        mirror_bridge_python_module(resource
            e2e/advanced/smart_ptrs/resource_binding.cpp
            INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/e2e/advanced/smart_ptrs
        )
    endif()

    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/e2e/containers/document)
        mirror_bridge_python_module(document
            e2e/containers/document/document_binding.cpp
            INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/e2e/containers/document
        )
    endif()

    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/e2e/containers/particle)
        mirror_bridge_python_module(particle
            e2e/containers/particle/particle_binding.cpp
            INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/e2e/containers/particle
        )
    endif()

    # E2E Callbacks test
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/e2e/callbacks)
        mirror_bridge_python_module(callbacks
            e2e/callbacks/callbacks_binding.cpp
            INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/e2e/callbacks
        )
    endif()

    # Single header test (robot)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/single_header_test)
        mirror_bridge_python_module(robot_py
            single_header_test/robot_py_binding.cpp
            INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/single_header_test
        )
    endif()

    # The single header must also compile with no repo include path at all
    # (issue #12). Runs the compiler directly against a copy in a temp dir.
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/single_header_test)
        add_test(NAME single_header_standalone
            COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/single_header_test/check_standalone.sh
                    ${CMAKE_CXX_COMPILER} ${Python3_INCLUDE_DIRS}
        )
    endif()

    # By-reference argument semantics (zero-copy const T&, visible T& mutation)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/byref_args)
        mirror_bridge_python_module(byref
            byref_args/byref_binding.cpp
            INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/byref_args
        )
    endif()

    # GIL release policy (release_gil(): native calls overlap across threads)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/gil_release)
        mirror_bridge_python_module(gil_worker
            gil_release/gil_release_binding.cpp
            INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/gil_release
        )
    endif()

    # const data members bind as read-only attributes
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/const_members)
        mirror_bridge_python_module(const_members
            const_members/const_members_binding.cpp
            INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/const_members
        )
    endif()

endif()

# ============================================================================
# Lua Binding Tests
# ============================================================================

if(MIRROR_BRIDGE_BUILD_LUA AND LUA_FOUND)

    # Lua modules go to their own subdirectory: several share an OUTPUT_NAME
    # with a Python module (e.g. point2d) and would clobber it in tests/.
    # The Lua test runner's LUA_CPATH points here.
    set(LUA_TEST_OUTPUT_DIR "${CMAKE_BINARY_DIR}/tests/lua")

    # Static method test (Vec3) - output as vec3.so to match test expectations
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/lua/static_method_test)
        mirror_bridge_lua_module(vec3_lua
            lua/static_method_test/vec3_binding.cpp
            INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/lua/static_method_test
            OUTPUT_DIRECTORY ${LUA_TEST_OUTPUT_DIR}
        )
        # Test expects 'vec3' not 'vec3_lua'
        set_target_properties(vec3_lua PROPERTIES OUTPUT_NAME vec3)
    endif()

    # Basic point2d test
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/lua/basic/point2d)
        mirror_bridge_lua_module(point2d_lua
            lua/basic/point2d/point2d_binding.cpp
            INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/lua/basic/point2d
            OUTPUT_DIRECTORY ${LUA_TEST_OUTPUT_DIR}
        )
        set_target_properties(point2d_lua PROPERTIES OUTPUT_NAME point2d)
    endif()

    # Comprehensive calculator test
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/lua/comprehensive/calculator)
        mirror_bridge_lua_module(calculator_lua
            lua/comprehensive/calculator/calculator_binding.cpp
            INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/lua/comprehensive/calculator
            OUTPUT_DIRECTORY ${LUA_TEST_OUTPUT_DIR}
        )
        set_target_properties(calculator_lua PROPERTIES OUTPUT_NAME calculator)
    endif()

    # Comprehensive features test
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/comprehensive_features)
        mirror_bridge_lua_module(features_lua
            comprehensive_features/features_binding_lua.cpp
            INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/comprehensive_features
            OUTPUT_DIRECTORY ${LUA_TEST_OUTPUT_DIR}
        )
    endif()

    # Single header test (robot)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/single_header_test)
        mirror_bridge_lua_module(robot_lua
            single_header_test/robot_lua.cpp
            INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/single_header_test
            OUTPUT_DIRECTORY ${LUA_TEST_OUTPUT_DIR}
        )
    endif()

endif()

# ============================================================================
# JavaScript Binding Tests
# ============================================================================

if(MIRROR_BRIDGE_BUILD_JS AND NAPI_FOUND)

    # Static method test (Vec3)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/js/static_method_test)
        mirror_bridge_js_module(vec3_js
            js/static_method_test/vec3_binding.cpp
            INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/js/static_method_test
        )
    endif()

    # Single header test (robot)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/single_header_test)
        mirror_bridge_js_module(robot_js
            single_header_test/robot_js.cpp
            INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/single_header_test
        )
    endif()

endif()

# ============================================================================
# Python Test Runner
# ============================================================================

# CTest contract: only tests whose binding modules are built by THIS CMake
# run are registered. Tests whose modules come from the bash harness
# (tests/run_all_tests.sh, per-directory build scripts, or the CLI) are
# excluded here — they are still exercised by CI's bash jobs. To promote
# one into ctest, add its module via mirror_bridge_python_module() above
# and remove its directory from this pattern.
set(MB_PYTHON_BASH_ONLY_PATTERN
    "auto_discovery|auto_trampoline|^trampoline|^readme|^inheritance|^exceptions|^expected|^bulk_transfer|^kwargs_defaults|^smart_ptr_wrapper|custom_converter|e2e/features"
)

# Find all Python test files and add them as CTest tests
file(GLOB_RECURSE PYTHON_TESTS "${CMAKE_CURRENT_SOURCE_DIR}/**/test_*.py")

foreach(TEST_FILE ${PYTHON_TESTS})
    get_filename_component(TEST_NAME ${TEST_FILE} NAME_WE)
    get_filename_component(TEST_DIR ${TEST_FILE} DIRECTORY)

    # Create unique test name by including relative path
    file(RELATIVE_PATH REL_PATH ${CMAKE_CURRENT_SOURCE_DIR} ${TEST_DIR})

    if(REL_PATH MATCHES "${MB_PYTHON_BASH_ONLY_PATTERN}")
        continue()
    endif()

    string(REPLACE "/" "_" REL_PATH_SAFE "${REL_PATH}")
    set(UNIQUE_TEST_NAME "python_${REL_PATH_SAFE}_${TEST_NAME}")

    add_test(
        NAME ${UNIQUE_TEST_NAME}
        COMMAND ${Python3_EXECUTABLE} ${TEST_FILE}
        WORKING_DIRECTORY ${TEST_DIR}
    )

    # Set PYTHONPATH to find built modules
    set_tests_properties(${UNIQUE_TEST_NAME} PROPERTIES
        ENVIRONMENT "PYTHONPATH=${CMAKE_BINARY_DIR}/tests:${CMAKE_BINARY_DIR}"
    )
endforeach()

# ============================================================================
# Lua Test Runner
# ============================================================================

if(LUA_FOUND)
    find_program(LUA_EXECUTABLE NAMES lua5.4 lua)

    if(LUA_EXECUTABLE)
        file(GLOB_RECURSE LUA_TESTS "${CMAKE_CURRENT_SOURCE_DIR}/**/test_*.lua")

        foreach(TEST_FILE ${LUA_TESTS})
            get_filename_component(TEST_NAME ${TEST_FILE} NAME_WE)
            get_filename_component(TEST_DIR ${TEST_FILE} DIRECTORY)

            # Create unique test name by including relative path
            file(RELATIVE_PATH REL_PATH ${CMAKE_CURRENT_SOURCE_DIR} ${TEST_DIR})
            string(REPLACE "/" "_" REL_PATH_SAFE "${REL_PATH}")
            set(UNIQUE_TEST_NAME "lua_${REL_PATH_SAFE}_${TEST_NAME}")

            # Skip tests whose modules the bash harness builds, not CMake
            # (same contract as the Python runner above)
            if(REL_PATH MATCHES "auto_discovery|custom_converter|smart_ptr_test|exception_test")
                continue()
            endif()

            add_test(
                NAME ${UNIQUE_TEST_NAME}
                COMMAND ${LUA_EXECUTABLE} ${TEST_FILE}
                WORKING_DIRECTORY ${TEST_DIR}
            )

            # Set LUA_CPATH to find built modules in lua/ subdirectory
            set_tests_properties(${UNIQUE_TEST_NAME} PROPERTIES
                ENVIRONMENT "LUA_CPATH=${CMAKE_BINARY_DIR}/tests/lua/?.so;;"
            )
        endforeach()
    endif()
endif()

# ============================================================================
# JavaScript Test Runner
# ============================================================================

if(NAPI_FOUND)
    find_program(NODE_EXECUTABLE NAMES node nodejs)

    if(NODE_EXECUTABLE)
        file(GLOB_RECURSE JS_TESTS "${CMAKE_CURRENT_SOURCE_DIR}/**/test_*.js")

        foreach(TEST_FILE ${JS_TESTS})
            get_filename_component(TEST_NAME ${TEST_FILE} NAME_WE)
            get_filename_component(TEST_DIR ${TEST_FILE} DIRECTORY)

            # Create unique test name by including relative path
            file(RELATIVE_PATH REL_PATH ${CMAKE_CURRENT_SOURCE_DIR} ${TEST_DIR})
            string(REPLACE "/" "_" REL_PATH_SAFE "${REL_PATH}")
            set(UNIQUE_TEST_NAME "js_${REL_PATH_SAFE}_${TEST_NAME}")

            # Skip tests whose modules the bash harness builds, not CMake
            # (same contract as the Python runner above)
            if(REL_PATH MATCHES "auto_discovery|custom_converter|smart_ptr_test|basic|comprehensive")
                continue()
            endif()

            add_test(
                NAME ${UNIQUE_TEST_NAME}
                COMMAND ${NODE_EXECUTABLE} ${TEST_FILE}
                WORKING_DIRECTORY ${TEST_DIR}
            )

            # Set NODE_PATH to find built modules
            set_tests_properties(${UNIQUE_TEST_NAME} PROPERTIES
                ENVIRONMENT "NODE_PATH=${CMAKE_BINARY_DIR}/tests"
            )
        endforeach()
    endif()
endif()
