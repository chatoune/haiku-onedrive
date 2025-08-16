#ifndef ONEDRIVE_TEST_CASE_H
#define ONEDRIVE_TEST_CASE_H

#include <stdio.h>
#include <string.h>

// Simple test framework for Haiku
class TestCase {
public:
	TestCase(const char* name) : fName(name), fPassed(0), fFailed(0) {}
	virtual ~TestCase() {}
	
	// Override to set up test fixtures
	virtual void SetUp() {}
	virtual void TearDown() {}
	
	// Test runner
	void Run() {
		printf("\nRunning test suite: %s\n", fName);
		printf("================================\n");
		RunTests();
		printf("\nResults: %d passed, %d failed\n", fPassed, fFailed);
	}
	
protected:
	// Override to add test methods
	virtual void RunTests() = 0;
	
	// Assertion helpers
	void ASSERT_TRUE(bool condition, const char* message = "") {
		if (!condition) {
			printf("  FAILED: %s\n", message);
			fFailed++;
		} else {
			printf("  PASSED: %s\n", message);
			fPassed++;
		}
	}
	
	void ASSERT_FALSE(bool condition, const char* message = "") {
		ASSERT_TRUE(!condition, message);
	}
	
	void ASSERT_EQ(int expected, int actual, const char* message = "") {
		char buffer[256];
		snprintf(buffer, sizeof(buffer), "%s (expected: %d, actual: %d)", 
			message, expected, actual);
		ASSERT_TRUE(expected == actual, buffer);
	}
	
	void ASSERT_STR_EQ(const char* expected, const char* actual, const char* message = "") {
		char buffer[256];
		snprintf(buffer, sizeof(buffer), "%s (expected: '%s', actual: '%s')", 
			message, expected, actual);
		ASSERT_TRUE(strcmp(expected, actual) == 0, buffer);
	}
	
	void ASSERT_NOT_NULL(void* ptr, const char* message = "") {
		ASSERT_TRUE(ptr != NULL, message);
	}
	
	void ASSERT_NULL(void* ptr, const char* message = "") {
		ASSERT_TRUE(ptr == NULL, message);
	}
	
private:
	const char* fName;
	int fPassed;
	int fFailed;
};

#endif // ONEDRIVE_TEST_CASE_H