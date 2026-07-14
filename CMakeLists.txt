cmake_minimum_required(VERSION 3.29)

project(open_control_framework LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

set(MS_UNITY_SOURCE_DIR
    ""
    CACHE PATH "Path to the Unity test framework source directory")

if(MS_UNITY_SOURCE_DIR AND NOT EXISTS "${MS_UNITY_SOURCE_DIR}/src/unity.c")
    message(FATAL_ERROR "Unity source not found at ${MS_UNITY_SOURCE_DIR}")
endif()

if(MS_UNITY_SOURCE_DIR AND NOT TARGET unity)
    add_library(unity STATIC "${MS_UNITY_SOURCE_DIR}/src/unity.c")
    target_include_directories(unity PUBLIC "${MS_UNITY_SOURCE_DIR}/src")
endif()

file(GLOB_RECURSE OC_FRAMEWORK_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_library(oc_framework_native STATIC ${OC_FRAMEWORK_SOURCES})
target_include_directories(oc_framework_native PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")

set(OC_FRAMEWORK_BUILD_TESTS_DEFAULT OFF)
if(PROJECT_IS_TOP_LEVEL)
    set(OC_FRAMEWORK_BUILD_TESTS_DEFAULT ON)
endif()

option(
    OC_FRAMEWORK_BUILD_TESTS
    "Build and register OpenControl framework unit tests"
    ${OC_FRAMEWORK_BUILD_TESTS_DEFAULT})

include(CTest)

if(OC_FRAMEWORK_BUILD_TESTS AND BUILD_TESTING)
    if(NOT TARGET unity)
        message(FATAL_ERROR "MS_UNITY_SOURCE_DIR is required for tests. Run tests through `uv run ms test ...`.")
    endif()

    find_package(Python3 REQUIRED COMPONENTS Interpreter)

    add_test(
        NAME oc_framework_architecture
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/script/dev/check_architecture.py")
    set_tests_properties(oc_framework_architecture PROPERTIES LABELS open-control-framework)

    file(GLOB OC_FRAMEWORK_TEST_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/test/test_*/*.cpp")

    set(OC_FRAMEWORK_TEST_TARGETS)

    foreach(test_source IN LISTS OC_FRAMEWORK_TEST_SOURCES)
        get_filename_component(test_dir "${test_source}" DIRECTORY)
        get_filename_component(test_name "${test_dir}" NAME)
        set(test_target "oc_framework_${test_name}")
        list(APPEND OC_FRAMEWORK_TEST_TARGETS "${test_target}")

        add_executable("${test_target}" "${test_source}")
        target_link_libraries("${test_target}" PRIVATE oc_framework_native unity)
        target_include_directories("${test_target}"
            PRIVATE
                "${CMAKE_CURRENT_SOURCE_DIR}/test"
                "${CMAKE_CURRENT_SOURCE_DIR}/src")

        add_test(NAME "${test_name}" COMMAND "${test_target}")
        set_tests_properties("${test_name}" PROPERTIES LABELS open-control-framework)
    endforeach()

    # Keep the opt-in NotificationQueue diagnostics covered by the default test
    # workflow without enabling stats in the production-equivalent library.
    add_library(
        oc_framework_notificationqueue_diagnostics_support
        STATIC
        "${CMAKE_CURRENT_SOURCE_DIR}/src/oc/diagnostics/Performance.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/oc/log/Log.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/oc/state/NotificationQueue.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/oc/state/Signal.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/oc/time/Time.cpp")
    target_include_directories(
        oc_framework_notificationqueue_diagnostics_support
        PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_definitions(
        oc_framework_notificationqueue_diagnostics_support
        PUBLIC OC_ENABLE_STATS=1 OC_LOG)

    set(notificationqueue_diagnostics_target
        oc_framework_test_notificationqueue_diagnostics)
    add_executable(
        "${notificationqueue_diagnostics_target}"
        "${CMAKE_CURRENT_SOURCE_DIR}/test/test_notificationqueue/test_notificationqueue.cpp")
    target_link_libraries(
        "${notificationqueue_diagnostics_target}"
        PRIVATE oc_framework_notificationqueue_diagnostics_support unity)
    target_include_directories(
        "${notificationqueue_diagnostics_target}"
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/test"
            "${CMAKE_CURRENT_SOURCE_DIR}/src")
    list(APPEND OC_FRAMEWORK_TEST_TARGETS "${notificationqueue_diagnostics_target}")
    add_test(
        NAME test_notificationqueue_diagnostics
        COMMAND "${notificationqueue_diagnostics_target}")
    set_tests_properties(
        test_notificationqueue_diagnostics
        PROPERTIES LABELS open-control-framework)

    if(PROJECT_IS_TOP_LEVEL)
        add_custom_target(test-native
            COMMAND "${CMAKE_CTEST_COMMAND}" --test-dir "${CMAKE_BINARY_DIR}" --parallel --output-on-failure
            DEPENDS ${OC_FRAMEWORK_TEST_TARGETS}
            USES_TERMINAL)
    endif()
endif()
