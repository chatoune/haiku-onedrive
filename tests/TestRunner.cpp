#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Test runner for all OneDrive tests
int main(int argc, char** argv) {
	printf("OneDrive Test Runner\n");
	printf("====================\n");
	
	if (argc > 1 && strcmp(argv[1], "--help") == 0) {
		printf("Usage: TestRunner [options]\n");
		printf("Options:\n");
		printf("  --help           Show this help message\n");
		printf("  --suite <name>   Run specific test suite\n");
		printf("  --list           List all available test suites\n");
		return 0;
	}
	
	if (argc > 1 && strcmp(argv[1], "--list") == 0) {
		printf("Available test suites:\n");
		printf("  - AuthenticationServiceTest\n");
		printf("  - OneDriveDaemonTest (not implemented)\n");
		printf("  - SyncEngineTest (not implemented)\n");
		return 0;
	}
	
	// Check for specific suite
	const char* suite = NULL;
	if (argc > 2 && strcmp(argv[1], "--suite") == 0) {
		suite = argv[2];
	}
	
	// Run tests based on suite selection
	if (suite == NULL || strcmp(suite, "AuthenticationServiceTest") == 0) {
		printf("\nRunning AuthenticationServiceTest...\n");
		system("tests/api/AuthenticationServiceTest");
	}
	
	printf("\nAll tests completed.\n");
	return 0;
}