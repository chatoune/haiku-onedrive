#!/bin/bash

# OneDrive Test Runner Script
# Automated test execution script for the Haiku OneDrive project

set -e  # Exit on any error

# Script configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"
TEST_DIR="$BUILD_DIR/src/tests"
LOG_DIR="$PROJECT_ROOT/test_logs"
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Default options
RUN_UNIT_TESTS=false
RUN_INTEGRATION_TESTS=false
RUN_ALL_TESTS=true
VERBOSE=false
GENERATE_COVERAGE=false
MEMORY_CHECK=false
XML_OUTPUT=false
PARALLEL=false
STOP_ON_FAILURE=false

# Print usage information
usage() {
    echo "OneDrive Test Runner Script"
    echo "Usage: $0 [options]"
    echo ""
    echo "Options:"
    echo "  -u, --unit              Run unit tests only"
    echo "  -i, --integration       Run integration tests only"
    echo "  -a, --all               Run all tests (default)"
    echo "  -v, --verbose           Verbose output"
    echo "  -c, --coverage          Generate code coverage report"
    echo "  -m, --memory            Run with memory checking (valgrind)"
    echo "  -x, --xml               Generate XML test reports"
    echo "  -p, --parallel          Run tests in parallel"
    echo "  -s, --stop-on-failure   Stop on first test failure"
    echo "  -h, --help              Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0                      # Run all tests"
    echo "  $0 -u -v               # Run unit tests with verbose output"
    echo "  $0 -i -c               # Run integration tests with coverage"
    echo "  $0 -a -x -p            # Run all tests, generate XML, parallel"
}

# Parse command line arguments
parse_args() {
    while [[ $# -gt 0 ]]; do
        case $1 in
            -u|--unit)
                RUN_UNIT_TESTS=true
                RUN_ALL_TESTS=false
                shift
                ;;
            -i|--integration)
                RUN_INTEGRATION_TESTS=true
                RUN_ALL_TESTS=false
                shift
                ;;
            -a|--all)
                RUN_ALL_TESTS=true
                shift
                ;;
            -v|--verbose)
                VERBOSE=true
                shift
                ;;
            -c|--coverage)
                GENERATE_COVERAGE=true
                shift
                ;;
            -m|--memory)
                MEMORY_CHECK=true
                shift
                ;;
            -x|--xml)
                XML_OUTPUT=true
                shift
                ;;
            -p|--parallel)
                PARALLEL=true
                shift
                ;;
            -s|--stop-on-failure)
                STOP_ON_FAILURE=true
                shift
                ;;
            -h|--help)
                usage
                exit 0
                ;;
            *)
                echo "Unknown option: $1"
                usage
                exit 1
                ;;
        esac
    done
}

# Print colored output
print_status() {
    local color=$1
    local message=$2
    echo -e "${color}${message}${NC}"
}

# Check prerequisites
check_prerequisites() {
    print_status "$BLUE" "Checking prerequisites..."
    
    # Check if we're on Haiku
    if [[ "$(uname)" != "Haiku" ]]; then
        print_status "$YELLOW" "Warning: Not running on Haiku OS"
    fi
    
    # Check if build directory exists
    if [[ ! -d "$BUILD_DIR" ]]; then
        print_status "$RED" "Error: Build directory not found: $BUILD_DIR"
        print_status "$YELLOW" "Please run 'cmake .. && make' first"
        exit 1
    fi
    
    # Check if test executables exist
    local test_executables=(
        "$TEST_DIR/auth_tests"
        "$TEST_DIR/api_tests"
        "$TEST_DIR/daemon_tests"
        "$TEST_DIR/integration_tests"
        "$TEST_DIR/onedrive_tests"
    )
    
    for executable in "${test_executables[@]}"; do
        if [[ ! -x "$executable" ]]; then
            print_status "$RED" "Error: Test executable not found or not executable: $executable"
            print_status "$YELLOW" "Please build the project with tests enabled"
            exit 1
        fi
    done
    
    # Check for optional tools
    if $GENERATE_COVERAGE && ! command -v gcov &> /dev/null; then
        print_status "$YELLOW" "Warning: gcov not found, coverage disabled"
        GENERATE_COVERAGE=false
    fi
    
    if $MEMORY_CHECK && ! command -v valgrind &> /dev/null; then
        print_status "$YELLOW" "Warning: valgrind not found, memory checking disabled"
        MEMORY_CHECK=false
    fi
    
    print_status "$GREEN" "Prerequisites check passed"
}

# Set up test environment
setup_environment() {
    print_status "$BLUE" "Setting up test environment..."
    
    # Create log directory
    mkdir -p "$LOG_DIR"
    
    # Create test output directory
    mkdir -p "$TEST_DIR/output"
    
    # Set test environment variables
    export ONEDRIVE_TEST_MODE=1
    export ONEDRIVE_LOG_LEVEL=DEBUG
    export ONEDRIVE_TEST_CLIENT_ID="test-client-id"
    
    # Clean up any existing test files
    rm -rf /tmp/onedrive_test_*
    
    print_status "$GREEN" "Test environment ready"
}

# Run a single test executable
run_test_executable() {
    local test_name=$1
    local executable=$2
    local log_file="$LOG_DIR/${test_name}_${TIMESTAMP}.log"
    local xml_file=""
    
    if $XML_OUTPUT; then
        xml_file="$TEST_DIR/output/${test_name}_${TIMESTAMP}.xml"
    fi
    
    print_status "$BLUE" "Running $test_name..."
    
    # Build command
    local cmd="$executable"
    
    if $VERBOSE; then
        cmd="$cmd --verbose"
    fi
    
    if $XML_OUTPUT && [[ -n "$xml_file" ]]; then
        cmd="$cmd --xml --output $xml_file"
    fi
    
    if $STOP_ON_FAILURE; then
        cmd="$cmd --stop-on-failure"
    fi
    
    # Add memory checking if requested
    if $MEMORY_CHECK; then
        cmd="valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all $cmd"
    fi
    
    # Run the test
    local start_time=$(date +%s)
    if $VERBOSE; then
        # Show output in real-time
        if eval "$cmd" 2>&1 | tee "$log_file"; then
            local result="PASSED"
            print_status "$GREEN" "$test_name: $result"
        else
            local result="FAILED"
            print_status "$RED" "$test_name: $result"
            return 1
        fi
    else
        # Capture output to log file
        if eval "$cmd" > "$log_file" 2>&1; then
            local result="PASSED"
            print_status "$GREEN" "$test_name: $result"
        else
            local result="FAILED"
            print_status "$RED" "$test_name: $result"
            if $STOP_ON_FAILURE; then
                print_status "$RED" "Stopping on failure as requested"
                cat "$log_file"
                return 1
            fi
            return 1
        fi
    fi
    
    local end_time=$(date +%s)
    local duration=$((end_time - start_time))
    
    if $VERBOSE; then
        print_status "$BLUE" "$test_name completed in ${duration}s"
    fi
    
    return 0
}

# Run unit tests
run_unit_tests() {
    print_status "$BLUE" "=== Running Unit Tests ==="
    
    local failed=0
    
    # AuthManager tests
    if ! run_test_executable "AuthManager" "$TEST_DIR/auth_tests"; then
        ((failed++))
    fi
    
    # OneDriveAPI tests
    if ! run_test_executable "OneDriveAPI" "$TEST_DIR/api_tests"; then
        ((failed++))
    fi
    
    # OneDriveDaemon tests
    if ! run_test_executable "OneDriveDaemon" "$TEST_DIR/daemon_tests"; then
        ((failed++))
    fi
    
    return $failed
}

# Run integration tests
run_integration_tests() {
    print_status "$BLUE" "=== Running Integration Tests ==="
    
    local failed=0
    
    # Integration tests
    if ! run_test_executable "Integration" "$TEST_DIR/integration_tests"; then
        ((failed++))
    fi
    
    return $failed
}

# Run all tests using the comprehensive test runner
run_all_tests() {
    print_status "$BLUE" "=== Running All Tests ==="
    
    if run_test_executable "AllTests" "$TEST_DIR/onedrive_tests"; then
        return 0
    else
        return 1
    fi
}

# Generate coverage report
generate_coverage() {
    if ! $GENERATE_COVERAGE; then
        return 0
    fi
    
    print_status "$BLUE" "Generating coverage report..."
    
    cd "$BUILD_DIR"
    
    # Find and process coverage files
    find . -name "*.gcda" -exec gcov {} \;
    
    # Generate HTML report if lcov is available
    if command -v lcov &> /dev/null && command -v genhtml &> /dev/null; then
        lcov --capture --directory . --output-file coverage.info
        lcov --remove coverage.info '/usr/*' '*/tests/*' --output-file coverage.info
        genhtml coverage.info --output-directory coverage_html
        
        print_status "$GREEN" "Coverage report generated: $BUILD_DIR/coverage_html/index.html"
    else
        print_status "$YELLOW" "lcov/genhtml not available, basic coverage files generated"
    fi
}

# Clean up test environment
cleanup() {
    print_status "$BLUE" "Cleaning up test environment..."
    
    # Clean up temporary test files
    rm -rf /tmp/onedrive_test_*
    
    # Stop any test daemons that might be running
    pkill -f onedrive_daemon || true
    
    print_status "$GREEN" "Cleanup completed"
}

# Main test execution function
run_tests() {
    local total_failed=0
    
    if $RUN_ALL_TESTS; then
        if ! run_all_tests; then
            ((total_failed++))
        fi
    else
        if $RUN_UNIT_TESTS; then
            local unit_failed
            unit_failed=$(run_unit_tests) || true
            ((total_failed += unit_failed))
        fi
        
        if $RUN_INTEGRATION_TESTS; then
            local integration_failed
            integration_failed=$(run_integration_tests) || true
            ((total_failed += integration_failed))
        fi
    fi
    
    return $total_failed
}

# Print test summary
print_summary() {
    local failed_count=$1
    local start_time=$2
    local end_time=$3
    local duration=$((end_time - start_time))
    
    echo ""
    print_status "$BLUE" "=== Test Summary ==="
    
    if [[ $failed_count -eq 0 ]]; then
        print_status "$GREEN" "✅ All tests passed!"
    else
        print_status "$RED" "❌ $failed_count test suite(s) failed"
    fi
    
    print_status "$BLUE" "Total execution time: ${duration}s"
    print_status "$BLUE" "Log files saved to: $LOG_DIR"
    
    if $XML_OUTPUT; then
        print_status "$BLUE" "XML reports saved to: $TEST_DIR/output"
    fi
    
    if $GENERATE_COVERAGE; then
        print_status "$BLUE" "Coverage report available in: $BUILD_DIR/coverage_html"
    fi
}

# Main script execution
main() {
    local start_time=$(date +%s)
    
    # Parse arguments
    parse_args "$@"
    
    # Print header
    echo ""
    print_status "$BLUE" "OneDrive Test Runner"
    print_status "$BLUE" "===================="
    echo ""
    
    # Set up signal handlers for cleanup
    trap cleanup EXIT
    
    # Run checks and setup
    check_prerequisites
    setup_environment
    
    # Run tests
    local failed_count
    failed_count=$(run_tests) || true
    
    # Generate coverage if requested
    generate_coverage
    
    # Print summary
    local end_time=$(date +%s)
    print_summary $failed_count $start_time $end_time
    
    # Exit with appropriate code
    if [[ $failed_count -eq 0 ]]; then
        exit 0
    else
        exit 1
    fi
}

# Execute main function
main "$@"