# Tests CMakeLists.txt

# Test framework
add_library(TestFramework INTERFACE)
target_include_directories(TestFramework INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# Authentication Service Test
add_executable(AuthenticationServiceTest
    api/AuthenticationServiceTest.cpp
    ../src/api/AuthenticationService.cpp
)

target_link_libraries(AuthenticationServiceTest
    be
    network
    bnetapi
)

target_include_directories(AuthenticationServiceTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

# Test Runner
add_executable(TestRunner
    TestRunner.cpp
)

# Add test target
add_custom_target(run-tests
    COMMAND TestRunner
    DEPENDS TestRunner AuthenticationServiceTest
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Individual test targets
add_test(NAME AuthenticationService 
    COMMAND AuthenticationServiceTest
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)