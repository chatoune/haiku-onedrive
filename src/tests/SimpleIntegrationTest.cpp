/**
 * @file SimpleIntegrationTest.cpp
 * @brief Simple integration tests for OneDrive components
 * @author Haiku OneDrive Team
 * @version 1.0.0
 * @date 2025-08-14
 * 
 * Basic integration tests using the Simple Test Framework.
 */

#include "SimpleTestFramework.h"
#include <String.h>
#include <Message.h>
#include <stdio.h>

#include "../api/AuthManager.h"
#include "../api/OneDriveAPI.h"

/**
 * @brief Test AuthenticationManager and OneDriveAPI integration
 */
class AuthAPIIntegrationTest : public SimpleTestCase {
public:
    AuthAPIIntegrationTest() : SimpleTestCase("AuthAPIIntegrationTest") {}
    
    void runTest() override {
        printf("Testing Auth-API integration...\n");
        
        // Create authentication manager
        AuthenticationManager authManager;
        authManager.SetClientId("test-client-id");
        
        // Create API with auth manager
        OneDriveAPI api(&authManager);
        
        // Initially not authenticated
        SIMPLE_ASSERT_EQUAL(false, authManager.IsAuthenticated());
        
        // Simulate authentication
        status_t authResult = authManager.HandleAuthCallback("test-auth-code");
        SIMPLE_ASSERT_EQUAL((int)B_OK, (int)authResult);
        SIMPLE_ASSERT_EQUAL(true, authManager.IsAuthenticated());
        
        // API should work with authentication
        BMessage userProfile;
        status_t apiResult = api.GetUserProfile(userProfile);
        SIMPLE_ASSERT_EQUAL((int)B_OK, (int)apiResult);
        
        printf("✓ Auth-API integration test passed\n");
    }
};

/**
 * @brief Test full authentication workflow
 */
class FullAuthWorkflowTest : public SimpleTestCase {
public:
    FullAuthWorkflowTest() : SimpleTestCase("FullAuthWorkflowTest") {}
    
    void runTest() override {
        printf("Testing full authentication workflow...\n");
        
        AuthenticationManager authManager;
        authManager.SetClientId("test-client-id");
        
        // 1. Get authorization URL
        BString authUrl;
        status_t urlResult = authManager.GetAuthorizationURL(authUrl);
        SIMPLE_ASSERT_EQUAL((int)B_OK, (int)urlResult);
        SIMPLE_ASSERT(authUrl.Length() > 0);
        
        // 2. Handle callback
        status_t callbackResult = authManager.HandleAuthCallback("test-auth-code");
        SIMPLE_ASSERT_EQUAL((int)B_OK, (int)callbackResult);
        
        // 3. Verify authentication state
        SIMPLE_ASSERT_EQUAL(true, authManager.IsAuthenticated());
        SIMPLE_ASSERT_EQUAL((int)AUTH_STATE_AUTHENTICATED, (int)authManager.GetAuthState());
        
        // 4. Get access token
        BString accessToken;
        status_t tokenResult = authManager.GetAccessToken(accessToken);
        SIMPLE_ASSERT_EQUAL((int)B_OK, (int)tokenResult);
        SIMPLE_ASSERT(accessToken.Length() > 0);
        
        printf("✓ Full authentication workflow test passed\n");
    }
};

/**
 * @brief Test error propagation between components
 */
class ErrorPropagationTest : public SimpleTestCase {
public:
    ErrorPropagationTest() : SimpleTestCase("ErrorPropagationTest") {}
    
    void runTest() override {
        printf("Testing error propagation...\n");
        
        AuthenticationManager authManager;
        OneDriveAPI api(&authManager);
        
        // Test unauthenticated API access
        BMessage userProfile;
        status_t result = api.GetUserProfile(userProfile);
        
        // Should fail since not authenticated
        SIMPLE_ASSERT(result != B_OK);
        
        printf("✓ Error propagation test passed\n");
    }
};

/**
 * @brief Test component recovery
 */
class ComponentRecoveryTest : public SimpleTestCase {
public:
    ComponentRecoveryTest() : SimpleTestCase("ComponentRecoveryTest") {}
    
    void runTest() override {
        printf("Testing component recovery...\n");
        
        AuthenticationManager authManager;
        OneDriveAPI api(&authManager);
        
        authManager.SetClientId("test-client-id");
        
        // Authenticate first
        status_t authResult = authManager.HandleAuthCallback("test-auth-code");
        SIMPLE_ASSERT_EQUAL((int)B_OK, (int)authResult);
        SIMPLE_ASSERT_EQUAL(true, authManager.IsAuthenticated());
        
        // Logout
        authManager.Logout();
        SIMPLE_ASSERT_EQUAL(false, authManager.IsAuthenticated());
        
        // Re-authenticate
        authResult = authManager.HandleAuthCallback("test-auth-code");
        SIMPLE_ASSERT_EQUAL((int)B_OK, (int)authResult);
        SIMPLE_ASSERT_EQUAL(true, authManager.IsAuthenticated());
        
        // Should work again
        BMessage userProfile;
        status_t profileResult = api.GetUserProfile(userProfile);
        SIMPLE_ASSERT_EQUAL((int)B_OK, (int)profileResult);
        
        printf("✓ Component recovery test passed\n");
    }
};

/**
 * @brief Create and run integration test suite
 */
void RunIntegrationTests() {
    printf("\n=== Integration Tests ===\n");
    
    SimpleTestSuite integrationSuite("IntegrationTests");
    
    integrationSuite.addTest(new AuthAPIIntegrationTest());
    integrationSuite.addTest(new FullAuthWorkflowTest());
    integrationSuite.addTest(new ErrorPropagationTest());
    integrationSuite.addTest(new ComponentRecoveryTest());
    
    int failures = integrationSuite.run(true);
    
    if (failures == 0) {
        printf("🎉 All Integration tests passed!\n");
    } else {
        printf("❌ %d Integration test(s) failed.\n", failures);
    }
}

/**
 * @brief Main function for standalone test execution
 */
int main() {
    printf("OneDrive Integration Test Suite\n");
    printf("==============================\n");
    
    RunIntegrationTests();
    
    return 0;
}