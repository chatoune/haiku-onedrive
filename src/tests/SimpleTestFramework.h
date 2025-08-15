/**
 * @file SimpleTestFramework.h
 * @brief Simple testing framework for Haiku when CppUnit is not available
 * @author Haiku OneDrive Team
 * @version 1.0.0
 * @date 2025-08-14
 * 
 * A lightweight testing framework that provides basic testing functionality
 * similar to CppUnit but without external dependencies. Used as a fallback
 * when CppUnit is not available on the system.
 */

#ifndef SIMPLE_TEST_FRAMEWORK_H
#define SIMPLE_TEST_FRAMEWORK_H

#include <String.h>
#include <List.h>

// Test assertion macros
#define SIMPLE_ASSERT(condition) \
    assertTrue((condition), #condition, __FILE__, __LINE__)

#define SIMPLE_ASSERT_EQUAL(expected, actual) \
    assertEqual((expected), (actual), #expected " == " #actual, __FILE__, __LINE__)

#define SIMPLE_ASSERT_NOT_NULL(pointer) \
    assertNotNull((pointer), #pointer " != NULL", __FILE__, __LINE__)

#define SIMPLE_FAIL(message) \
    fail((message), __FILE__, __LINE__)

/**
 * @brief Base class for individual test cases
 * 
 * Similar to CppUnit::TestCase, provides basic testing functionality
 * including assertions and test lifecycle management.
 */
class SimpleTestCase {
public:
    /**
     * @brief Constructor
     * @param name Test case name
     */
    explicit SimpleTestCase(const char* name);
    
    /**
     * @brief Destructor
     */
    virtual ~SimpleTestCase();
    
    /**
     * @brief Set up test environment (called before each test)
     */
    virtual void setUp();
    
    /**
     * @brief Clean up test environment (called after each test)
     */
    virtual void tearDown();
    
    /**
     * @brief Run the actual test (must be implemented by subclasses)
     */
    virtual void runTest() = 0;
    
    /**
     * @brief Assert that a condition is true
     * @param condition Condition to test
     * @param message Error message if assertion fails
     * @param file Source file name
     * @param line Source line number
     */
    void assertTrue(bool condition, const char* message, 
                   const char* file, int line);
    
    /**
     * @brief Assert that two integers are equal
     * @param expected Expected value
     * @param actual Actual value
     * @param message Error message if assertion fails
     * @param file Source file name
     * @param line Source line number
     */
    void assertEqual(int expected, int actual, const char* message,
                    const char* file, int line);
    
    /**
     * @brief Assert that two strings are equal
     * @param expected Expected string
     * @param actual Actual string
     * @param message Error message if assertion fails
     * @param file Source file name
     * @param line Source line number
     */
    void assertEqual(const char* expected, const char* actual, 
                    const char* message, const char* file, int line);
    
    /**
     * @brief Assert that a pointer is not null
     * @param pointer Pointer to test
     * @param message Error message if assertion fails
     * @param file Source file name
     * @param line Source line number
     */
    void assertNotNull(void* pointer, const char* message,
                      const char* file, int line);
    
    /**
     * @brief Force a test failure
     * @param message Failure message
     * @param file Source file name
     * @param line Source line number
     */
    void fail(const char* message, const char* file, int line);
    
    /**
     * @brief Get the number of assertions executed
     * @return Number of assertions
     */
    int getAssertCount() const;
    
    /**
     * @brief Get the number of failed assertions
     * @return Number of failures
     */
    int getFailCount() const;
    
    /**
     * @brief Get the test case name
     * @return Test case name
     */
    const char* getName() const;

protected:
    friend class SimpleTestSuite;
    
    BString fTestName;      ///< Test case name
    int fAssertCount;       ///< Number of assertions executed
    int fFailCount;         ///< Number of failed assertions
};

/**
 * @brief Test suite that contains multiple test cases
 * 
 * Similar to CppUnit::TestSuite, manages a collection of test cases
 * and provides methods to run them all.
 */
class SimpleTestSuite {
public:
    /**
     * @brief Constructor
     * @param name Test suite name
     */
    explicit SimpleTestSuite(const char* name);
    
    /**
     * @brief Destructor
     */
    virtual ~SimpleTestSuite();
    
    /**
     * @brief Add a test case to the suite
     * @param testCase Test case to add (takes ownership)
     */
    void addTest(SimpleTestCase* testCase);
    
    /**
     * @brief Run all test cases in the suite
     * @param verbose Enable verbose output
     * @return Number of failures
     */
    int run(bool verbose = true);
    
    /**
     * @brief Get the test suite name
     * @return Test suite name
     */
    const char* getName() const;

private:
    BString fSuiteName;     ///< Test suite name
    BList fTestCases;       ///< List of test cases
};

/**
 * @brief Global test registry
 * 
 * Manages all test suites in the application and provides
 * a way to run all tests at once.
 */
class SimpleTestRegistry {
public:
    /**
     * @brief Get the singleton instance
     * @return Global test registry instance
     */
    static SimpleTestRegistry* getInstance();
    
    /**
     * @brief Add a test suite to the registry
     * @param suite Test suite to add (takes ownership)
     */
    void addTestSuite(SimpleTestSuite* suite);
    
    /**
     * @brief Run all registered test suites
     * @param verbose Enable verbose output
     * @return Total number of failures
     */
    int runAllTests(bool verbose = true);

private:
    /**
     * @brief Constructor (private for singleton)
     */
    SimpleTestRegistry();
    
    /**
     * @brief Destructor
     */
    ~SimpleTestRegistry();
    
    BList fTestSuites;      ///< List of test suites
};

/**
 * @brief Test runner utility class
 * 
 * Provides a convenient interface for running tests, similar to
 * CppUnit::TextTestRunner.
 */
class SimpleTestRunner {
public:
    /**
     * @brief Constructor
     */
    SimpleTestRunner();
    
    /**
     * @brief Destructor
     */
    ~SimpleTestRunner();
    
    /**
     * @brief Add a test suite to run
     * @param suite Test suite to add
     */
    void addTestSuite(SimpleTestSuite* suite);
    
    /**
     * @brief Run all added test suites
     * @param verbose Enable verbose output
     * @return Number of failures
     */
    int run(bool verbose = true);
};

// Compatibility macros for CppUnit-style testing
#ifdef USE_SIMPLE_TEST_FRAMEWORK

#define CPPUNIT_ASSERT(condition) SIMPLE_ASSERT(condition)
#define CPPUNIT_ASSERT_EQUAL(expected, actual) SIMPLE_ASSERT_EQUAL(expected, actual)
#define CPPUNIT_FAIL(message) SIMPLE_FAIL(message)

// Mock CppUnit classes for compatibility
namespace CppUnit {
    typedef SimpleTestCase TestCase;
    typedef SimpleTestSuite TestSuite;
    typedef SimpleTestRunner TextTestRunner;
    
    class Test {
    public:
        virtual ~Test() {}
    };
}

#endif // USE_SIMPLE_TEST_FRAMEWORK

#endif // SIMPLE_TEST_FRAMEWORK_H