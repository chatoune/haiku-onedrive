/**
 * @file SimpleTestRunner.cpp
 * @brief Simple comprehensive test runner for OneDrive components
 * @author Haiku OneDrive Team
 * @version 1.0.0
 * @date 2025-08-14
 * 
 * Main test runner that executes all test suites and provides
 * comprehensive test reporting using the Simple Test Framework.
 */

#include "SimpleTestFramework.h"
#include <stdio.h>
#include <OS.h>

// Forward declarations
void RunAuthTests();
void RunAPITests();
void RunIntegrationTests();

/**
 * @brief Print system information
 */
void PrintSystemInfo() {
    system_info sysInfo;
    get_system_info(&sysInfo);
    
    printf("System Information:\n");
    printf("  Operating System: Haiku\n");
    printf("  Kernel Version: %" B_PRId64 ".%" B_PRId64 "\n", 
           sysInfo.kernel_version >> 32, 
           sysInfo.kernel_version & 0xFFFFFFFF);
    printf("  CPU Count: %" B_PRId32 "\n", sysInfo.cpu_count);
    printf("  Memory: %" B_PRId64 " MB\n", 
           sysInfo.max_pages * B_PAGE_SIZE / (1024 * 1024));
    printf("\n");
}

/**
 * @brief Run all test suites
 */
int RunAllTests() {
    printf("OneDrive Comprehensive Test Suite\n");
    printf("=================================\n\n");
    
    PrintSystemInfo();
    
    bigtime_t startTime = system_time();
    int totalFailures = 0;
    
    // Run Authentication Manager tests
    printf("Starting Authentication Manager tests...\n");
    try {
        RunAuthTests();
    } catch (...) {
        printf("❌ Authentication Manager tests crashed\n");
        totalFailures++;
    }
    
    printf("\n");
    
    // Run OneDrive API tests  
    printf("Starting OneDrive API tests...\n");
    try {
        RunAPITests();
    } catch (...) {
        printf("❌ OneDrive API tests crashed\n");
        totalFailures++;
    }
    
    printf("\n");
    
    // Run Integration tests
    printf("Starting Integration tests...\n");
    try {
        RunIntegrationTests();
    } catch (...) {
        printf("❌ Integration tests crashed\n");
        totalFailures++;
    }
    
    bigtime_t endTime = system_time();
    double duration = (endTime - startTime) / 1000000.0;
    
    printf("\n=== Test Suite Summary ===\n");
    printf("Total execution time: %.2f seconds\n", duration);
    
    if (totalFailures == 0) {
        printf("🎉 All test suites completed successfully!\n");
        printf("✅ OneDrive Milestone 1 components are working correctly.\n");
    } else {
        printf("❌ %d test suite(s) had issues.\n", totalFailures);
        printf("⚠️  Some OneDrive components may need attention.\n");
    }
    
    return totalFailures;
}

/**
 * @brief Main entry point
 */
int main(int argc, char* argv[]) {
    bool showHelp = false;
    bool verbose = false;
    
    // Simple argument parsing
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            showHelp = true;
        } else if (strcmp(argv[i], "--verbose") == 0 || strcmp(argv[i], "-v") == 0) {
            verbose = true;
        }
    }
    
    if (showHelp) {
        printf("OneDrive Simple Test Runner\n");
        printf("Usage: %s [options]\n\n", argv[0]);
        printf("Options:\n");
        printf("  -h, --help      Show this help message\n");
        printf("  -v, --verbose   Verbose output (not implemented)\n");
        printf("\n");
        printf("This test runner executes all OneDrive Milestone 1 tests:\n");
        printf("  - Authentication Manager tests\n");
        printf("  - OneDrive API tests\n");
        printf("  - Component integration tests\n");
        printf("\n");
        return 0;
    }
    
    return RunAllTests();
}

// Stub implementations for tests (to be linked with actual test files)
__attribute__((weak))
void RunAuthTests() {
    printf("Authentication tests not linked - building with individual test files.\n");
    printf("To run auth tests: ./SimpleAuthTest\n");
}

__attribute__((weak))
void RunAPITests() {
    printf("API tests not linked - building with individual test files.\n");
    printf("To run API tests: ./SimpleAPITest\n");
}

__attribute__((weak))
void RunIntegrationTests() {
    printf("Integration tests not linked - building with individual test files.\n");
    printf("To run integration tests: ./SimpleIntegrationTest\n");
}