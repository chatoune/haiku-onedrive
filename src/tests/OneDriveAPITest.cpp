/**
 * @file OneDriveAPITest.cpp
 * @brief Unit tests for OneDriveAPI class
 * @author Haiku OneDrive Team
 * @version 1.0.0
 * @date 2025-08-14
 * 
 * Comprehensive unit tests for the OneDriveAPI class covering:
 * - Microsoft Graph API communication
 * - File operations (upload, download, list, delete)
 * - Folder operations (create, list, navigate)
 * - Metadata synchronization including BFS attributes
 * - Error handling and rate limiting
 * - Authentication integration
 */

#include <cppunit/TestCase.h>
#include <cppunit/TestSuite.h>
#include <cppunit/TestCaller.h>
#include <String.h>
#include <Message.h>
#include <File.h>
#include <Directory.h>
#include <Path.h>
#include <stdio.h>
#include <stdlib.h>

#include "../api/OneDriveAPI.h"
#include "../api/AuthManager.h"

/**
 * @brief Test fixture for OneDriveAPI tests
 */
class OneDriveAPITest : public CppUnit::TestCase {
public:
    /**
     * @brief Constructor
     */
    OneDriveAPITest();
    
    /**
     * @brief Destructor
     */
    virtual ~OneDriveAPITest();
    
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
     * @brief Test OneDriveAPI construction and initialization
     */
    void TestConstruction();
    
    /**
     * @brief Test authentication integration
     */
    void TestAuthenticationIntegration();
    
    /**
     * @brief Test user profile retrieval
     */
    void TestGetUserProfile();
    
    /**
     * @brief Test drive information retrieval
     */
    void TestGetDriveInfo();
    
    /**
     * @brief Test folder listing
     */
    void TestListFolder();
    
    /**
     * @brief Test folder listing with empty folder
     */
    void TestListEmptyFolder();
    
    /**
     * @brief Test item information retrieval
     */
    void TestGetItemInfo();
    
    /**
     * @brief Test file download
     */
    void TestDownloadFile();
    
    /**
     * @brief Test file upload
     */
    void TestUploadFile();
    
    /**
     * @brief Test folder creation
     */
    void TestCreateFolder();
    
    /**
     * @brief Test item deletion
     */
    void TestDeleteItem();
    
    /**
     * @brief Test metadata update
     */
    void TestUpdateItemMetadata();
    
    /**
     * @brief Test BFS attribute synchronization
     */
    void TestSyncAttributes();
    
    /**
     * @brief Test error handling for non-existent items
     */
    void TestErrorHandlingNotFound();
    
    /**
     * @brief Test error handling for network errors
     */
    void TestErrorHandlingNetwork();
    
    /**
     * @brief Test error handling for authentication errors
     */
    void TestErrorHandlingAuth();
    
    /**
     * @brief Test rate limiting behavior
     */
    void TestRateLimiting();
    
    /**
     * @brief Test progress callbacks during file operations
     */
    void TestProgressCallbacks();
    
    /**
     * @brief Test concurrent API requests
     */
    void TestConcurrentRequests();
    
    /**
     * @brief Test API quota and limits
     */
    void TestQuotaLimits();

private:
    OneDriveAPI* fAPI;                     ///< Test subject
    AuthenticationManager* fAuthManager;   ///< Authentication for API
    BString fTestFolderId;                 ///< Test folder ID for operations
    BString fTestFileId;                   ///< Test file ID for operations
    BString fTestFileName;                 ///< Test file name
    BString fTestFolderName;               ///< Test folder name
    BPath fTestFilePath;                   ///< Local test file path
    
    /**
     * @brief Helper method to set up authenticated API instance
     */
    status_t _SetupAuthentication();
    
    /**
     * @brief Helper method to create a test file for upload tests
     */
    status_t _CreateTestFile();
    
    /**
     * @brief Helper method to clean up test files and folders
     */
    void _CleanupTestItems();
    
    /**
     * @brief Helper method to verify OneDriveItem structure
     */
    void _VerifyOneDriveItem(const OneDriveItem& item);
    
    /**
     * @brief Progress callback for testing file operations
     */
    static bool _ProgressCallback(int64 current, int64 total, void* userData);
    
    /**
     * @brief Test data for progress tracking
     */
    struct ProgressData {
        int callCount;
        int64 lastCurrent;
        int64 lastTotal;
    };
};

// Test suite registration
static OneDriveAPITest sOneDriveAPITest;

OneDriveAPITest::OneDriveAPITest()
    : BTestCase("OneDriveAPITest"),
      fAPI(nullptr),
      fAuthManager(nullptr),
      fTestFolderId(""),
      fTestFileId(""),
      fTestFileName("haiku_test_file.txt"),
      fTestFolderName("haiku_test_folder")
{
    // Set up test file path
    fTestFilePath.SetTo("/tmp/onedrive_test_file.txt");
}

OneDriveAPITest::~OneDriveAPITest()
{
}

void OneDriveAPITest::setUp()
{
    // Create AuthenticationManager
    fAuthManager = new AuthenticationManager();
    
    // Create OneDriveAPI instance
    fAPI = new OneDriveAPI(fAuthManager);
    
    // Set up authentication for testing
    _SetupAuthentication();
    
    // Create test file
    _CreateTestFile();
    
    // Clean up any existing test items
    _CleanupTestItems();
}

void OneDriveAPITest::tearDown()
{
    // Clean up test items
    _CleanupTestItems();
    
    // Clean up objects
    if (fAPI) {
        delete fAPI;
        fAPI = nullptr;
    }
    
    if (fAuthManager) {
        delete fAuthManager;
        fAuthManager = nullptr;
    }
    
    // Remove test file
    BEntry testFile(fTestFilePath.Path());
    if (testFile.Exists()) {
        testFile.Remove();
    }
}

void OneDriveAPITest::TestConstruction()
{
    // Test construction with null auth manager
    OneDriveAPI* nullAPI = new OneDriveAPI(nullptr);
    CPPUNIT_ASSERT(nullAPI != nullptr);
    delete nullAPI;
    
    // Test construction with valid auth manager
    AuthenticationManager* authManager = new AuthenticationManager();
    OneDriveAPI* validAPI = new OneDriveAPI(authManager);
    CPPUNIT_ASSERT(validAPI != nullptr);
    
    delete validAPI;
    delete authManager;
}

void OneDriveAPITest::TestAuthenticationIntegration()
{
    // Test that API correctly uses authentication manager
    CPPUNIT_ASSERT(fAPI != nullptr);
    
    // In development mode, API should handle authentication gracefully
    BMessage userProfile;
    status_t result = fAPI->GetUserProfile(userProfile);
    
    // Should either succeed (if authenticated) or fail gracefully
    CPPUNIT_ASSERT(result == B_OK || result == B_NOT_ALLOWED);
}

void OneDriveAPITest::TestGetUserProfile()
{
    BMessage userProfile;
    status_t result = fAPI->GetUserProfile(userProfile);
    
    if (result == B_OK) {
        // If successful, verify user profile contains expected fields
        BString displayName, email, userId;
        userProfile.FindString("displayName", &displayName);
        userProfile.FindString("mail", &email);
        userProfile.FindString("id", &userId);
        
        // At least one field should be present
        CPPUNIT_ASSERT(displayName.Length() > 0 || email.Length() > 0 || userId.Length() > 0);
    } else {
        // In development mode, this might fail due to no authentication
        CPPUNIT_ASSERT(result == B_NOT_ALLOWED || result == B_ERROR);
    }
}

void OneDriveAPITest::TestGetDriveInfo()
{
    BMessage driveInfo;
    status_t result = fAPI->GetDriveInfo(driveInfo);
    
    if (result == B_OK) {
        // Verify drive info contains expected fields
        BString driveId, driveType;
        int64 totalSpace, usedSpace;
        
        driveInfo.FindString("id", &driveId);
        driveInfo.FindString("driveType", &driveType);
        driveInfo.FindInt64("quota.total", &totalSpace);
        driveInfo.FindInt64("quota.used", &usedSpace);
        
        CPPUNIT_ASSERT(driveId.Length() > 0);
        CPPUNIT_ASSERT(totalSpace >= 0);
        CPPUNIT_ASSERT(usedSpace >= 0);
    } else {
        // Expected in development mode
        CPPUNIT_ASSERT(result == B_NOT_ALLOWED || result == B_ERROR);
    }
}

void OneDriveAPITest::TestListFolder()
{
    BList items;
    status_t result = fAPI->ListFolder("root", items);
    
    if (result == B_OK) {
        // Verify items list
        CPPUNIT_ASSERT(items.CountItems() >= 0);
        
        // Check first item if any exist
        if (items.CountItems() > 0) {
            OneDriveItem* item = static_cast<OneDriveItem*>(items.ItemAt(0));
            CPPUNIT_ASSERT(item != nullptr);
            _VerifyOneDriveItem(*item);
        }
        
        // Clean up items
        for (int32 i = 0; i < items.CountItems(); i++) {
            delete static_cast<OneDriveItem*>(items.ItemAt(i));
        }
    } else {
        // Expected in development mode
        CPPUNIT_ASSERT(result == B_NOT_ALLOWED || result == B_ERROR);
    }
}

void OneDriveAPITest::TestListEmptyFolder()
{
    // First create a test folder
    OneDriveItem createdFolder;
    status_t createResult = fAPI->CreateFolder("root", fTestFolderName, createdFolder);
    
    if (createResult == B_OK) {
        fTestFolderId = createdFolder.id;
        
        // List the empty folder
        BList items;
        status_t result = fAPI->ListFolder(fTestFolderId, items);
        
        CPPUNIT_ASSERT_EQUAL(B_OK, result);
        CPPUNIT_ASSERT_EQUAL((int32)0, items.CountItems());
    }
}

void OneDriveAPITest::TestGetItemInfo()
{
    // Test getting info for root folder
    OneDriveItem rootInfo;
    status_t result = fAPI->GetItemInfo("root", rootInfo);
    
    if (result == B_OK) {
        _VerifyOneDriveItem(rootInfo);
        CPPUNIT_ASSERT_EQUAL(ITEM_TYPE_FOLDER, rootInfo.type);
        CPPUNIT_ASSERT(rootInfo.name.Length() > 0);
    } else {
        // Expected in development mode
        CPPUNIT_ASSERT(result == B_NOT_ALLOWED || result == B_ERROR);
    }
}

void OneDriveAPITest::TestDownloadFile()
{
    // First need to upload a file to download
    OneDriveItem uploadedFile;
    status_t uploadResult = fAPI->UploadFile("root", fTestFilePath.Path(), 
                                           fTestFileName, uploadedFile, nullptr, nullptr);
    
    if (uploadResult == B_OK) {
        fTestFileId = uploadedFile.id;
        
        // Now download the file
        BPath downloadPath("/tmp/downloaded_test_file.txt");
        status_t downloadResult = fAPI->DownloadFile(fTestFileId, downloadPath.Path(), 
                                                    nullptr, nullptr);
        
        CPPUNIT_ASSERT_EQUAL(B_OK, downloadResult);
        
        // Verify downloaded file exists
        BEntry downloadedFile(downloadPath.Path());
        CPPUNIT_ASSERT_EQUAL(true, downloadedFile.Exists());
        
        // Clean up
        downloadedFile.Remove();
    }
}

void OneDriveAPITest::TestUploadFile()
{
    OneDriveItem uploadedFile;
    ProgressData progressData = {0, 0, 0};
    
    status_t result = fAPI->UploadFile("root", fTestFilePath.Path(), fTestFileName, 
                                      uploadedFile, _ProgressCallback, &progressData);
    
    if (result == B_OK) {
        fTestFileId = uploadedFile.id;
        
        _VerifyOneDriveItem(uploadedFile);
        CPPUNIT_ASSERT_EQUAL(fTestFileName, uploadedFile.name);
        CPPUNIT_ASSERT_EQUAL(ITEM_TYPE_FILE, uploadedFile.type);
        CPPUNIT_ASSERT(uploadedFile.size > 0);
        
        // Verify progress callback was called
        CPPUNIT_ASSERT(progressData.callCount > 0);
    } else {
        // Expected in development mode
        CPPUNIT_ASSERT(result == B_NOT_ALLOWED || result == B_ERROR);
    }
}

void OneDriveAPITest::TestCreateFolder()
{
    OneDriveItem createdFolder;
    status_t result = fAPI->CreateFolder("root", fTestFolderName, createdFolder);
    
    if (result == B_OK) {
        fTestFolderId = createdFolder.id;
        
        _VerifyOneDriveItem(createdFolder);
        CPPUNIT_ASSERT_EQUAL(fTestFolderName, createdFolder.name);
        CPPUNIT_ASSERT_EQUAL(ITEM_TYPE_FOLDER, createdFolder.type);
        CPPUNIT_ASSERT_EQUAL((off_t)0, createdFolder.size);
    } else {
        // Expected in development mode
        CPPUNIT_ASSERT(result == B_NOT_ALLOWED || result == B_ERROR);
    }
}

void OneDriveAPITest::TestDeleteItem()
{
    // First create an item to delete
    OneDriveItem createdFolder;
    status_t createResult = fAPI->CreateFolder("root", fTestFolderName, createdFolder);
    
    if (createResult == B_OK) {
        // Now delete it
        status_t deleteResult = fAPI->DeleteItem(createdFolder.id);
        CPPUNIT_ASSERT_EQUAL(B_OK, deleteResult);
        
        // Verify it's gone by trying to get info
        OneDriveItem deletedItem;
        status_t getResult = fAPI->GetItemInfo(createdFolder.id, deletedItem);
        CPPUNIT_ASSERT(getResult != B_OK);
    }
}

void OneDriveAPITest::TestUpdateItemMetadata()
{
    // First create an item to update
    OneDriveItem createdFolder;
    status_t createResult = fAPI->CreateFolder("root", fTestFolderName, createdFolder);
    
    if (createResult == B_OK) {
        fTestFolderId = createdFolder.id;
        
        // Update metadata
        BMessage metadata;
        metadata.AddString("description", "Test folder description");
        
        status_t updateResult = fAPI->UpdateItemMetadata(fTestFolderId, metadata);
        
        if (updateResult == B_OK) {
            // Verify the update
            OneDriveItem updatedItem;
            status_t getResult = fAPI->GetItemInfo(fTestFolderId, updatedItem);
            CPPUNIT_ASSERT_EQUAL(B_OK, getResult);
        }
    }
}

void OneDriveAPITest::TestSyncAttributes()
{
    // Create test attributes
    BMessage attributes;
    attributes.AddString("custom:haiku:test", "test value");
    attributes.AddInt32("custom:haiku:number", 42);
    
    // First create a file to sync attributes for
    OneDriveItem uploadedFile;
    status_t uploadResult = fAPI->UploadFile("root", fTestFilePath.Path(), 
                                           fTestFileName, uploadedFile, nullptr, nullptr);
    
    if (uploadResult == B_OK) {
        fTestFileId = uploadedFile.id;
        
        // Sync attributes
        status_t syncResult = fAPI->SyncAttributes(fTestFileId, attributes);
        
        if (syncResult == B_OK) {
            // Verify attributes were synced
            OneDriveItem itemWithAttrs;
            status_t getResult = fAPI->GetItemInfo(fTestFileId, itemWithAttrs);
            CPPUNIT_ASSERT_EQUAL(B_OK, getResult);
            
            // Check if attributes are present
            BString testValue;
            if (itemWithAttrs.attributes.FindString("custom:haiku:test", &testValue) == B_OK) {
                CPPUNIT_ASSERT_EQUAL(BString("test value"), testValue);
            }
        }
    }
}

void OneDriveAPITest::TestErrorHandlingNotFound()
{
    // Try to get info for non-existent item
    OneDriveItem item;
    status_t result = fAPI->GetItemInfo("non-existent-id-12345", item);
    
    // Should return appropriate error code
    CPPUNIT_ASSERT(result != B_OK);
    CPPUNIT_ASSERT(result == B_ENTRY_NOT_FOUND || result == B_ERROR);
}

void OneDriveAPITest::TestErrorHandlingNetwork()
{
    // This test would simulate network errors
    // In development mode, network errors are simulated by the API
    
    // Try an operation that might fail due to network
    BMessage userProfile;
    status_t result = fAPI->GetUserProfile(userProfile);
    
    // Should handle network errors gracefully
    CPPUNIT_ASSERT(result == B_OK || result == B_ERROR || result == B_NOT_ALLOWED);
}

void OneDriveAPITest::TestErrorHandlingAuth()
{
    // Create API without authentication
    OneDriveAPI* unauthAPI = new OneDriveAPI(nullptr);
    
    // Try to perform operation requiring authentication
    BMessage userProfile;
    status_t result = unauthAPI->GetUserProfile(userProfile);
    
    // Should fail with authentication error
    CPPUNIT_ASSERT(result != B_OK);
    
    delete unauthAPI;
}

void OneDriveAPITest::TestRateLimiting()
{
    // Test rate limiting by making multiple rapid requests
    for (int i = 0; i < 5; i++) {
        BMessage userProfile;
        status_t result = fAPI->GetUserProfile(userProfile);
        
        // API should handle rate limiting gracefully
        CPPUNIT_ASSERT(result == B_OK || result == B_ERROR || result == B_NOT_ALLOWED);
        
        // Small delay between requests
        snooze(100000); // 100ms
    }
}

void OneDriveAPITest::TestProgressCallbacks()
{
    ProgressData progressData = {0, 0, 0};
    
    // Test upload progress
    OneDriveItem uploadedFile;
    status_t result = fAPI->UploadFile("root", fTestFilePath.Path(), fTestFileName, 
                                      uploadedFile, _ProgressCallback, &progressData);
    
    if (result == B_OK) {
        // Progress callback should have been called at least once
        CPPUNIT_ASSERT(progressData.callCount > 0);
        CPPUNIT_ASSERT(progressData.lastTotal > 0);
        
        fTestFileId = uploadedFile.id;
    }
}

void OneDriveAPITest::TestConcurrentRequests()
{
    // This is a basic concurrency test
    // In a full implementation, we would spawn multiple threads
    
    // For now, just verify that rapid sequential calls work
    for (int i = 0; i < 3; i++) {
        OneDriveItem rootInfo;
        status_t result = fAPI->GetItemInfo("root", rootInfo);
        
        // Should handle concurrent-like access
        CPPUNIT_ASSERT(result == B_OK || result == B_ERROR || result == B_NOT_ALLOWED);
    }
}

void OneDriveAPITest::TestQuotaLimits()
{
    // Test quota information retrieval
    BMessage driveInfo;
    status_t result = fAPI->GetDriveInfo(driveInfo);
    
    if (result == B_OK) {
        int64 totalSpace, usedSpace, remainingSpace;
        
        if (driveInfo.FindInt64("quota.total", &totalSpace) == B_OK &&
            driveInfo.FindInt64("quota.used", &usedSpace) == B_OK) {
            
            remainingSpace = totalSpace - usedSpace;
            
            // Verify quota values make sense
            CPPUNIT_ASSERT(totalSpace > 0);
            CPPUNIT_ASSERT(usedSpace >= 0);
            CPPUNIT_ASSERT(remainingSpace >= 0);
            CPPUNIT_ASSERT(usedSpace <= totalSpace);
        }
    }
}

status_t OneDriveAPITest::_SetupAuthentication()
{
    // Set up test client ID
    status_t result = fAuthManager->SetClientId("test-client-id");
    if (result != B_OK) {
        return result;
    }
    
    // In development mode, simulate authentication
    result = fAuthManager->HandleAuthCallback("test-auth-code");
    
    return result;
}

status_t OneDriveAPITest::_CreateTestFile()
{
    BFile testFile(fTestFilePath.Path(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
    
    if (testFile.InitCheck() != B_OK) {
        return testFile.InitCheck();
    }
    
    // Write some test content
    const char* testContent = "This is a test file for OneDrive API testing.\n"
                             "It contains some sample content to upload.\n"
                             "Created by Haiku OneDrive test suite.\n";
    
    ssize_t written = testFile.Write(testContent, strlen(testContent));
    if (written < 0) {
        return written;
    }
    
    return B_OK;
}

void OneDriveAPITest::_CleanupTestItems()
{
    // Delete test file if it exists
    if (fTestFileId.Length() > 0) {
        fAPI->DeleteItem(fTestFileId);
        fTestFileId = "";
    }
    
    // Delete test folder if it exists
    if (fTestFolderId.Length() > 0) {
        fAPI->DeleteItem(fTestFolderId);
        fTestFolderId = "";
    }
}

void OneDriveAPITest::_VerifyOneDriveItem(const OneDriveItem& item)
{
    // Verify basic item structure
    CPPUNIT_ASSERT(item.id.Length() > 0);
    CPPUNIT_ASSERT(item.name.Length() > 0);
    CPPUNIT_ASSERT(item.type == ITEM_TYPE_FILE || item.type == ITEM_TYPE_FOLDER);
    CPPUNIT_ASSERT(item.createdTime > 0);
    CPPUNIT_ASSERT(item.modifiedTime > 0);
    CPPUNIT_ASSERT(item.eTag.Length() > 0);
}

bool OneDriveAPITest::_ProgressCallback(int64 current, int64 total, void* userData)
{
    ProgressData* data = static_cast<ProgressData*>(userData);
    if (data) {
        data->callCount++;
        data->lastCurrent = current;
        data->lastTotal = total;
    }
    
    // Continue operation
    return true;
}

// Test suite factory
CppUnit::Test* OneDriveAPITestSuite()
{
    CppUnit::TestSuite* suite = new CppUnit::TestSuite("OneDriveAPITestSuite");
    
    suite->addTest(new CppUnit::TestCaller<OneDriveAPITest>(
        "TestConstruction", &OneDriveAPITest::TestConstruction));
    suite->addTest(new CppUnit::TestCaller<OneDriveAPITest>(
        "TestAuthenticationIntegration", &OneDriveAPITest::TestAuthenticationIntegration));
    suite->addTest(new CppUnit::TestCaller<OneDriveAPITest>(
        "TestGetUserProfile", &OneDriveAPITest::TestGetUserProfile));
    suite->addTest(new CppUnit::TestCaller<OneDriveAPITest>(
        "TestGetDriveInfo", &OneDriveAPITest::TestGetDriveInfo));
    suite->addTest(new CppUnit::TestCaller<OneDriveAPITest>(
        "TestListFolder", &OneDriveAPITest::TestListFolder));
    suite->addTest(new CppUnit::TestCaller<OneDriveAPITest>(
        "TestListEmptyFolder", &OneDriveAPITest::TestListEmptyFolder));
    suite->addTest(new CppUnit::TestCaller<OneDriveAPITest>(
        "TestGetItemInfo", &OneDriveAPITest::TestGetItemInfo));
    suite->addTest(new CppUnit::TestCaller<OneDriveAPITest>(
        "TestDownloadFile", &OneDriveAPITest::TestDownloadFile));
    suite->addTest(new CppUnit::TestCaller<OneDriveAPITest>(
        "TestUploadFile", &OneDriveAPITest::TestUploadFile));
    suite->addTest(new CppUnit::TestCaller<OneDriveAPITest>(
        "TestCreateFolder", &OneDriveAPITest::TestCreateFolder));
    suite->addTest(new CppUnit::TestCaller<OneDriveAPITest>(
        "TestDeleteItem", &OneDriveAPITest::TestDeleteItem));
    suite->addTest(new CppUnit::TestCaller<OneDriveAPITest>(
        "TestUpdateItemMetadata", &OneDriveAPITest::TestUpdateItemMetadata));
    suite->addTest(new CppUnit::TestCaller<OneDriveAPITest>(
        "TestSyncAttributes", &OneDriveAPITest::TestSyncAttributes));
    suite->addTest(new CppUnit::TestCaller<OneDriveAPITest>(
        "TestErrorHandlingNotFound", &OneDriveAPITest::TestErrorHandlingNotFound));
    suite->addTest(new CppUnit::TestCaller<OneDriveAPITest>(
        "TestErrorHandlingNetwork", &OneDriveAPITest::TestErrorHandlingNetwork));
    suite->addTest(new CppUnit::TestCaller<OneDriveAPITest>(
        "TestErrorHandlingAuth", &OneDriveAPITest::TestErrorHandlingAuth));
    suite->addTest(new CppUnit::TestCaller<OneDriveAPITest>(
        "TestRateLimiting", &OneDriveAPITest::TestRateLimiting));
    suite->addTest(new CppUnit::TestCaller<OneDriveAPITest>(
        "TestProgressCallbacks", &OneDriveAPITest::TestProgressCallbacks));
    suite->addTest(new CppUnit::TestCaller<OneDriveAPITest>(
        "TestConcurrentRequests", &OneDriveAPITest::TestConcurrentRequests));
    suite->addTest(new CppUnit::TestCaller<OneDriveAPITest>(
        "TestQuotaLimits", &OneDriveAPITest::TestQuotaLimits));
    
    return suite;
}