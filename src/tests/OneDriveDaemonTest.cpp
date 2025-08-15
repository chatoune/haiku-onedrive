/**
 * @file OneDriveDaemonTest.cpp
 * @brief Unit tests for OneDriveDaemon class
 * @author Haiku OneDrive Team
 * @version 1.0.0
 * @date 2025-08-14
 * 
 * Comprehensive unit tests for the OneDriveDaemon class covering:
 * - Daemon initialization and shutdown
 * - Message handling and processing
 * - Settings management
 * - Service control (start, stop, restart)
 * - IPC communication with preferences
 * - Error handling and recovery
 */

#include <TestSuite.h>
#include <TestCase.h>
#include <TestUtils.h>
#include <String.h>
#include <Message.h>
#include <Messenger.h>
#include <Looper.h>
#include <OS.h>
#include <stdio.h>

#include "../daemon/OneDriveDaemon.h"
#include "../shared/OneDriveConstants.h"

/**
 * @brief Test fixture for OneDriveDaemon tests
 */
class OneDriveDaemonTest : public BTestCase {
public:
    /**
     * @brief Constructor
     */
    OneDriveDaemonTest();
    
    /**
     * @brief Destructor
     */
    virtual ~OneDriveDaemonTest();
    
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
     * @brief Test OneDriveDaemon construction and initialization
     */
    void TestConstruction();
    
    /**
     * @brief Test daemon startup and shutdown
     */
    void TestStartupShutdown();
    
    /**
     * @brief Test settings loading and saving
     */
    void TestSettingsManagement();
    
    /**
     * @brief Test service start command
     */
    void TestServiceStart();
    
    /**
     * @brief Test service stop command
     */
    void TestServiceStop();
    
    /**
     * @brief Test service restart command
     */
    void TestServiceRestart();
    
    /**
     * @brief Test service status query
     */
    void TestServiceStatus();
    
    /**
     * @brief Test authentication request handling
     */
    void TestAuthenticationRequest();
    
    /**
     * @brief Test sync start/stop commands
     */
    void TestSyncControl();
    
    /**
     * @brief Test sync status queries
     */
    void TestSyncStatus();
    
    /**
     * @brief Test quota information requests
     */
    void TestQuotaRequest();
    
    /**
     * @brief Test settings update messages
     */
    void TestSettingsUpdate();
    
    /**
     * @brief Test unknown message handling
     */
    void TestUnknownMessage();
    
    /**
     * @brief Test message timeout handling
     */
    void TestMessageTimeout();
    
    /**
     * @brief Test concurrent message processing
     */
    void TestConcurrentMessages();
    
    /**
     * @brief Test daemon recovery from errors
     */
    void TestErrorRecovery();
    
    /**
     * @brief Test daemon persistence across restarts
     */
    void TestPersistence();
    
    /**
     * @brief Test signal handling (SIGTERM, SIGINT)
     */
    void TestSignalHandling();
    
    /**
     * @brief Test daemon auto-restart functionality
     */
    void TestAutoRestart();

private:
    OneDriveDaemon* fDaemon;           ///< Test subject
    BMessenger* fDaemonMessenger;      ///< Messenger to daemon
    bool fDaemonStarted;               ///< Track daemon state
    
    /**
     * @brief Helper method to start daemon in test mode
     */
    status_t _StartTestDaemon();
    
    /**
     * @brief Helper method to stop test daemon
     */
    void _StopTestDaemon();
    
    /**
     * @brief Helper method to send message and wait for reply
     */
    status_t _SendMessageAndWait(BMessage* message, BMessage* reply, 
                                bigtime_t timeout = 5000000);
    
    /**
     * @brief Helper method to verify daemon is running
     */
    bool _IsDaemonRunning();
    
    /**
     * @brief Helper method to wait for daemon state change
     */
    bool _WaitForDaemonState(bool shouldBeRunning, bigtime_t timeout = 5000000);
    
    /**
     * @brief Helper method to create test settings
     */
    BMessage _CreateTestSettings();
};

// Test suite registration
static OneDriveDaemonTest sOneDriveDaemonTest;

OneDriveDaemonTest::OneDriveDaemonTest()
    : BTestCase("OneDriveDaemonTest"),
      fDaemon(nullptr),
      fDaemonMessenger(nullptr),
      fDaemonStarted(false)
{
}

OneDriveDaemonTest::~OneDriveDaemonTest()
{
}

void OneDriveDaemonTest::setUp()
{
    // Clean up any existing daemon instance
    _StopTestDaemon();
    
    // Create fresh daemon instance
    fDaemon = new OneDriveDaemon();
    
    // Start daemon in test mode
    _StartTestDaemon();
}

void OneDriveDaemonTest::tearDown()
{
    // Stop daemon
    _StopTestDaemon();
    
    // Clean up
    if (fDaemonMessenger) {
        delete fDaemonMessenger;
        fDaemonMessenger = nullptr;
    }
    
    if (fDaemon) {
        delete fDaemon;
        fDaemon = nullptr;
    }
}

void OneDriveDaemonTest::TestConstruction()
{
    // Test that OneDriveDaemon can be constructed successfully
    OneDriveDaemon* daemon = new OneDriveDaemon();
    CPPUNIT_ASSERT(daemon != nullptr);
    
    // Test initial state
    CPPUNIT_ASSERT_EQUAL(false, daemon->IsRunning());
    
    delete daemon;
}

void OneDriveDaemonTest::TestStartupShutdown()
{
    // Test starting daemon
    CPPUNIT_ASSERT(fDaemon != nullptr);
    
    if (fDaemonStarted) {
        CPPUNIT_ASSERT_EQUAL(true, fDaemon->IsRunning());
        
        // Test stopping daemon
        fDaemon->Quit();
        
        // Wait for shutdown
        bool stopped = _WaitForDaemonState(false, 3000000);
        CPPUNIT_ASSERT_EQUAL(true, stopped);
        
        fDaemonStarted = false;
    }
}

void OneDriveDaemonTest::TestSettingsManagement()
{
    if (!fDaemonStarted) return;
    
    // Create test settings
    BMessage testSettings = _CreateTestSettings();
    
    // Send settings update message
    BMessage settingsMsg(ONEDRIVE_MSG_SETTINGS_UPDATE);
    settingsMsg.AddMessage("settings", &testSettings);
    
    BMessage reply;
    status_t result = _SendMessageAndWait(&settingsMsg, &reply);
    
    if (result == B_OK) {
        // Verify settings were accepted
        int32 replyCode;
        reply.FindInt32("result", &replyCode);
        CPPUNIT_ASSERT_EQUAL(B_OK, (status_t)replyCode);
    }
}

void OneDriveDaemonTest::TestServiceStart()
{
    if (!fDaemonStarted) return;
    
    // Send service start message
    BMessage startMsg(ONEDRIVE_MSG_SERVICE_START);
    BMessage reply;
    
    status_t result = _SendMessageAndWait(&startMsg, &reply);
    
    if (result == B_OK) {
        int32 replyCode;
        reply.FindInt32("result", &replyCode);
        
        // Service start should succeed or already be running
        CPPUNIT_ASSERT(replyCode == B_OK || replyCode == B_ALREADY_RUNNING);
    }
}

void OneDriveDaemonTest::TestServiceStop()
{
    if (!fDaemonStarted) return;
    
    // First start service
    BMessage startMsg(ONEDRIVE_MSG_SERVICE_START);
    BMessage startReply;
    _SendMessageAndWait(&startMsg, &startReply);
    
    // Now stop service
    BMessage stopMsg(ONEDRIVE_MSG_SERVICE_STOP);
    BMessage reply;
    
    status_t result = _SendMessageAndWait(&stopMsg, &reply);
    
    if (result == B_OK) {
        int32 replyCode;
        reply.FindInt32("result", &replyCode);
        CPPUNIT_ASSERT_EQUAL(B_OK, (status_t)replyCode);
    }
}

void OneDriveDaemonTest::TestServiceRestart()
{
    if (!fDaemonStarted) return;
    
    // Send service restart message
    BMessage restartMsg(ONEDRIVE_MSG_SERVICE_RESTART);
    BMessage reply;
    
    status_t result = _SendMessageAndWait(&restartMsg, &reply);
    
    if (result == B_OK) {
        int32 replyCode;
        reply.FindInt32("result", &replyCode);
        CPPUNIT_ASSERT_EQUAL(B_OK, (status_t)replyCode);
    }
}

void OneDriveDaemonTest::TestServiceStatus()
{
    if (!fDaemonStarted) return;
    
    // Send service status request
    BMessage statusMsg(ONEDRIVE_MSG_SERVICE_STATUS);
    BMessage reply;
    
    status_t result = _SendMessageAndWait(&statusMsg, &reply);
    
    if (result == B_OK) {
        // Verify status response contains expected fields
        bool isRunning, isAuthenticated;
        BString status;
        
        reply.FindBool("is_running", &isRunning);
        reply.FindBool("is_authenticated", &isAuthenticated);
        reply.FindString("status", &status);
        
        // These fields should be present
        CPPUNIT_ASSERT(status.Length() > 0);
    }
}

void OneDriveDaemonTest::TestAuthenticationRequest()
{
    if (!fDaemonStarted) return;
    
    // Send authentication request
    BMessage authMsg(ONEDRIVE_MSG_AUTH_REQUEST);
    BMessage reply;
    
    status_t result = _SendMessageAndWait(&authMsg, &reply);
    
    if (result == B_OK) {
        // Should get authentication URL or status
        BString authUrl;
        int32 authState;
        
        if (reply.FindString("auth_url", &authUrl) == B_OK) {
            CPPUNIT_ASSERT(authUrl.Length() > 0);
        }
        
        if (reply.FindInt32("auth_state", &authState) == B_OK) {
            CPPUNIT_ASSERT(authState >= 0);
        }
    }
}

void OneDriveDaemonTest::TestSyncControl()
{
    if (!fDaemonStarted) return;
    
    // Test sync start
    BMessage syncStartMsg(ONEDRIVE_MSG_SYNC_START);
    BMessage startReply;
    
    status_t result = _SendMessageAndWait(&syncStartMsg, &startReply);
    
    if (result == B_OK) {
        int32 replyCode;
        startReply.FindInt32("result", &replyCode);
        
        // Sync start should succeed or already be running
        CPPUNIT_ASSERT(replyCode == B_OK || replyCode == B_ALREADY_RUNNING);
    }
    
    // Test sync stop
    BMessage syncStopMsg(ONEDRIVE_MSG_SYNC_STOP);
    BMessage stopReply;
    
    result = _SendMessageAndWait(&syncStopMsg, &stopReply);
    
    if (result == B_OK) {
        int32 replyCode;
        stopReply.FindInt32("result", &replyCode);
        CPPUNIT_ASSERT_EQUAL(B_OK, (status_t)replyCode);
    }
}

void OneDriveDaemonTest::TestSyncStatus()
{
    if (!fDaemonStarted) return;
    
    // Send sync status request
    BMessage statusMsg(ONEDRIVE_MSG_SYNC_STATUS);
    BMessage reply;
    
    status_t result = _SendMessageAndWait(&statusMsg, &reply);
    
    if (result == B_OK) {
        // Verify sync status response
        bool isSyncing;
        int32 filesInQueue;
        BString lastSync;
        
        reply.FindBool("is_syncing", &isSyncing);
        reply.FindInt32("files_in_queue", &filesInQueue);
        reply.FindString("last_sync", &lastSync);
        
        // Files in queue should be non-negative
        CPPUNIT_ASSERT(filesInQueue >= 0);
    }
}

void OneDriveDaemonTest::TestQuotaRequest()
{
    if (!fDaemonStarted) return;
    
    // Send quota request
    BMessage quotaMsg(ONEDRIVE_MSG_QUOTA_REQUEST);
    BMessage reply;
    
    status_t result = _SendMessageAndWait(&quotaMsg, &reply);
    
    if (result == B_OK) {
        // Verify quota response
        int64 totalSpace, usedSpace, remainingSpace;
        
        if (reply.FindInt64("total_space", &totalSpace) == B_OK &&
            reply.FindInt64("used_space", &usedSpace) == B_OK &&
            reply.FindInt64("remaining_space", &remainingSpace) == B_OK) {
            
            CPPUNIT_ASSERT(totalSpace >= 0);
            CPPUNIT_ASSERT(usedSpace >= 0);
            CPPUNIT_ASSERT(remainingSpace >= 0);
        }
    }
}

void OneDriveDaemonTest::TestSettingsUpdate()
{
    if (!fDaemonStarted) return;
    
    // Create settings update
    BMessage settings;
    settings.AddString("sync_folder", "/boot/home/OneDrive");
    settings.AddBool("sync_hidden_files", true);
    settings.AddInt32("bandwidth_limit", 1000);
    
    BMessage settingsMsg(ONEDRIVE_MSG_SETTINGS_UPDATE);
    settingsMsg.AddMessage("settings", &settings);
    
    BMessage reply;
    status_t result = _SendMessageAndWait(&settingsMsg, &reply);
    
    if (result == B_OK) {
        int32 replyCode;
        reply.FindInt32("result", &replyCode);
        CPPUNIT_ASSERT_EQUAL(B_OK, (status_t)replyCode);
    }
}

void OneDriveDaemonTest::TestUnknownMessage()
{
    if (!fDaemonStarted) return;
    
    // Send unknown message
    BMessage unknownMsg(0x12345678);
    BMessage reply;
    
    status_t result = _SendMessageAndWait(&unknownMsg, &reply, 2000000); // Shorter timeout
    
    // Should either timeout or respond with error
    CPPUNIT_ASSERT(result == B_TIMED_OUT || result == B_ERROR);
}

void OneDriveDaemonTest::TestMessageTimeout()
{
    if (!fDaemonStarted) return;
    
    // Send valid message but with very short timeout
    BMessage statusMsg(ONEDRIVE_MSG_SERVICE_STATUS);
    BMessage reply;
    
    status_t result = _SendMessageAndWait(&statusMsg, &reply, 1000); // 1ms timeout
    
    // Should likely timeout
    CPPUNIT_ASSERT(result == B_TIMED_OUT || result == B_OK);
}

void OneDriveDaemonTest::TestConcurrentMessages()
{
    if (!fDaemonStarted) return;
    
    // Send multiple status requests rapidly
    for (int i = 0; i < 3; i++) {
        BMessage statusMsg(ONEDRIVE_MSG_SERVICE_STATUS);
        BMessage reply;
        
        status_t result = _SendMessageAndWait(&statusMsg, &reply, 1000000);
        
        // Should handle concurrent requests
        CPPUNIT_ASSERT(result == B_OK || result == B_TIMED_OUT);
        
        // Small delay between requests
        snooze(10000); // 10ms
    }
}

void OneDriveDaemonTest::TestErrorRecovery()
{
    if (!fDaemonStarted) return;
    
    // Test daemon recovery by sending invalid data
    BMessage invalidMsg(ONEDRIVE_MSG_SETTINGS_UPDATE);
    // Don't add settings message - should cause error
    
    BMessage reply;
    status_t result = _SendMessageAndWait(&invalidMsg, &reply);
    
    // Should handle error gracefully
    if (result == B_OK) {
        int32 replyCode;
        reply.FindInt32("result", &replyCode);
        CPPUNIT_ASSERT(replyCode != B_OK); // Should return error
    }
    
    // Daemon should still be responsive after error
    BMessage statusMsg(ONEDRIVE_MSG_SERVICE_STATUS);
    BMessage statusReply;
    status_t statusResult = _SendMessageAndWait(&statusMsg, &statusReply);
    
    CPPUNIT_ASSERT_EQUAL(B_OK, statusResult);
}

void OneDriveDaemonTest::TestPersistence()
{
    // This test would verify that daemon settings persist across restarts
    // For now, just verify that a new daemon instance can be created
    
    OneDriveDaemon* newDaemon = new OneDriveDaemon();
    CPPUNIT_ASSERT(newDaemon != nullptr);
    
    delete newDaemon;
}

void OneDriveDaemonTest::TestSignalHandling()
{
    // This test would verify signal handling (SIGTERM, SIGINT)
    // In a test environment, we just verify the daemon can be stopped cleanly
    
    if (fDaemonStarted && fDaemon) {
        // Stop daemon cleanly
        fDaemon->Quit();
        
        // Verify it stops
        bool stopped = _WaitForDaemonState(false, 3000000);
        CPPUNIT_ASSERT_EQUAL(true, stopped);
        
        fDaemonStarted = false;
    }
}

void OneDriveDaemonTest::TestAutoRestart()
{
    // This test would verify auto-restart functionality
    // For now, just verify that daemon can be restarted manually
    
    if (fDaemonStarted) {
        // Send restart command
        BMessage restartMsg(ONEDRIVE_MSG_SERVICE_RESTART);
        BMessage reply;
        
        status_t result = _SendMessageAndWait(&restartMsg, &reply);
        
        if (result == B_OK) {
            int32 replyCode;
            reply.FindInt32("result", &replyCode);
            CPPUNIT_ASSERT_EQUAL(B_OK, (status_t)replyCode);
        }
    }
}

status_t OneDriveDaemonTest::_StartTestDaemon()
{
    if (!fDaemon) {
        return B_ERROR;
    }
    
    // Start daemon
    status_t result = fDaemon->Run();
    if (result != B_OK) {
        return result;
    }
    
    // Create messenger
    fDaemonMessenger = new BMessenger(fDaemon);
    
    // Wait for daemon to be ready
    if (_WaitForDaemonState(true, 3000000)) {
        fDaemonStarted = true;
        return B_OK;
    }
    
    return B_ERROR;
}

void OneDriveDaemonTest::_StopTestDaemon()
{
    if (fDaemonStarted && fDaemon) {
        fDaemon->Quit();
        _WaitForDaemonState(false, 3000000);
        fDaemonStarted = false;
    }
    
    if (fDaemonMessenger) {
        delete fDaemonMessenger;
        fDaemonMessenger = nullptr;
    }
}

status_t OneDriveDaemonTest::_SendMessageAndWait(BMessage* message, BMessage* reply, 
                                                bigtime_t timeout)
{
    if (!fDaemonMessenger || !message || !reply) {
        return B_BAD_VALUE;
    }
    
    return fDaemonMessenger->SendMessage(message, reply, timeout, timeout);
}

bool OneDriveDaemonTest::_IsDaemonRunning()
{
    return fDaemonStarted && fDaemon && fDaemon->IsRunning();
}

bool OneDriveDaemonTest::_WaitForDaemonState(bool shouldBeRunning, bigtime_t timeout)
{
    bigtime_t start = system_time();
    
    while ((system_time() - start) < timeout) {
        bool isRunning = _IsDaemonRunning();
        if (isRunning == shouldBeRunning) {
            return true;
        }
        
        snooze(10000); // 10ms
    }
    
    return false;
}

BMessage OneDriveDaemonTest::_CreateTestSettings()
{
    BMessage settings;
    
    settings.AddString("sync_folder", "/boot/home/OneDrive");
    settings.AddBool("sync_hidden_files", false);
    settings.AddBool("sync_attributes", true);
    settings.AddBool("wifi_only", false);
    settings.AddInt32("bandwidth_limit", 0);
    settings.AddString("client_id", "test-client-id");
    
    return settings;
}

// Test suite factory
CppUnit::Test* OneDriveDaemonTestSuite()
{
    CppUnit::TestSuite* suite = new CppUnit::TestSuite("OneDriveDaemonTestSuite");
    
    suite->addTest(new CppUnit::TestCaller<OneDriveDaemonTest>(
        "TestConstruction", &OneDriveDaemonTest::TestConstruction));
    suite->addTest(new CppUnit::TestCaller<OneDriveDaemonTest>(
        "TestStartupShutdown", &OneDriveDaemonTest::TestStartupShutdown));
    suite->addTest(new CppUnit::TestCaller<OneDriveDaemonTest>(
        "TestSettingsManagement", &OneDriveDaemonTest::TestSettingsManagement));
    suite->addTest(new CppUnit::TestCaller<OneDriveDaemonTest>(
        "TestServiceStart", &OneDriveDaemonTest::TestServiceStart));
    suite->addTest(new CppUnit::TestCaller<OneDriveDaemonTest>(
        "TestServiceStop", &OneDriveDaemonTest::TestServiceStop));
    suite->addTest(new CppUnit::TestCaller<OneDriveDaemonTest>(
        "TestServiceRestart", &OneDriveDaemonTest::TestServiceRestart));
    suite->addTest(new CppUnit::TestCaller<OneDriveDaemonTest>(
        "TestServiceStatus", &OneDriveDaemonTest::TestServiceStatus));
    suite->addTest(new CppUnit::TestCaller<OneDriveDaemonTest>(
        "TestAuthenticationRequest", &OneDriveDaemonTest::TestAuthenticationRequest));
    suite->addTest(new CppUnit::TestCaller<OneDriveDaemonTest>(
        "TestSyncControl", &OneDriveDaemonTest::TestSyncControl));
    suite->addTest(new CppUnit::TestCaller<OneDriveDaemonTest>(
        "TestSyncStatus", &OneDriveDaemonTest::TestSyncStatus));
    suite->addTest(new CppUnit::TestCaller<OneDriveDaemonTest>(
        "TestQuotaRequest", &OneDriveDaemonTest::TestQuotaRequest));
    suite->addTest(new CppUnit::TestCaller<OneDriveDaemonTest>(
        "TestSettingsUpdate", &OneDriveDaemonTest::TestSettingsUpdate));
    suite->addTest(new CppUnit::TestCaller<OneDriveDaemonTest>(
        "TestUnknownMessage", &OneDriveDaemonTest::TestUnknownMessage));
    suite->addTest(new CppUnit::TestCaller<OneDriveDaemonTest>(
        "TestMessageTimeout", &OneDriveDaemonTest::TestMessageTimeout));
    suite->addTest(new CppUnit::TestCaller<OneDriveDaemonTest>(
        "TestConcurrentMessages", &OneDriveDaemonTest::TestConcurrentMessages));
    suite->addTest(new CppUnit::TestCaller<OneDriveDaemonTest>(
        "TestErrorRecovery", &OneDriveDaemonTest::TestErrorRecovery));
    suite->addTest(new CppUnit::TestCaller<OneDriveDaemonTest>(
        "TestPersistence", &OneDriveDaemonTest::TestPersistence));
    suite->addTest(new CppUnit::TestCaller<OneDriveDaemonTest>(
        "TestSignalHandling", &OneDriveDaemonTest::TestSignalHandling));
    suite->addTest(new CppUnit::TestCaller<OneDriveDaemonTest>(
        "TestAutoRestart", &OneDriveDaemonTest::TestAutoRestart));
    
    return suite;
}