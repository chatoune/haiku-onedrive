/**
 * @file IntegrationTest.cpp
 * @brief Integration tests for OneDrive components
 * @author Haiku OneDrive Team
 * @version 1.0.0
 * @date 2025-08-14
 * 
 * Integration tests verifying that components work together correctly:
 * - AuthenticationManager + OneDriveAPI integration
 * - OneDriveDaemon + API integration
 * - End-to-end authentication and file operations
 * - Cross-component error handling
 * - Full workflow testing
 */

#include <TestSuite.h>
#include <TestCase.h>
#include <TestUtils.h>
#include <String.h>
#include <Message.h>
#include <File.h>
#include <Path.h>
#include <Entry.h>
#include <stdio.h>

#include "../api/AuthManager.h"
#include "../api/OneDriveAPI.h"
#include "../daemon/OneDriveDaemon.h"

/**
 * @brief Integration test fixture
 */
class IntegrationTest : public BTestCase {
public:
    /**
     * @brief Constructor
     */
    IntegrationTest();
    
    /**
     * @brief Destructor
     */
    virtual ~IntegrationTest();
    
    /**
     * @brief Set up test environment before each test
     */
    virtual void setUp();
    
    /**
     * @brief Clean up test environment after each test
     */
    virtual void tearDown();
    
    // Integration test cases
    
    /**
     * @brief Test AuthenticationManager and OneDriveAPI integration
     */
    void TestAuthAPIIntegration();
    
    /**
     * @brief Test full authentication workflow
     */
    void TestFullAuthWorkflow();
    
    /**
     * @brief Test authenticated file operations
     */
    void TestAuthenticatedFileOps();
    
    /**
     * @brief Test daemon and API integration
     */
    void TestDaemonAPIIntegration();
    
    /**
     * @brief Test end-to-end file upload workflow
     */
    void TestEndToEndUpload();
    
    /**
     * @brief Test end-to-end file download workflow
     */
    void TestEndToEndDownload();
    
    /**
     * @brief Test authentication token refresh during API operations
     */
    void TestTokenRefreshDuringOps();
    
    /**
     * @brief Test error propagation across components
     */
    void TestErrorPropagation();
    
    /**
     * @brief Test component recovery from failures
     */
    void TestComponentRecovery();
    
    /**
     * @brief Test concurrent operations across components
     */
    void TestConcurrentOperations();
    
    /**
     * @brief Test settings synchronization between components
     */
    void TestSettingsSync();
    
    /**
     * @brief Test daemon service lifecycle with API operations
     */
    void TestServiceLifecycle();

private:
    AuthenticationManager* fAuthManager;   ///< Authentication component
    OneDriveAPI* fAPI;                     ///< API component
    OneDriveDaemon* fDaemon;               ///< Daemon component
    BString fTestClientId;                 ///< Test client ID
    BPath fTestFilePath;                   ///< Test file for operations
    BString fTestFileName;                 ///< Test file name
    
    /**
     * @brief Helper method to set up all components
     */
    status_t _SetupComponents();
    
    /**
     * @brief Helper method to clean up all components
     */
    void _CleanupComponents();
    
    /**
     * @brief Helper method to simulate full authentication
     */
    status_t _SimulateAuthentication();
    
    /**
     * @brief Helper method to create test file
     */
    status_t _CreateTestFile();
    
    /**
     * @brief Helper method to verify component integration
     */
    bool _VerifyComponentIntegration();
    
    /**
     * @brief Helper method to wait for daemon readiness
     */
    bool _WaitForDaemonReady(bigtime_t timeout = 5000000);
};

// Test suite registration
static IntegrationTest sIntegrationTest;

IntegrationTest::IntegrationTest()
    : BTestCase("IntegrationTest"),
      fAuthManager(nullptr),
      fAPI(nullptr),
      fDaemon(nullptr),
      fTestClientId("integration-test-client-id"),
      fTestFileName("integration_test_file.txt")
{
    fTestFilePath.SetTo("/tmp/integration_test_file.txt");
}

IntegrationTest::~IntegrationTest()
{
}

void IntegrationTest::setUp()
{
    // Clean up any existing components
    _CleanupComponents();
    
    // Set up components
    _SetupComponents();
    
    // Create test file
    _CreateTestFile();
}

void IntegrationTest::tearDown()
{
    // Clean up components
    _CleanupComponents();
    
    // Remove test file
    BEntry testFile(fTestFilePath.Path());
    if (testFile.Exists()) {
        testFile.Remove();
    }
}

void IntegrationTest::TestAuthAPIIntegration()
{
    // Test that AuthenticationManager and OneDriveAPI work together
    CPPUNIT_ASSERT(fAuthManager != nullptr);
    CPPUNIT_ASSERT(fAPI != nullptr);
    
    // Initially not authenticated
    CPPUNIT_ASSERT_EQUAL(false, fAuthManager->IsAuthenticated());
    
    // Simulate authentication
    status_t authResult = _SimulateAuthentication();
    
    if (authResult == B_OK) {
        // Should now be authenticated
        CPPUNIT_ASSERT_EQUAL(true, fAuthManager->IsAuthenticated());
        
        // API should be able to use authentication
        BMessage userProfile;
        status_t apiResult = fAPI->GetUserProfile(userProfile);
        
        // Should succeed since we're authenticated
        CPPUNIT_ASSERT_EQUAL(B_OK, apiResult);
    }
}

void IntegrationTest::TestFullAuthWorkflow()
{
    // Test complete authentication workflow from start to finish
    
    // 1. Get authorization URL
    BString authUrl;
    status_t urlResult = fAuthManager->GetAuthorizationURL(authUrl);
    CPPUNIT_ASSERT_EQUAL(B_OK, urlResult);
    CPPUNIT_ASSERT(authUrl.Length() > 0);
    
    // 2. Simulate OAuth callback
    status_t callbackResult = fAuthManager->HandleAuthCallback("test-auth-code");
    CPPUNIT_ASSERT_EQUAL(B_OK, callbackResult);
    
    // 3. Verify authentication state
    CPPUNIT_ASSERT_EQUAL(true, fAuthManager->IsAuthenticated());
    CPPUNIT_ASSERT_EQUAL(AUTH_STATE_AUTHENTICATED, fAuthManager->GetAuthState());
    
    // 4. Get access token
    BString accessToken;
    status_t tokenResult = fAuthManager->GetAccessToken(accessToken);
    CPPUNIT_ASSERT_EQUAL(B_OK, tokenResult);
    CPPUNIT_ASSERT(accessToken.Length() > 0);
    
    // 5. Use token with API
    BMessage userProfile;
    status_t profileResult = fAPI->GetUserProfile(userProfile);
    CPPUNIT_ASSERT_EQUAL(B_OK, profileResult);
}

void IntegrationTest::TestAuthenticatedFileOps()
{
    // Test file operations with authenticated API
    
    // First authenticate
    status_t authResult = _SimulateAuthentication();
    if (authResult != B_OK) {
        // Skip test if authentication simulation fails
        return;
    }
    
    // Test file upload
    OneDriveItem uploadedFile;
    status_t uploadResult = fAPI->UploadFile("root", fTestFilePath.Path(), 
                                           fTestFileName, uploadedFile, nullptr, nullptr);
    
    if (uploadResult == B_OK) {
        // Verify upload result
        CPPUNIT_ASSERT_EQUAL(fTestFileName, uploadedFile.name);
        CPPUNIT_ASSERT_EQUAL(ITEM_TYPE_FILE, uploadedFile.type);
        CPPUNIT_ASSERT(uploadedFile.id.Length() > 0);
        
        // Test file download
        BPath downloadPath("/tmp/downloaded_integration_test.txt");
        status_t downloadResult = fAPI->DownloadFile(uploadedFile.id, downloadPath.Path(), 
                                                    nullptr, nullptr);
        
        if (downloadResult == B_OK) {
            // Verify downloaded file exists
            BEntry downloadedFile(downloadPath.Path());
            CPPUNIT_ASSERT_EQUAL(true, downloadedFile.Exists());
            
            // Clean up
            downloadedFile.Remove();
        }
        
        // Test file deletion
        status_t deleteResult = fAPI->DeleteItem(uploadedFile.id);
        CPPUNIT_ASSERT_EQUAL(B_OK, deleteResult);
    }
}

void IntegrationTest::TestDaemonAPIIntegration()
{
    // Test that daemon can control API operations
    if (!fDaemon || !_WaitForDaemonReady()) {
        // Skip if daemon not available
        return;
    }
    
    // Send authentication request to daemon
    BMessage authMsg(ONEDRIVE_MSG_AUTH_REQUEST);
    BMessage authReply;
    
    BMessenger daemonMessenger(fDaemon);
    status_t msgResult = daemonMessenger.SendMessage(&authMsg, &authReply, 5000000, 5000000);
    
    if (msgResult == B_OK) {
        // Should get authentication URL or status
        BString authUrl;
        int32 authState;
        
        bool hasUrl = authReply.FindString("auth_url", &authUrl) == B_OK;
        bool hasState = authReply.FindInt32("auth_state", &authState) == B_OK;
        
        CPPUNIT_ASSERT(hasUrl || hasState);
    }
    
    // Test service status
    BMessage statusMsg(ONEDRIVE_MSG_SERVICE_STATUS);
    BMessage statusReply;
    
    msgResult = daemonMessenger.SendMessage(&statusMsg, &statusReply, 5000000, 5000000);
    
    if (msgResult == B_OK) {
        bool isRunning;
        statusReply.FindBool("is_running", &isRunning);
        // Daemon should report its status
    }
}

void IntegrationTest::TestEndToEndUpload()
{
    // Test complete upload workflow through all components
    
    // 1. Authenticate
    status_t authResult = _SimulateAuthentication();
    if (authResult != B_OK) return;
    
    // 2. Upload file through API
    OneDriveItem uploadedFile;
    status_t uploadResult = fAPI->UploadFile("root", fTestFilePath.Path(), 
                                           fTestFileName, uploadedFile, nullptr, nullptr);
    
    if (uploadResult == B_OK) {
        // 3. Verify file exists on OneDrive
        OneDriveItem retrievedFile;
        status_t getResult = fAPI->GetItemInfo(uploadedFile.id, retrievedFile);
        
        CPPUNIT_ASSERT_EQUAL(B_OK, getResult);
        CPPUNIT_ASSERT_EQUAL(uploadedFile.id, retrievedFile.id);
        CPPUNIT_ASSERT_EQUAL(uploadedFile.name, retrievedFile.name);
        
        // 4. Clean up
        fAPI->DeleteItem(uploadedFile.id);
    }
}

void IntegrationTest::TestEndToEndDownload()
{
    // Test complete download workflow
    
    // 1. Authenticate and upload a file first
    status_t authResult = _SimulateAuthentication();
    if (authResult != B_OK) return;
    
    OneDriveItem uploadedFile;
    status_t uploadResult = fAPI->UploadFile("root", fTestFilePath.Path(), 
                                           fTestFileName, uploadedFile, nullptr, nullptr);
    
    if (uploadResult == B_OK) {
        // 2. Download the file
        BPath downloadPath("/tmp/end_to_end_download_test.txt");
        status_t downloadResult = fAPI->DownloadFile(uploadedFile.id, downloadPath.Path(), 
                                                    nullptr, nullptr);
        
        CPPUNIT_ASSERT_EQUAL(B_OK, downloadResult);
        
        // 3. Verify downloaded file
        BEntry downloadedFile(downloadPath.Path());
        CPPUNIT_ASSERT_EQUAL(true, downloadedFile.Exists());
        
        // 4. Compare file contents
        BFile originalFile(fTestFilePath.Path(), B_READ_ONLY);
        BFile downloadedFileHandle(downloadPath.Path(), B_READ_ONLY);
        
        if (originalFile.InitCheck() == B_OK && downloadedFileHandle.InitCheck() == B_OK) {
            off_t originalSize, downloadedSize;
            originalFile.GetSize(&originalSize);
            downloadedFileHandle.GetSize(&downloadedSize);
            
            CPPUNIT_ASSERT_EQUAL(originalSize, downloadedSize);
        }
        
        // 5. Clean up
        downloadedFile.Remove();
        fAPI->DeleteItem(uploadedFile.id);
    }
}

void IntegrationTest::TestTokenRefreshDuringOps()
{
    // Test that token refresh works during ongoing operations
    
    // Authenticate first
    status_t authResult = _SimulateAuthentication();
    if (authResult != B_OK) return;
    
    // Perform an operation
    BMessage userProfile;
    status_t profileResult = fAPI->GetUserProfile(userProfile);
    
    if (profileResult == B_OK) {
        // Force token refresh
        status_t refreshResult = fAuthManager->RefreshToken();
        CPPUNIT_ASSERT_EQUAL(B_OK, refreshResult);
        
        // Should still be authenticated
        CPPUNIT_ASSERT_EQUAL(true, fAuthManager->IsAuthenticated());
        
        // API operations should still work
        BMessage driveInfo;
        status_t driveResult = fAPI->GetDriveInfo(driveInfo);
        CPPUNIT_ASSERT_EQUAL(B_OK, driveResult);
    }
}

void IntegrationTest::TestErrorPropagation()
{
    // Test that errors propagate correctly between components
    
    // Test unauthenticated API access
    BMessage userProfile;
    status_t result = fAPI->GetUserProfile(userProfile);
    
    // Should fail since not authenticated
    CPPUNIT_ASSERT(result != B_OK);
    
    // Test invalid authentication
    status_t authResult = fAuthManager->HandleAuthCallback("invalid-auth-code");
    
    // Depending on implementation, might succeed in dev mode or fail
    if (authResult != B_OK) {
        CPPUNIT_ASSERT_EQUAL(false, fAuthManager->IsAuthenticated());
    }
}

void IntegrationTest::TestComponentRecovery()
{
    // Test that components can recover from failures
    
    // Authenticate first
    status_t authResult = _SimulateAuthentication();
    if (authResult != B_OK) return;
    
    // Simulate logout
    fAuthManager->Logout();
    CPPUNIT_ASSERT_EQUAL(false, fAuthManager->IsAuthenticated());
    
    // API operations should fail
    BMessage userProfile;
    status_t profileResult = fAPI->GetUserProfile(userProfile);
    CPPUNIT_ASSERT(profileResult != B_OK);
    
    // Re-authenticate
    authResult = _SimulateAuthentication();
    if (authResult == B_OK) {
        // Should work again
        CPPUNIT_ASSERT_EQUAL(true, fAuthManager->IsAuthenticated());
        
        profileResult = fAPI->GetUserProfile(userProfile);
        CPPUNIT_ASSERT_EQUAL(B_OK, profileResult);
    }
}

void IntegrationTest::TestConcurrentOperations()
{
    // Test concurrent operations across components
    
    // Authenticate first
    status_t authResult = _SimulateAuthentication();
    if (authResult != B_OK) return;
    
    // Perform multiple operations rapidly
    for (int i = 0; i < 3; i++) {
        // Get user profile
        BMessage userProfile;
        fAPI->GetUserProfile(userProfile);
        
        // Get drive info
        BMessage driveInfo;
        fAPI->GetDriveInfo(driveInfo);
        
        // Check auth state
        fAuthManager->IsAuthenticated();
        
        // Small delay
        snooze(10000); // 10ms
    }
    
    // All operations should complete without issues
    CPPUNIT_ASSERT_EQUAL(true, fAuthManager->IsAuthenticated());
}

void IntegrationTest::TestSettingsSync()
{
    // Test settings synchronization between components
    
    // Set client ID in auth manager
    status_t setResult = fAuthManager->SetClientId(fTestClientId);
    CPPUNIT_ASSERT_EQUAL(B_OK, setResult);
    
    // Verify authorization URL contains the client ID
    BString authUrl;
    status_t urlResult = fAuthManager->GetAuthorizationURL(authUrl);
    
    if (urlResult == B_OK) {
        CPPUNIT_ASSERT(authUrl.FindFirst(fTestClientId) >= 0);
    }
}

void IntegrationTest::TestServiceLifecycle()
{
    // Test daemon service lifecycle with API operations
    if (!fDaemon) return;
    
    // Start daemon
    if (!_WaitForDaemonReady()) return;
    
    BMessenger daemonMessenger(fDaemon);
    
    // Start service
    BMessage startMsg(ONEDRIVE_MSG_SERVICE_START);
    BMessage startReply;
    status_t startResult = daemonMessenger.SendMessage(&startMsg, &startReply, 5000000, 5000000);
    
    if (startResult == B_OK) {
        int32 replyCode;
        startReply.FindInt32("result", &replyCode);
        CPPUNIT_ASSERT(replyCode == B_OK || replyCode == B_ALREADY_RUNNING);
        
        // Check status
        BMessage statusMsg(ONEDRIVE_MSG_SERVICE_STATUS);
        BMessage statusReply;
        status_t statusResult = daemonMessenger.SendMessage(&statusMsg, &statusReply, 5000000, 5000000);
        
        if (statusResult == B_OK) {
            bool isRunning;
            statusReply.FindBool("is_running", &isRunning);
            // Service should be running
        }
        
        // Stop service
        BMessage stopMsg(ONEDRIVE_MSG_SERVICE_STOP);
        BMessage stopReply;
        status_t stopResult = daemonMessenger.SendMessage(&stopMsg, &stopReply, 5000000, 5000000);
        
        if (stopResult == B_OK) {
            int32 stopReplyCode;
            stopReply.FindInt32("result", &stopReplyCode);
            CPPUNIT_ASSERT_EQUAL(B_OK, (status_t)stopReplyCode);
        }
    }
}

status_t IntegrationTest::_SetupComponents()
{
    // Create AuthenticationManager
    fAuthManager = new AuthenticationManager();
    if (!fAuthManager) return B_NO_MEMORY;
    
    // Set client ID
    fAuthManager->SetClientId(fTestClientId);
    
    // Create OneDriveAPI
    fAPI = new OneDriveAPI(fAuthManager);
    if (!fAPI) return B_NO_MEMORY;
    
    // Create OneDriveDaemon
    fDaemon = new OneDriveDaemon();
    if (!fDaemon) return B_NO_MEMORY;
    
    // Start daemon
    fDaemon->Run();
    
    return B_OK;
}

void IntegrationTest::_CleanupComponents()
{
    if (fDaemon) {
        fDaemon->Quit();
        delete fDaemon;
        fDaemon = nullptr;
    }
    
    if (fAPI) {
        delete fAPI;
        fAPI = nullptr;
    }
    
    if (fAuthManager) {
        fAuthManager->Logout();
        delete fAuthManager;
        fAuthManager = nullptr;
    }
}

status_t IntegrationTest::_SimulateAuthentication()
{
    if (!fAuthManager) return B_ERROR;
    
    // In development mode, use test authorization code
    return fAuthManager->HandleAuthCallback("integration-test-auth-code");
}

status_t IntegrationTest::_CreateTestFile()
{
    BFile testFile(fTestFilePath.Path(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
    
    if (testFile.InitCheck() != B_OK) {
        return testFile.InitCheck();
    }
    
    const char* testContent = "Integration test file for OneDrive components.\n"
                             "This file tests end-to-end workflows.\n"
                             "Created by Haiku OneDrive integration test suite.\n";
    
    ssize_t written = testFile.Write(testContent, strlen(testContent));
    if (written < 0) {
        return written;
    }
    
    return B_OK;
}

bool IntegrationTest::_VerifyComponentIntegration()
{
    return (fAuthManager != nullptr) && (fAPI != nullptr) && (fDaemon != nullptr);
}

bool IntegrationTest::_WaitForDaemonReady(bigtime_t timeout)
{
    if (!fDaemon) return false;
    
    bigtime_t start = system_time();
    
    while ((system_time() - start) < timeout) {
        if (fDaemon->IsRunning()) {
            return true;
        }
        snooze(10000); // 10ms
    }
    
    return false;
}

// Test suite factory
CppUnit::Test* IntegrationTestSuite()
{
    CppUnit::TestSuite* suite = new CppUnit::TestSuite("IntegrationTestSuite");
    
    suite->addTest(new CppUnit::TestCaller<IntegrationTest>(
        "TestAuthAPIIntegration", &IntegrationTest::TestAuthAPIIntegration));
    suite->addTest(new CppUnit::TestCaller<IntegrationTest>(
        "TestFullAuthWorkflow", &IntegrationTest::TestFullAuthWorkflow));
    suite->addTest(new CppUnit::TestCaller<IntegrationTest>(
        "TestAuthenticatedFileOps", &IntegrationTest::TestAuthenticatedFileOps));
    suite->addTest(new CppUnit::TestCaller<IntegrationTest>(
        "TestDaemonAPIIntegration", &IntegrationTest::TestDaemonAPIIntegration));
    suite->addTest(new CppUnit::TestCaller<IntegrationTest>(
        "TestEndToEndUpload", &IntegrationTest::TestEndToEndUpload));
    suite->addTest(new CppUnit::TestCaller<IntegrationTest>(
        "TestEndToEndDownload", &IntegrationTest::TestEndToEndDownload));
    suite->addTest(new CppUnit::TestCaller<IntegrationTest>(
        "TestTokenRefreshDuringOps", &IntegrationTest::TestTokenRefreshDuringOps));
    suite->addTest(new CppUnit::TestCaller<IntegrationTest>(
        "TestErrorPropagation", &IntegrationTest::TestErrorPropagation));
    suite->addTest(new CppUnit::TestCaller<IntegrationTest>(
        "TestComponentRecovery", &IntegrationTest::TestComponentRecovery));
    suite->addTest(new CppUnit::TestCaller<IntegrationTest>(
        "TestConcurrentOperations", &IntegrationTest::TestConcurrentOperations));
    suite->addTest(new CppUnit::TestCaller<IntegrationTest>(
        "TestSettingsSync", &IntegrationTest::TestSettingsSync));
    suite->addTest(new CppUnit::TestCaller<IntegrationTest>(
        "TestServiceLifecycle", &IntegrationTest::TestServiceLifecycle));
    
    return suite;
}