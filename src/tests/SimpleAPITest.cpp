/**
 * @file SimpleAPITest.cpp
 * @brief Simple unit tests for OneDriveAPI class
 * @author Haiku OneDrive Team
 * @version 1.0.0
 * @date 2025-08-14
 * 
 * Basic unit tests for the OneDriveAPI class using the Simple Test Framework.
 */

#include "SimpleTestFramework.h"
#include <String.h>
#include <Message.h>
#include <stdio.h>

#include "../api/OneDriveAPI.h"
#include "../api/AuthManager.h"

/**
 * @brief Test OneDriveAPI construction
 */
class APIConstructionTest : public SimpleTestCase {
public:
    APIConstructionTest() : SimpleTestCase("APIConstructionTest") {}
    
    void runTest() override {
        printf("Testing OneDriveAPI construction...\n");
        
        // Test with null auth manager
        OneDriveAPI* nullAPI = new OneDriveAPI(nullptr);
        SIMPLE_ASSERT_NOT_NULL(nullAPI);
        delete nullAPI;
        
        // Test with valid auth manager
        AuthenticationManager* authManager = new AuthenticationManager();
        OneDriveAPI* validAPI = new OneDriveAPI(authManager);
        SIMPLE_ASSERT_NOT_NULL(validAPI);
        
        delete validAPI;
        delete authManager;
        
        printf("✓ OneDriveAPI construction test passed\n");
    }
};

/**
 * @brief Test user profile retrieval
 */
class APIUserProfileTest : public SimpleTestCase {
public:
    APIUserProfileTest() : SimpleTestCase("APIUserProfileTest") {}
    
    void runTest() override {
        printf("Testing user profile retrieval...\n");
        
        AuthenticationManager authManager;
        authManager.SetClientId("test-client-id");
        authManager.HandleAuthCallback("test-auth-code"); // Authenticate
        
        OneDriveAPI api(&authManager);
        
        BMessage userProfile;
        status_t result = api.GetUserProfile(userProfile);
        
        // Should succeed in development mode
        SIMPLE_ASSERT_EQUAL((int)B_OK, (int)result);
        
        printf("✓ User profile test passed\n");
    }
};

/**
 * @brief Test drive info retrieval
 */
class APIDriveInfoTest : public SimpleTestCase {
public:
    APIDriveInfoTest() : SimpleTestCase("APIDriveInfoTest") {}
    
    void runTest() override {
        printf("Testing drive info retrieval...\n");
        
        AuthenticationManager authManager;
        authManager.SetClientId("test-client-id");
        authManager.HandleAuthCallback("test-auth-code"); // Authenticate
        
        OneDriveAPI api(&authManager);
        
        BMessage driveInfo;
        status_t result = api.GetDriveInfo(driveInfo);
        
        // Should succeed in development mode
        SIMPLE_ASSERT_EQUAL((int)B_OK, (int)result);
        
        printf("✓ Drive info test passed\n");
    }
};

/**
 * @brief Test folder listing
 */
class APIListFolderTest : public SimpleTestCase {
public:
    APIListFolderTest() : SimpleTestCase("APIListFolderTest") {}
    
    void runTest() override {
        printf("Testing folder listing...\n");
        
        AuthenticationManager authManager;
        authManager.SetClientId("test-client-id");
        authManager.HandleAuthCallback("test-auth-code"); // Authenticate
        
        OneDriveAPI api(&authManager);
        
        BList items;
        status_t result = api.ListFolder("root", items);
        
        // Should succeed in development mode
        SIMPLE_ASSERT_EQUAL((int)B_OK, (int)result);
        
        // Clean up items
        for (int32 i = 0; i < items.CountItems(); i++) {
            delete static_cast<OneDriveItem*>(items.ItemAt(i));
        }
        
        printf("✓ Folder listing test passed\n");
    }
};

/**
 * @brief Create and run API test suite
 */
void RunAPITests() {
    printf("\n=== OneDrive API Tests ===\n");
    
    SimpleTestSuite apiSuite("OneDriveAPITests");
    
    apiSuite.addTest(new APIConstructionTest());
    apiSuite.addTest(new APIUserProfileTest());
    apiSuite.addTest(new APIDriveInfoTest());
    apiSuite.addTest(new APIListFolderTest());
    
    int failures = apiSuite.run(true);
    
    if (failures == 0) {
        printf("🎉 All OneDrive API tests passed!\n");
    } else {
        printf("❌ %d OneDrive API test(s) failed.\n", failures);
    }
}

/**
 * @brief Main function for standalone test execution
 */
int main() {
    printf("OneDrive API Test Suite\n");
    printf("======================\n");
    
    RunAPITests();
    
    return 0;
}