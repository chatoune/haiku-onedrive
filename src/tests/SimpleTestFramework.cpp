/**
 * @file SimpleTestFramework.cpp
 * @brief Simple testing framework for Haiku when CppUnit is not available
 * @author Haiku OneDrive Team
 * @version 1.0.0
 * @date 2025-08-14
 * 
 * A lightweight testing framework that provides basic testing functionality
 * similar to CppUnit but without external dependencies.
 */

#include "SimpleTestFramework.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <OS.h>

// Global test registry
static SimpleTestRegistry* g_testRegistry = nullptr;

SimpleTestCase::SimpleTestCase(const char* name)
    : fTestName(name),
      fAssertCount(0),
      fFailCount(0)
{
}

SimpleTestCase::~SimpleTestCase()
{
}

void SimpleTestCase::setUp()
{
    // Default implementation - can be overridden
}

void SimpleTestCase::tearDown()
{
    // Default implementation - can be overridden
}

void SimpleTestCase::assertTrue(bool condition, const char* message, 
                               const char* file, int line)
{
    fAssertCount++;
    if (!condition) {
        fFailCount++;
        printf("ASSERTION FAILED: %s:%d - %s\n", file, line, message);
    }
}

void SimpleTestCase::assertEqual(int expected, int actual, const char* message,
                                const char* file, int line)
{
    fAssertCount++;
    if (expected != actual) {
        fFailCount++;
        printf("ASSERTION FAILED: %s:%d - %s (expected: %d, actual: %d)\n", 
               file, line, message, expected, actual);
    }
}

void SimpleTestCase::assertEqual(const char* expected, const char* actual, 
                                const char* message, const char* file, int line)
{
    fAssertCount++;
    if (strcmp(expected, actual) != 0) {
        fFailCount++;
        printf("ASSERTION FAILED: %s:%d - %s (expected: '%s', actual: '%s')\n", 
               file, line, message, expected, actual);
    }
}

void SimpleTestCase::assertNotNull(void* pointer, const char* message,
                                  const char* file, int line)
{
    fAssertCount++;
    if (pointer == nullptr) {
        fFailCount++;
        printf("ASSERTION FAILED: %s:%d - %s (pointer is null)\n", 
               file, line, message);
    }
}

void SimpleTestCase::fail(const char* message, const char* file, int line)
{
    fAssertCount++;
    fFailCount++;
    printf("TEST FAILED: %s:%d - %s\n", file, line, message);
}

int SimpleTestCase::getAssertCount() const
{
    return fAssertCount;
}

int SimpleTestCase::getFailCount() const
{
    return fFailCount;
}

const char* SimpleTestCase::getName() const
{
    return fTestName.String();
}

SimpleTestSuite::SimpleTestSuite(const char* name)
    : fSuiteName(name)
{
}

SimpleTestSuite::~SimpleTestSuite()
{
    // Clean up test cases
    for (int32 i = 0; i < fTestCases.CountItems(); i++) {
        delete static_cast<SimpleTestCase*>(fTestCases.ItemAt(i));
    }
}

void SimpleTestSuite::addTest(SimpleTestCase* testCase)
{
    if (testCase) {
        fTestCases.AddItem(testCase);
    }
}

int SimpleTestSuite::run(bool verbose)
{
    int totalTests = fTestCases.CountItems();
    int totalAsserts = 0;
    int totalFailures = 0;
    bigtime_t startTime = system_time();
    
    if (verbose) {
        printf("Running test suite: %s (%d tests)\n", fSuiteName.String(), totalTests);
        printf("----------------------------------------\n");
    }
    
    for (int32 i = 0; i < fTestCases.CountItems(); i++) {
        SimpleTestCase* testCase = static_cast<SimpleTestCase*>(fTestCases.ItemAt(i));
        
        if (verbose) {
            printf("Running %s...", testCase->getName());
            fflush(stdout);
        }
        
        // Reset test case state
        testCase->fAssertCount = 0;
        testCase->fFailCount = 0;
        
        // Run test
        bigtime_t testStart = system_time();
        testCase->setUp();
        testCase->runTest();
        testCase->tearDown();
        bigtime_t testEnd = system_time();
        
        // Update totals
        totalAsserts += testCase->getAssertCount();
        totalFailures += testCase->getFailCount();
        
        if (verbose) {
            if (testCase->getFailCount() == 0) {
                printf(" PASSED (%d assertions, %.2fms)\n", 
                       testCase->getAssertCount(),
                       (testEnd - testStart) / 1000.0);
            } else {
                printf(" FAILED (%d/%d assertions failed, %.2fms)\n",
                       testCase->getFailCount(),
                       testCase->getAssertCount(),
                       (testEnd - testStart) / 1000.0);
            }
        }
    }
    
    bigtime_t endTime = system_time();
    
    if (verbose) {
        printf("----------------------------------------\n");
        printf("Test suite completed in %.2fms\n", (endTime - startTime) / 1000.0);
        printf("Tests: %d, Assertions: %d, Failures: %d\n", 
               totalTests, totalAsserts, totalFailures);
        
        if (totalFailures == 0) {
            printf("✅ All tests passed!\n");
        } else {
            printf("❌ %d test(s) failed.\n", totalFailures);
        }
        printf("\n");
    }
    
    return totalFailures;
}

const char* SimpleTestSuite::getName() const
{
    return fSuiteName.String();
}

SimpleTestRegistry::SimpleTestRegistry()
{
}

SimpleTestRegistry::~SimpleTestRegistry()
{
    // Clean up test suites
    for (int32 i = 0; i < fTestSuites.CountItems(); i++) {
        delete static_cast<SimpleTestSuite*>(fTestSuites.ItemAt(i));
    }
}

SimpleTestRegistry* SimpleTestRegistry::getInstance()
{
    if (g_testRegistry == nullptr) {
        g_testRegistry = new SimpleTestRegistry();
    }
    return g_testRegistry;
}

void SimpleTestRegistry::addTestSuite(SimpleTestSuite* suite)
{
    if (suite) {
        fTestSuites.AddItem(suite);
    }
}

int SimpleTestRegistry::runAllTests(bool verbose)
{
    int totalFailures = 0;
    
    if (verbose) {
        printf("OneDrive Simple Test Framework\n");
        printf("==============================\n\n");
    }
    
    for (int32 i = 0; i < fTestSuites.CountItems(); i++) {
        SimpleTestSuite* suite = static_cast<SimpleTestSuite*>(fTestSuites.ItemAt(i));
        totalFailures += suite->run(verbose);
    }
    
    return totalFailures;
}

SimpleTestRunner::SimpleTestRunner()
{
}

SimpleTestRunner::~SimpleTestRunner()
{
}

void SimpleTestRunner::addTestSuite(SimpleTestSuite* suite)
{
    SimpleTestRegistry::getInstance()->addTestSuite(suite);
}

int SimpleTestRunner::run(bool verbose)
{
    return SimpleTestRegistry::getInstance()->runAllTests(verbose);
}