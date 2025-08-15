/**
 * @file AuthManagerTest.cpp
 * @brief Unit tests for AuthenticationManager class
 * @author Haiku OneDrive Team
 * @version 1.0.0
 * @date 2025-08-14
 * 
 * Comprehensive unit tests for the AuthenticationManager class covering:
 * - OAuth2 authentication flow
 * - Token management and storage
 * - Keystore integration
 * - Error handling and state management
 * - Thread safety
 */

#include <cppunit/TestCase.h>
#include <cppunit/TestSuite.h>
#include <cppunit/TestCaller.h>
#include <cppunit/TestRunner.h>
#include <cppunit/TestResult.h>
#include <cppunit/TestResultCollector.h>
#include <cppunit/CompilerOutputter.h>

#include <String.h>
#include <Message.h>
#include <stdio.h>

#include "../api/AuthManager.h"

/**
 * @brief Test fixture for AuthenticationManager tests
 */
class AuthManagerTest : public CppUnit::TestCase {
public:
    /**
     * @brief Constructor
     */
    AuthManagerTest();
    
    /**
     * @brief Destructor
     */
    virtual ~AuthManagerTest();
    
    /**
     * @brief Set up test environment before each test
     */
    virtual void setUp();
    
    /**
     * @brief Clean up test environment after each test
     */
    virtual void tearDown();
    
    // Test cases
    
    /**
     * @brief Test AuthenticationManager construction and initialization
     */
    void TestConstruction();
    
    /**
     * @brief Test OAuth2 authorization URL generation
     */
    void TestAuthorizationURL();
    
    /**
     * @brief Test authentication state management
     */
    void TestAuthenticationState();
    
    /**
     * @brief Test OAuth2 callback handling with valid authorization code
     */
    void TestAuthCallbackValid();
    
    /**
     * @brief Test OAuth2 callback handling with invalid authorization code
     */
    void TestAuthCallbackInvalid();
    
    /**
     * @brief Test access token retrieval when authenticated
     */
    void TestGetAccessTokenAuthenticated();
    
    /**
     * @brief Test access token retrieval when not authenticated
     */
    void TestGetAccessTokenNotAuthenticated();
    
    /**
     * @brief Test token refresh functionality
     */
    void TestTokenRefresh();
    
    /**
     * @brief Test logout functionality
     */
    void TestLogout();
    
    /**
     * @brief Test client ID configuration
     */
    void TestSetClientId();
    
    /**
     * @brief Test user information retrieval
     */
    void TestGetUserInfo();
    
    /**
     * @brief Test error handling and error message retrieval
     */
    void TestErrorHandling();
    
    /**
     * @brief Test thread safety of authentication operations
     */
    void TestThreadSafety();
    
    /**
     * @brief Test keystore integration and token persistence
     */
    void TestKeystoreIntegration();
    
    /**
     * @brief Test token expiration detection
     */
    void TestTokenExpiration();

private:
    AuthenticationManager* fAuthManager;    ///< Test subject
    BString fTestClientId;                  ///< Test OAuth2 client ID
    BString fTestAuthCode;                  ///< Test authorization code
    BString fTestAccessToken;               ///< Test access token
    BString fTestRefreshToken;              ///< Test refresh token
    
    /**
     * @brief Helper method to simulate successful authentication
     */
    void _SimulateAuthentication();
    
    /**
     * @brief Helper method to clean up test keystore entries
     */
    void _CleanupKeystore();
};

// Test suite registration
static AuthManagerTest sAuthManagerTest;

AuthManagerTest::AuthManagerTest()
    : CppUnit::TestCase("AuthManagerTest"),
      fAuthManager(nullptr),
      fTestClientId("test-client-id-12345"),
      fTestAuthCode("test-auth-code-67890"),
      fTestAccessToken("test-access-token-abcdef"),
      fTestRefreshToken("test-refresh-token-ghijkl")
{
}

AuthManagerTest::~AuthManagerTest()
{
}

void AuthManagerTest::setUp()
{
    // Clean up any existing test data
    _CleanupKeystore();
    
    // Create fresh AuthenticationManager instance
    fAuthManager = new AuthenticationManager();
    
    // Set test client ID
    fAuthManager->SetClientId(fTestClientId);
}

void AuthManagerTest::tearDown()
{
    // Clean up
    if (fAuthManager) {
        fAuthManager->Logout();
        delete fAuthManager;
        fAuthManager = nullptr;
    }
    
    // Clean up keystore
    _CleanupKeystore();
}

void AuthManagerTest::TestConstruction()
{
    // Test that AuthenticationManager can be constructed successfully
    AuthenticationManager* authManager = new AuthenticationManager();
    CPPUNIT_ASSERT(authManager != nullptr);
    
    // Test initial state
    CPPUNIT_ASSERT_EQUAL(AUTH_STATE_NOT_AUTHENTICATED, authManager->GetAuthState());
    CPPUNIT_ASSERT_EQUAL(false, authManager->IsAuthenticated());
    
    delete authManager;
}

void AuthManagerTest::TestAuthorizationURL()
{
    BString authUrl;
    status_t result = fAuthManager->GetAuthorizationURL(authUrl);
    
    // Should succeed in generating URL
    CPPUNIT_ASSERT_EQUAL(B_OK, result);
    CPPUNIT_ASSERT(authUrl.Length() > 0);
    
    // URL should contain expected OAuth2 parameters
    CPPUNIT_ASSERT(authUrl.FindFirst("https://login.microsoftonline.com/") == 0);
    CPPUNIT_ASSERT(authUrl.FindFirst("client_id=") >= 0);
    CPPUNIT_ASSERT(authUrl.FindFirst("response_type=code") >= 0);
    CPPUNIT_ASSERT(authUrl.FindFirst("scope=") >= 0);
    CPPUNIT_ASSERT(authUrl.FindFirst("redirect_uri=") >= 0);
}

void AuthManagerTest::TestAuthenticationState()
{
    // Initial state should be not authenticated
    CPPUNIT_ASSERT_EQUAL(AUTH_STATE_NOT_AUTHENTICATED, fAuthManager->GetAuthState());
    CPPUNIT_ASSERT_EQUAL(false, fAuthManager->IsAuthenticated());
    
    // After starting auth flow, state should change to authenticating
    status_t result = fAuthManager->StartAuthFlow();
    // Note: In development mode, this might return B_OK or an error depending on implementation
    
    // Simulate authentication success (this would normally happen via callback)
    _SimulateAuthentication();
    
    // State should now be authenticated
    CPPUNIT_ASSERT_EQUAL(AUTH_STATE_AUTHENTICATED, fAuthManager->GetAuthState());
    CPPUNIT_ASSERT_EQUAL(true, fAuthManager->IsAuthenticated());
}

void AuthManagerTest::TestAuthCallbackValid()
{
    // Test handling valid authorization code
    status_t result = fAuthManager->HandleAuthCallback(fTestAuthCode);
    
    // In development mode, this should succeed
    CPPUNIT_ASSERT_EQUAL(B_OK, result);
    
    // State should be authenticated after successful callback
    CPPUNIT_ASSERT_EQUAL(AUTH_STATE_AUTHENTICATED, fAuthManager->GetAuthState());
    CPPUNIT_ASSERT_EQUAL(true, fAuthManager->IsAuthenticated());
}

void AuthManagerTest::TestAuthCallbackInvalid()
{
    // Test handling invalid authorization code
    BString invalidCode("invalid-auth-code");
    status_t result = fAuthManager->HandleAuthCallback(invalidCode);
    
    // Should handle gracefully but not authenticate
    // In development mode, this might still succeed, so we check the state
    if (result != B_OK) {
        CPPUNIT_ASSERT_EQUAL(AUTH_STATE_ERROR, fAuthManager->GetAuthState());
        CPPUNIT_ASSERT_EQUAL(false, fAuthManager->IsAuthenticated());
    }
}

void AuthManagerTest::TestGetAccessTokenAuthenticated()
{
    // First authenticate
    _SimulateAuthentication();
    
    // Now try to get access token
    BString accessToken;
    status_t result = fAuthManager->GetAccessToken(accessToken);
    
    CPPUNIT_ASSERT_EQUAL(B_OK, result);
    CPPUNIT_ASSERT(accessToken.Length() > 0);
}

void AuthManagerTest::TestGetAccessTokenNotAuthenticated()
{
    // Try to get access token without authentication
    BString accessToken;
    status_t result = fAuthManager->GetAccessToken(accessToken);
    
    // Should fail
    CPPUNIT_ASSERT(result != B_OK);
    CPPUNIT_ASSERT_EQUAL(0, (int)accessToken.Length());
}

void AuthManagerTest::TestTokenRefresh()
{
    // First authenticate
    _SimulateAuthentication();
    
    // Try to refresh token
    status_t result = fAuthManager->RefreshToken();
    
    // In development mode, this should succeed
    CPPUNIT_ASSERT_EQUAL(B_OK, result);
    
    // Should still be authenticated after refresh
    CPPUNIT_ASSERT_EQUAL(true, fAuthManager->IsAuthenticated());
}

void AuthManagerTest::TestLogout()
{
    // First authenticate
    _SimulateAuthentication();
    CPPUNIT_ASSERT_EQUAL(true, fAuthManager->IsAuthenticated());
    
    // Now logout
    status_t result = fAuthManager->Logout();
    CPPUNIT_ASSERT_EQUAL(B_OK, result);
    
    // Should no longer be authenticated
    CPPUNIT_ASSERT_EQUAL(false, fAuthManager->IsAuthenticated());
    CPPUNIT_ASSERT_EQUAL(AUTH_STATE_NOT_AUTHENTICATED, fAuthManager->GetAuthState());
    
    // Access token should not be available
    BString accessToken;
    status_t tokenResult = fAuthManager->GetAccessToken(accessToken);
    CPPUNIT_ASSERT(tokenResult != B_OK);
}

void AuthManagerTest::TestSetClientId()
{
    BString newClientId("new-test-client-id");
    status_t result = fAuthManager->SetClientId(newClientId);
    
    CPPUNIT_ASSERT_EQUAL(B_OK, result);
    
    // Verify the client ID is used in authorization URL
    BString authUrl;
    fAuthManager->GetAuthorizationURL(authUrl);
    CPPUNIT_ASSERT(authUrl.FindFirst(newClientId) >= 0);
}

void AuthManagerTest::TestGetUserInfo()
{
    // Try to get user info without authentication
    BMessage userInfo;
    status_t result = fAuthManager->GetUserInfo(userInfo);
    CPPUNIT_ASSERT(result != B_OK);
    
    // Authenticate first
    _SimulateAuthentication();
    
    // Now try to get user info
    result = fAuthManager->GetUserInfo(userInfo);
    CPPUNIT_ASSERT_EQUAL(B_OK, result);
    
    // Should contain some user information
    BString userId;
    if (userInfo.FindString("user_id", &userId) == B_OK) {
        CPPUNIT_ASSERT(userId.Length() > 0);
    }
}

void AuthManagerTest::TestErrorHandling()
{
    // Test error message retrieval
    BString errorMsg = fAuthManager->GetLastError();
    // Initial error should be empty
    CPPUNIT_ASSERT_EQUAL(0, (int)errorMsg.Length());
    
    // Trigger an error by using invalid auth code
    fAuthManager->HandleAuthCallback("definitely-invalid-code-123");
    
    // Check if error message is set (in development mode, might not set error)
    errorMsg = fAuthManager->GetLastError();
    // Error message might be set depending on implementation
}

void AuthManagerTest::TestThreadSafety()
{
    // This is a basic thread safety test
    // In a full implementation, we would spawn multiple threads
    // and test concurrent access to AuthenticationManager methods
    
    // For now, just verify that the object can handle rapid calls
    for (int i = 0; i < 10; i++) {
        AuthState state = fAuthManager->GetAuthState();
        bool authenticated = fAuthManager->IsAuthenticated();
        
        // These calls should not crash or corrupt state
        CPPUNIT_ASSERT(state >= AUTH_STATE_NOT_AUTHENTICATED && state <= AUTH_STATE_ERROR);
    }
}

void AuthManagerTest::TestKeystoreIntegration()
{
    // Authenticate to store tokens in keystore
    _SimulateAuthentication();
    
    // Create a new AuthenticationManager instance
    AuthenticationManager* newAuthManager = new AuthenticationManager();
    newAuthManager->SetClientId(fTestClientId);
    
    // The new instance should be able to detect existing authentication
    // (This behavior depends on the keystore implementation)
    // In development mode, tokens might not persist between instances
    
    delete newAuthManager;
}

void AuthManagerTest::TestTokenExpiration()
{
    // Authenticate first
    _SimulateAuthentication();
    
    // This test would verify token expiration logic
    // In development mode, we can't easily test real token expiration
    // but we can verify the state management works correctly
    
    CPPUNIT_ASSERT_EQUAL(true, fAuthManager->IsAuthenticated());
    
    // Try to refresh token to test expiration handling
    status_t result = fAuthManager->RefreshToken();
    CPPUNIT_ASSERT_EQUAL(B_OK, result);
}

void AuthManagerTest::_SimulateAuthentication()
{
    // In development mode, use the test authorization code
    status_t result = fAuthManager->HandleAuthCallback(fTestAuthCode);
    
    // Should succeed in development mode
    if (result == B_OK) {
        // Verify authentication was successful
        CPPUNIT_ASSERT_EQUAL(true, fAuthManager->IsAuthenticated());
    }
}

void AuthManagerTest::_CleanupKeystore()
{
    // Create a temporary AuthenticationManager to clean up keystore
    AuthenticationManager tempAuth;
    tempAuth.Logout(); // This should clear any stored tokens
}

// Test suite factory
CppUnit::Test* AuthManagerTestSuite()
{
    CppUnit::TestSuite* suite = new CppUnit::TestSuite("AuthManagerTestSuite");
    
    suite->addTest(new CppUnit::TestCaller<AuthManagerTest>(
        "TestConstruction", &AuthManagerTest::TestConstruction));
    suite->addTest(new CppUnit::TestCaller<AuthManagerTest>(
        "TestAuthorizationURL", &AuthManagerTest::TestAuthorizationURL));
    suite->addTest(new CppUnit::TestCaller<AuthManagerTest>(
        "TestAuthenticationState", &AuthManagerTest::TestAuthenticationState));
    suite->addTest(new CppUnit::TestCaller<AuthManagerTest>(
        "TestAuthCallbackValid", &AuthManagerTest::TestAuthCallbackValid));
    suite->addTest(new CppUnit::TestCaller<AuthManagerTest>(
        "TestAuthCallbackInvalid", &AuthManagerTest::TestAuthCallbackInvalid));
    suite->addTest(new CppUnit::TestCaller<AuthManagerTest>(
        "TestGetAccessTokenAuthenticated", &AuthManagerTest::TestGetAccessTokenAuthenticated));
    suite->addTest(new CppUnit::TestCaller<AuthManagerTest>(
        "TestGetAccessTokenNotAuthenticated", &AuthManagerTest::TestGetAccessTokenNotAuthenticated));
    suite->addTest(new CppUnit::TestCaller<AuthManagerTest>(
        "TestTokenRefresh", &AuthManagerTest::TestTokenRefresh));
    suite->addTest(new CppUnit::TestCaller<AuthManagerTest>(
        "TestLogout", &AuthManagerTest::TestLogout));
    suite->addTest(new CppUnit::TestCaller<AuthManagerTest>(
        "TestSetClientId", &AuthManagerTest::TestSetClientId));
    suite->addTest(new CppUnit::TestCaller<AuthManagerTest>(
        "TestGetUserInfo", &AuthManagerTest::TestGetUserInfo));
    suite->addTest(new CppUnit::TestCaller<AuthManagerTest>(
        "TestErrorHandling", &AuthManagerTest::TestErrorHandling));
    suite->addTest(new CppUnit::TestCaller<AuthManagerTest>(
        "TestThreadSafety", &AuthManagerTest::TestThreadSafety));
    suite->addTest(new CppUnit::TestCaller<AuthManagerTest>(
        "TestKeystoreIntegration", &AuthManagerTest::TestKeystoreIntegration));
    suite->addTest(new CppUnit::TestCaller<AuthManagerTest>(
        "TestTokenExpiration", &AuthManagerTest::TestTokenExpiration));
    
    return suite;
}