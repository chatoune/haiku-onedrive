# OneDrive Tests CMake Configuration
# Builds unit tests, integration tests, and test utilities for the OneDrive project

cmake_minimum_required(VERSION 3.16)

# Enable testing
enable_testing()

# Find required packages for testing
find_package(PkgConfig REQUIRED)

# Check for CppUnit (Haiku's testing framework)
# Note: On Haiku, we might need to use a simple test framework if CppUnit is not available
set(CPPUNIT_FOUND FALSE)
find_path(CPPUNIT_INCLUDE_DIR cppunit/TestCase.h
    PATHS /system/develop/headers /boot/system/develop/headers)
find_library(CPPUNIT_LIBRARY cppunit
    PATHS /system/develop/lib /boot/system/develop/lib)

if(CPPUNIT_INCLUDE_DIR AND CPPUNIT_LIBRARY)
    set(CPPUNIT_FOUND TRUE)
    message(STATUS "Found CppUnit: ${CPPUNIT_LIBRARY}")
else()
    message(WARNING "CppUnit not found - using simple test framework")
endif()

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/api
    ${CMAKE_SOURCE_DIR}/src/daemon
    ${CMAKE_SOURCE_DIR}/src/shared
    ${CMAKE_SOURCE_DIR}/src/preferences
)

if(CPPUNIT_FOUND)
    include_directories(${CPPUNIT_INCLUDE_DIR})
endif()

# Test source files
set(AUTH_TEST_SOURCES
    AuthManagerTest.cpp
)

set(API_TEST_SOURCES
    OneDriveAPITest.cpp
)

set(DAEMON_TEST_SOURCES
    OneDriveDaemonTest.cpp
)

set(INTEGRATION_TEST_SOURCES
    IntegrationTest.cpp
)

# Simple test framework for Haiku if CppUnit not available
if(NOT CPPUNIT_FOUND)
    set(SIMPLE_TEST_FRAMEWORK_SOURCES
        SimpleTestFramework.cpp
    )
endif()

# Test runner main
set(TEST_RUNNER_SOURCES
    TestRunner.cpp
)

# Link libraries for tests
set(TEST_LINK_LIBRARIES
    OneDriveAPI  # Our API library
    be           # Haiku base library
    network      # Haiku network library
    tracker      # Haiku tracker library
)

if(CPPUNIT_FOUND)
    list(APPEND TEST_LINK_LIBRARIES ${CPPUNIT_LIBRARY})
endif()

# Compiler flags for tests
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -std=c++17")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")

# Create simple test framework if needed
if(NOT CPPUNIT_FOUND)
    add_library(SimpleTestFramework STATIC ${SIMPLE_TEST_FRAMEWORK_SOURCES})
    target_link_libraries(SimpleTestFramework be)
endif()

# AuthenticationManager Tests
add_executable(auth_tests
    ${AUTH_TEST_SOURCES}
)

target_link_libraries(auth_tests
    ${TEST_LINK_LIBRARIES}
)

# OneDriveAPI Tests
add_executable(api_tests
    ${API_TEST_SOURCES}
)

target_link_libraries(api_tests
    ${TEST_LINK_LIBRARIES}
)

# OneDriveDaemon Tests
add_executable(daemon_tests
    ${DAEMON_TEST_SOURCES}
)

target_link_libraries(daemon_tests
    ${TEST_LINK_LIBRARIES}
)

# Integration Tests
add_executable(integration_tests
    ${INTEGRATION_TEST_SOURCES}
)

target_link_libraries(integration_tests
    ${TEST_LINK_LIBRARIES}
)

# Comprehensive Test Runner
add_executable(onedrive_tests
    ${TEST_RUNNER_SOURCES}
)

target_link_libraries(onedrive_tests
    ${TEST_LINK_LIBRARIES}
)

# Add tests to CTest
add_test(NAME AuthManagerTests COMMAND auth_tests)
add_test(NAME OneDriveAPITests COMMAND api_tests)
add_test(NAME OneDriveDaemonTests COMMAND daemon_tests)
add_test(NAME IntegrationTests COMMAND integration_tests)
add_test(NAME AllTests COMMAND onedrive_tests)

# Set test properties
set_tests_properties(AuthManagerTests PROPERTIES
    TIMEOUT 60
    LABELS "unit;auth"
)

set_tests_properties(OneDriveAPITests PROPERTIES
    TIMEOUT 120
    LABELS "unit;api"
)

set_tests_properties(OneDriveDaemonTests PROPERTIES
    TIMEOUT 60
    LABELS "unit;daemon"
)

set_tests_properties(IntegrationTests PROPERTIES
    TIMEOUT 180
    LABELS "integration"
)

set_tests_properties(AllTests PROPERTIES
    TIMEOUT 300
    LABELS "comprehensive"
)

# Custom targets for running specific test categories
add_custom_target(test_unit
    COMMAND ${CMAKE_CTEST_COMMAND} -L "unit" --output-on-failure
    DEPENDS auth_tests api_tests daemon_tests
    COMMENT "Running unit tests"
)

add_custom_target(test_integration
    COMMAND ${CMAKE_CTEST_COMMAND} -L "integration" --output-on-failure
    DEPENDS integration_tests
    COMMENT "Running integration tests"
)

add_custom_target(test_all
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS auth_tests api_tests daemon_tests integration_tests onedrive_tests
    COMMENT "Running all tests"
)

# Test coverage target (if gcov is available)
find_program(GCOV_PATH gcov)
if(GCOV_PATH)
    add_custom_target(test_coverage
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/generate_coverage.sh
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        DEPENDS auth_tests api_tests daemon_tests integration_tests
        COMMENT "Running tests with coverage analysis"
    )
endif()

# Memory testing with valgrind (if available)
find_program(VALGRIND_PATH valgrind)
if(VALGRIND_PATH)
    add_custom_target(test_memory
        COMMAND ${CMAKE_CTEST_COMMAND} -T memcheck --output-on-failure
        DEPENDS auth_tests api_tests daemon_tests integration_tests
        COMMENT "Running tests with memory checking"
    )
endif()

# Install test executables (optional, for system testing)
if(INSTALL_TESTS)
    install(TARGETS auth_tests api_tests daemon_tests integration_tests onedrive_tests
        DESTINATION bin/onedrive-tests
        COMPONENT tests
    )
    
    install(FILES GUITestCases.md
        DESTINATION share/doc/onedrive-tests
        COMPONENT tests
    )
endif()

# Test data files
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/test_config.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/test_config.h
    @ONLY
)

# Add test configuration to include path
target_include_directories(auth_tests PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(api_tests PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(daemon_tests PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(integration_tests PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(onedrive_tests PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

# Print test configuration summary
message(STATUS "Tests Configuration:")
message(STATUS "  CppUnit Found: ${CPPUNIT_FOUND}")
message(STATUS "  Test Framework: ${CMAKE_CXX_FLAGS}")
message(STATUS "  Test Targets: auth_tests, api_tests, daemon_tests, integration_tests, onedrive_tests")
message(STATUS "  Custom Targets: test_unit, test_integration, test_all")
if(GCOV_PATH)
    message(STATUS "  Coverage Target: test_coverage (gcov found)")
endif()
if(VALGRIND_PATH)
    message(STATUS "  Memory Target: test_memory (valgrind found)")
endif()

# Development helpers
add_custom_target(tests_help
    COMMAND ${CMAKE_COMMAND} -E echo "Available test targets:"
    COMMAND ${CMAKE_COMMAND} -E echo "  make test_unit       - Run unit tests only"
    COMMAND ${CMAKE_COMMAND} -E echo "  make test_integration - Run integration tests only"
    COMMAND ${CMAKE_COMMAND} -E echo "  make test_all        - Run all tests"
    COMMAND ${CMAKE_COMMAND} -E echo "  make test            - Run all tests (CTest)"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Individual test executables:"
    COMMAND ${CMAKE_COMMAND} -E echo "  ./auth_tests         - AuthenticationManager tests"
    COMMAND ${CMAKE_COMMAND} -E echo "  ./api_tests          - OneDriveAPI tests"
    COMMAND ${CMAKE_COMMAND} -E echo "  ./daemon_tests       - OneDriveDaemon tests"
    COMMAND ${CMAKE_COMMAND} -E echo "  ./integration_tests  - Cross-component tests"
    COMMAND ${CMAKE_COMMAND} -E echo "  ./onedrive_tests     - All tests in one runner"
    COMMENT "Displaying test help"
)

# Ensure tests depend on the main libraries
add_dependencies(auth_tests OneDriveAPI)
add_dependencies(api_tests OneDriveAPI)
add_dependencies(daemon_tests OneDriveAPI)
add_dependencies(integration_tests OneDriveAPI)
add_dependencies(onedrive_tests OneDriveAPI)