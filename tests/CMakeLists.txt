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

endif()

# ============================================================================
# Lua Binding Tests
# ============================================================================

if(MIRROR_BRIDGE_BUILD_LUA AND LUA_FOUND)

    # Static method test (Vec3) - output as vec3.so to match test expectations
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/lua/static_method_test)
        mirror_bridge_lua_module(vec3_lua
            lua/static_method_test/vec3_binding.cpp
            INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/lua/static_method_test
        )
        # Test expects 'vec3' not 'vec3_lua'
        set_target_properties(vec3_lua PROPERTIES OUTPUT_NAME vec3)
    endif()

    # Basic point2d test
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/lua/basic/point2d)
        mirror_bridge_lua_module(point2d_lua
            lua/basic/point2d/point2d_binding.cpp
            INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/lua/basic/point2d
        )
        set_target_properties(point2d_lua PROPERTIES OUTPUT_NAME point2d)
    endif()

    # Comprehensive calculator test
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/lua/comprehensive/calculator)
        mirror_bridge_lua_module(calculator_lua
            lua/comprehensive/calculator/calculator_binding.cpp
            INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/lua/comprehensive/calculator
        )
        set_target_properties(calculator_lua PROPERTIES OUTPUT_NAME calculator)
    endif()

    # Comprehensive features test
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/comprehensive_features)
        mirror_bridge_lua_module(features_lua
            comprehensive_features/features_binding_lua.cpp
            INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/comprehensive_features
        )
    endif()

    # Single header test (robot)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/single_header_test)
        mirror_bridge_lua_module(robot_lua
            single_header_test/robot_lua.cpp
            INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/single_header_test
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

# Find all Python test files and add them as CTest tests
file(GLOB_RECURSE PYTHON_TESTS "${CMAKE_CURRENT_SOURCE_DIR}/**/test_*.py")

foreach(TEST_FILE ${PYTHON_TESTS})
    get_filename_component(TEST_NAME ${TEST_FILE} NAME_WE)
    get_filename_component(TEST_DIR ${TEST_FILE} DIRECTORY)

    # Create unique test name by including relative path
    file(RELATIVE_PATH REL_PATH ${CMAKE_CURRENT_SOURCE_DIR} ${TEST_DIR})
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

            # Skip auto_discovery tests (use bash scripts to generate bindings)
            if(REL_PATH MATCHES "auto_discovery")
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

            # Skip tests for modules not built by CMake
            # (auto_discovery uses bash scripts)
            if(REL_PATH MATCHES "auto_discovery")
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
