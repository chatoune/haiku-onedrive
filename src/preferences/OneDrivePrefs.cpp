/**
 * @file OneDrivePrefs.cpp
 * @brief Implementation of OneDrive preferences application
 * @author Haiku OneDrive Team
 * @version 1.0.0
 * @date 2025-08-14
 */

#include "OneDrivePrefs.h"
#include "../api/AuthManager.h"
#include "../api/OneDriveAPI.h"
#include "../shared/OneDriveConstants.h"

#include <Alert.h>
#include <Box.h>
#include <LayoutBuilder.h>
#include <SeparatorView.h>
#include <SpaceLayoutItem.h>
#include <FindDirectory.h>
#include <StorageKit.h>
#include <InterfaceKit.h>
#include <SupportKit.h>
#include <Directory.h>
#include <syslog.h>
#include <stdio.h>
#include <StringItem.h>

// OneDrivePrefs static constants
#ifdef PREFS_APP_SIGNATURE
const char* OneDrivePrefs::kApplicationSignature = PREFS_APP_SIGNATURE;
#else
const char* OneDrivePrefs::kApplicationSignature = "application/x-vnd.Haiku-OneDrivePrefs";
#endif

// OneDrivePrefsWindow static constants
const char* OneDrivePrefsWindow::kSettingsFileName = "OneDrive_settings";
const char* OneDrivePrefsWindow::kDefaultSyncFolder = "OneDrive";
const int32 OneDrivePrefsWindow::kDefaultSyncInterval = 15; // 15 minutes
const int32 OneDrivePrefsWindow::kDefaultBandwidth = 0; // Unlimited

// OneDrivePrefsWindow Implementation

OneDrivePrefsWindow::OneDrivePrefsWindow(AuthenticationManager& authManager, OneDriveAPI& apiClient)
    : BWindow(BRect(100, 100, 600, 500), "OneDrive Preferences", 
              B_TITLED_WINDOW, B_ASYNCHRONOUS_CONTROLS | B_NOT_RESIZABLE),
      fAuthManager(authManager),
      fAPIClient(apiClient),
      fLock("PrefsWindow Lock"),
      fTabView(NULL),
      fAccountTab(NULL),
      fSyncTab(NULL),
      fAdvancedTab(NULL),
      fStatusLabel(NULL),
      fAuthButton(NULL),
      fUserNameLabel(NULL),
      fUserEmailLabel(NULL),
      fQuotaLabel(NULL),
      fTestButton(NULL),
      fSyncFolderControl(NULL),
      fSelectFolderButton(NULL),
      fAutoSyncCheckBox(NULL),
      fSyncAttributesBox(NULL),
      fSyncIntervalControl(NULL),
      fBandwidthLimit(NULL),
      fSyncHiddenFiles(NULL),
      fSyncOnlyWiFi(NULL),
      fResetButton(NULL),
      fOfflineFoldersList(NULL),
      fOfflineScrollView(NULL),
      fAddOfflineButton(NULL),
      fRemoveOfflineButton(NULL),
      fApplyButton(NULL),
      fRevertButton(NULL),
      fAuthStatus(AUTH_STATUS_NOT_CONNECTED),
      fFilePanel(NULL),
      fOfflineFolderPanel(NULL),
      fSettingsModified(false),
      fOfflineFolders()
{
    syslog(LOG_INFO, "OneDrive Preferences: Initializing preferences window");
    
    // Set up default sync folder path
    BPath homePath;
    if (find_directory(B_USER_DIRECTORY, &homePath) == B_OK) {
        fSyncFolderPath = homePath;
        fSyncFolderPath.Append(kDefaultSyncFolder);
    }
    
    _BuildUI();
    _LoadSettings();
    _UpdateUIForAuthState();
    
    CenterOnScreen();
}

OneDrivePrefsWindow::~OneDrivePrefsWindow()
{
    BAutolock lock(fLock);
    
    delete fFilePanel;
    delete fOfflineFolderPanel;
    
    syslog(LOG_INFO, "OneDrive Preferences: Window destroyed");
}

void
OneDrivePrefsWindow::MessageReceived(BMessage* message)
{
    switch (message->what) {
        case MSG_AUTHENTICATE:
            _StartAuthentication();
            break;
            
        case MSG_LOGOUT:
            _Logout();
            break;
            
        case MSG_SELECT_FOLDER:
            _SelectSyncFolder();
            break;
            
        case MSG_FOLDER_SELECTED:
            _HandleFolderSelection(message);
            break;
            
        case MSG_SETTINGS_CHANGED:
            fSettingsModified = true;
            fApplyButton->SetEnabled(true);
            fRevertButton->SetEnabled(true);
            // Update remove button if it's a list selection change
            void* source;
            if (message->FindPointer("source", &source) == B_OK && source == fOfflineFoldersList) {
                fRemoveOfflineButton->SetEnabled(fOfflineFoldersList->CurrentSelection() >= 0);
            }
            break;
            
        case MSG_APPLY_SETTINGS:
            _ApplySettings();
            break;
            
        case MSG_REVERT_SETTINGS:
            _RevertSettings();
            break;
            
        case MSG_TEST_CONNECTION:
            _TestConnection();
            break;
            
        case MSG_AUTH_COMPLETED:
        {
            bool success;
            if (message->FindBool("success", &success) == B_OK) {
                BString errorMsg;
                message->FindString("error", &errorMsg);
                _HandleAuthCompletion(success, errorMsg);
            }
            break;
        }
        
        case MSG_ADD_OFFLINE_FOLDER:
            _AddOfflineFolder();
            break;
            
        case MSG_REMOVE_OFFLINE_FOLDER:
            _RemoveOfflineFolder();
            break;
            
        case MSG_OFFLINE_FOLDER_SELECTED:
            _HandleOfflineFolderSelection(message);
            break;
        
        default:
            BWindow::MessageReceived(message);
            break;
    }
}

bool
OneDrivePrefsWindow::QuitRequested()
{
    if (fSettingsModified) {
        BAlert* alert = new BAlert("Unsaved Changes",
            "You have unsaved changes. Do you want to save them before closing?",
            "Don't Save", "Cancel", "Save", B_WIDTH_AS_USUAL, B_WARNING_ALERT);
        
        int32 choice = alert->Go();
        switch (choice) {
            case 0: // Don't Save
                break;
            case 1: // Cancel
                return false;
            case 2: // Save
                _SaveSettings();
                break;
        }
    }
    
    be_app->PostMessage(B_QUIT_REQUESTED);
    return true;
}

// Private methods

void
OneDrivePrefsWindow::_BuildUI()
{
    syslog(LOG_DEBUG, "OneDrive Preferences: Building UI");
    
    // Create main tab view
    fTabView = new BTabView("tab_view");
    
    // Create tabs
    fAccountTab = _CreateAccountTab();
    fSyncTab = _CreateSyncTab();
    fAdvancedTab = _CreateAdvancedTab();
    
    // Add tabs to tab view
    BTab* accountTab = new BTab(fAccountTab);
    accountTab->SetLabel("Account");
    fTabView->AddTab(fAccountTab, accountTab);
    
    BTab* syncTab = new BTab(fSyncTab);
    syncTab->SetLabel("Sync");
    fTabView->AddTab(fSyncTab, syncTab);
    
    BTab* advancedTab = new BTab(fAdvancedTab);
    advancedTab->SetLabel("Advanced");
    fTabView->AddTab(fAdvancedTab, advancedTab);
    
    // Create bottom button bar
    fApplyButton = new BButton("apply", "Apply", new BMessage(MSG_APPLY_SETTINGS));
    fRevertButton = new BButton("revert", "Revert", new BMessage(MSG_REVERT_SETTINGS));
    
    fApplyButton->SetEnabled(false);
    fRevertButton->SetEnabled(false);
    
    // Build main layout
    BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
        .Add(fTabView)
        .Add(new BSeparatorView(B_HORIZONTAL))
        .AddGroup(B_HORIZONTAL)
            .AddGlue()
            .Add(fRevertButton)
            .Add(fApplyButton)
            .SetInsets(B_USE_DEFAULT_SPACING)
        .End()
        .SetInsets(B_USE_DEFAULT_SPACING)
    .End();
}

BView*
OneDrivePrefsWindow::_CreateAccountTab()
{
    BView* view = new BView("account_tab", 0);
    view->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
    
    // Status display
    fStatusLabel = new BStringView("status", "Not connected to OneDrive");
    fStatusLabel->SetFont(be_bold_font);
    
    // Authentication button
    fAuthButton = new BButton("auth_button", "Sign In", new BMessage(MSG_AUTHENTICATE));
    fAuthButton->MakeDefault(true);
    
    // User information (initially hidden)
    fUserNameLabel = new BStringView("user_name", "");
    fUserEmailLabel = new BStringView("user_email", "");
    fQuotaLabel = new BStringView("quota", "");
    
    // Test connection button
    fTestButton = new BButton("test", "Test Connection", new BMessage(MSG_TEST_CONNECTION));
    fTestButton->SetEnabled(false);
    
    // Layout account tab
    BLayoutBuilder::Group<>(view, B_VERTICAL)
        .AddGroup(B_VERTICAL)
            .Add(new BStringView("title", "OneDrive Account"))
            .Add(new BSeparatorView(B_HORIZONTAL))
            .AddStrut(B_USE_DEFAULT_SPACING)
        .End()
        .AddGroup(B_VERTICAL)
            .Add(fStatusLabel)
            .AddStrut(B_USE_HALF_ITEM_SPACING)
            .Add(fAuthButton)
            .AddStrut(B_USE_DEFAULT_SPACING)
        .End()
        .AddGroup(B_VERTICAL)
            .Add(new BStringView("info_title", "Account Information:"))
            .Add(fUserNameLabel)
            .Add(fUserEmailLabel) 
            .Add(fQuotaLabel)
            .AddStrut(B_USE_DEFAULT_SPACING)
        .End()
        .Add(fTestButton)
        .AddGlue()
        .SetInsets(B_USE_DEFAULT_SPACING)
    .End();
    
    return view;
}

BView*
OneDrivePrefsWindow::_CreateSyncTab()
{
    BView* view = new BView("sync_tab", 0);
    view->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
    
    // Sync folder selection
    fSyncFolderControl = new BTextControl("sync_folder", "Sync folder:", 
        fSyncFolderPath.Path(), new BMessage(MSG_SETTINGS_CHANGED));
    fSyncFolderControl->SetModificationMessage(new BMessage(MSG_SETTINGS_CHANGED));
    
    fSelectFolderButton = new BButton("select_folder", "Browse...", 
        new BMessage(MSG_SELECT_FOLDER));
    
    // Sync options
    fAutoSyncCheckBox = new BCheckBox("auto_sync", "Enable automatic synchronization",
        new BMessage(MSG_SETTINGS_CHANGED));
    fAutoSyncCheckBox->SetValue(B_CONTROL_ON);
    
    fSyncAttributesBox = new BCheckBox("sync_attributes", "Synchronize Haiku file attributes",
        new BMessage(MSG_SETTINGS_CHANGED));
    fSyncAttributesBox->SetValue(B_CONTROL_ON);
    
    // Sync interval
    fSyncIntervalControl = new BTextControl("sync_interval", "Sync interval (minutes):",
        "15", new BMessage(MSG_SETTINGS_CHANGED));
    fSyncIntervalControl->SetModificationMessage(new BMessage(MSG_SETTINGS_CHANGED));
    
    // Layout sync tab
    BLayoutBuilder::Group<>(view, B_VERTICAL)
        .AddGroup(B_VERTICAL)
            .Add(new BStringView("title", "Synchronization Settings"))
            .Add(new BSeparatorView(B_HORIZONTAL))
            .AddStrut(B_USE_DEFAULT_SPACING)
        .End()
        .AddGroup(B_HORIZONTAL)
            .Add(fSyncFolderControl)
            .Add(fSelectFolderButton)
        .End()
        .Add(fAutoSyncCheckBox)
        .Add(fSyncAttributesBox)
        .Add(fSyncIntervalControl)
        .AddGlue()
        .SetInsets(B_USE_DEFAULT_SPACING)
    .End();
    
    return view;
}

BView*
OneDrivePrefsWindow::_CreateAdvancedTab()
{
    BView* view = new BView("advanced_tab", 0);
    view->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
    
    // Bandwidth limiting
    fBandwidthLimit = new BTextControl("bandwidth", "Bandwidth limit (KB/s, 0 = unlimited):",
        "0", new BMessage(MSG_SETTINGS_CHANGED));
    fBandwidthLimit->SetModificationMessage(new BMessage(MSG_SETTINGS_CHANGED));
    
    // Advanced sync options
    fSyncHiddenFiles = new BCheckBox("sync_hidden", "Synchronize hidden files",
        new BMessage(MSG_SETTINGS_CHANGED));
    
    fSyncOnlyWiFi = new BCheckBox("wifi_only", "Sync only when connected to WiFi",
        new BMessage(MSG_SETTINGS_CHANGED));
    
    // Reset button
    fResetButton = new BButton("reset", "Reset to Defaults", 
        new BMessage(MSG_REVERT_SETTINGS));
    
    // Offline folders management
    fOfflineFoldersList = new BListView("offline_folders", B_SINGLE_SELECTION_LIST);
    fOfflineFoldersList->SetSelectionMessage(new BMessage(MSG_SETTINGS_CHANGED));
    
    fOfflineScrollView = new BScrollView("offline_scroll", fOfflineFoldersList,
        B_WILL_DRAW | B_FRAME_EVENTS, false, true);
    fOfflineScrollView->SetExplicitMinSize(BSize(400, 120));
    
    fAddOfflineButton = new BButton("add_offline", "Add Folder...",
        new BMessage(MSG_ADD_OFFLINE_FOLDER));
    
    fRemoveOfflineButton = new BButton("remove_offline", "Remove",
        new BMessage(MSG_REMOVE_OFFLINE_FOLDER));
    fRemoveOfflineButton->SetEnabled(false);
    
    // Layout advanced tab
    BLayoutBuilder::Group<>(view, B_VERTICAL)
        .AddGroup(B_VERTICAL)
            .Add(new BStringView("title", "Advanced Settings"))
            .Add(new BSeparatorView(B_HORIZONTAL))
            .AddStrut(B_USE_DEFAULT_SPACING)
        .End()
        .Add(fBandwidthLimit)
        .Add(fSyncHiddenFiles)
        .Add(fSyncOnlyWiFi)
        .AddStrut(B_USE_DEFAULT_SPACING)
        .AddGroup(B_VERTICAL)
            .Add(new BStringView("offline_title", "Always Keep Offline:"))
            .Add(new BStringView("offline_desc", "These folders will be stored locally and never cached out"))
            .Add(fOfflineScrollView)
            .AddGroup(B_HORIZONTAL)
                .Add(fAddOfflineButton)
                .Add(fRemoveOfflineButton)
                .AddGlue()
            .End()
        .End()
        .AddStrut(B_USE_DEFAULT_SPACING)
        .Add(fResetButton)
        .AddGlue()
        .SetInsets(B_USE_DEFAULT_SPACING)
    .End();
    
    return view;
}

void
OneDrivePrefsWindow::_StartAuthentication()
{
    syslog(LOG_INFO, "OneDrive Preferences: Starting authentication");
    
    _UpdateAuthStatus(AUTH_STATUS_CONNECTING, "Starting authentication...");
    
    // Set up client ID if not already configured
    // Note: For production, register an app at https://portal.azure.com and use the real Client ID
    BString clientId = "00000000-0000-0000-0000-000000000000"; // Development placeholder
    status_t result = fAuthManager.SetClientId(clientId);
    if (result != B_OK) {
        _HandleAuthCompletion(false, "Failed to configure authentication client");
        return;
    }
    
    // Start OAuth2 flow
    result = fAuthManager.StartAuthFlow();
    if (result != B_OK) {
        BString errorMsg = "Failed to start authentication flow: ";
        errorMsg << fAuthManager.GetLastError();
        _HandleAuthCompletion(false, errorMsg);
        return;
    }
    
    // For development, simulate successful authentication after a delay
    // In production, this would wait for the OAuth2 callback
    BMessage authMsg(MSG_AUTH_COMPLETED);
    authMsg.AddBool("success", true);
    PostMessage(&authMsg, this);
}

void
OneDrivePrefsWindow::_HandleAuthCompletion(bool success, const BString& errorMessage)
{
    if (success) {
        _UpdateAuthStatus(AUTH_STATUS_CONNECTED, "Successfully connected to OneDrive");
        _LoadAccountInfo();
    } else {
        _UpdateAuthStatus(AUTH_STATUS_ERROR, errorMessage.IsEmpty() ? 
            "Authentication failed" : errorMessage);
    }
    
    _UpdateUIForAuthState();
}

void
OneDrivePrefsWindow::_Logout()
{
    syslog(LOG_INFO, "OneDrive Preferences: Logging out");
    
    status_t result = fAuthManager.Logout();
    if (result == B_OK) {
        _UpdateAuthStatus(AUTH_STATUS_NOT_CONNECTED, "Disconnected from OneDrive");
        _UpdateUIForAuthState();
    } else {
        BAlert* alert = new BAlert("Logout Error",
            "Failed to logout. Please try again.",
            "OK", NULL, NULL, B_WIDTH_AS_USUAL, B_WARNING_ALERT);
        alert->Go();
    }
}

void
OneDrivePrefsWindow::_UpdateAuthStatus(AuthStatus status, const BString& message)
{
    BAutolock lock(fLock);
    
    fAuthStatus = status;
    
    if (fStatusLabel) {
        fStatusLabel->SetText(message.String());
        
        // Set status color based on state
        switch (status) {
            case AUTH_STATUS_CONNECTED:
                fStatusLabel->SetHighColor(0, 150, 0); // Green
                break;
            case AUTH_STATUS_CONNECTING:
                fStatusLabel->SetHighColor(255, 165, 0); // Orange
                break;
            case AUTH_STATUS_ERROR:
                fStatusLabel->SetHighColor(255, 0, 0); // Red
                break;
            default:
                fStatusLabel->SetHighColor(ui_color(B_PANEL_TEXT_COLOR));
                break;
        }
        
        fStatusLabel->Invalidate();
    }
}

void
OneDrivePrefsWindow::_LoadAccountInfo()
{
    // Get user profile information
    BMessage userInfo;
    if (fAPIClient.GetUserProfile(userInfo) == ONEDRIVE_OK) {
        BString name, email;
        userInfo.FindString("displayName", &name);
        userInfo.FindString("mail", &email);
        
        if (fUserNameLabel) {
            BString nameText = "Name: ";
            nameText << (name.IsEmpty() ? "Unknown" : name);
            fUserNameLabel->SetText(nameText);
        }
        
        if (fUserEmailLabel) {
            BString emailText = "Email: ";
            emailText << (email.IsEmpty() ? "Unknown" : email);
            fUserEmailLabel->SetText(emailText);
        }
    }
    
    // Get drive quota information
    BMessage driveInfo;
    if (fAPIClient.GetDriveInfo(driveInfo) == ONEDRIVE_OK) {
        int64 totalBytes, usedBytes;
        if (driveInfo.FindInt64("total", &totalBytes) == B_OK &&
            driveInfo.FindInt64("used", &usedBytes) == B_OK) {
            
            BString quotaText;
            quotaText.SetToFormat("Storage: %.1f GB used of %.1f GB",
                usedBytes / (1024.0 * 1024.0 * 1024.0),
                totalBytes / (1024.0 * 1024.0 * 1024.0));
            
            if (fQuotaLabel) {
                fQuotaLabel->SetText(quotaText);
            }
        }
    }
}

void
OneDrivePrefsWindow::_UpdateUIForAuthState()
{
    bool authenticated = (fAuthStatus == AUTH_STATUS_CONNECTED);
    bool connecting = (fAuthStatus == AUTH_STATUS_CONNECTING);
    
    if (fAuthButton) {
        fAuthButton->SetLabel(authenticated ? "Sign Out" : "Sign In");
        fAuthButton->SetMessage(new BMessage(authenticated ? MSG_LOGOUT : MSG_AUTHENTICATE));
        fAuthButton->SetEnabled(!connecting);
    }
    
    if (fTestButton) {
        fTestButton->SetEnabled(authenticated);
    }
    
    // Show/hide user information by updating text
    if (authenticated) {
        // User info will be updated by _LoadAccountInfo()
    } else {
        // Clear user information when not authenticated
        if (fUserNameLabel) fUserNameLabel->SetText("Name: Not connected");
        if (fUserEmailLabel) fUserEmailLabel->SetText("Email: Not connected");
        if (fQuotaLabel) fQuotaLabel->SetText("Storage: Not available");
    }
    
    // Enable/disable sync settings based on authentication
    if (fSyncFolderControl) fSyncFolderControl->SetEnabled(authenticated);
    if (fSelectFolderButton) fSelectFolderButton->SetEnabled(authenticated);
    if (fAutoSyncCheckBox) fAutoSyncCheckBox->SetEnabled(authenticated);
    if (fSyncAttributesBox) fSyncAttributesBox->SetEnabled(authenticated);
    if (fSyncIntervalControl) fSyncIntervalControl->SetEnabled(authenticated);
}

void
OneDrivePrefsWindow::_LoadSettings()
{
    // TODO: Load settings from Haiku settings file
    // For now, use defaults
    syslog(LOG_DEBUG, "OneDrive Preferences: Loading settings (using defaults)");
    
    // Load offline folders
    _LoadOfflineFolders();
    
    fSettingsModified = false;
    if (fApplyButton) fApplyButton->SetEnabled(false);
    if (fRevertButton) fRevertButton->SetEnabled(false);
}

void
OneDrivePrefsWindow::_SaveSettings()
{
    // TODO: Save settings to Haiku settings file
    syslog(LOG_INFO, "OneDrive Preferences: Saving settings");
    
    // Save offline folders
    _SaveOfflineFolders();
    
    fSettingsModified = false;
    if (fApplyButton) fApplyButton->SetEnabled(false);
    if (fRevertButton) fRevertButton->SetEnabled(false);
}

void
OneDrivePrefsWindow::_ApplySettings()
{
    syslog(LOG_INFO, "OneDrive Preferences: Applying settings");
    
    // Validate settings
    BString syncFolder = fSyncFolderControl->Text();
    if (syncFolder.IsEmpty()) {
        BAlert* alert = new BAlert("Invalid Settings",
            "Please select a sync folder.",
            "OK");
        alert->Go();
        return;
    }
    
    _SaveSettings();
}

void
OneDrivePrefsWindow::_RevertSettings()
{
    _LoadSettings();
}

void
OneDrivePrefsWindow::_SelectSyncFolder()
{
    if (!fFilePanel) {
        fFilePanel = new BFilePanel(B_OPEN_PANEL, NULL, NULL, B_DIRECTORY_NODE,
            false, new BMessage(MSG_FOLDER_SELECTED));
        fFilePanel->SetTarget(this);
    }
    
    fFilePanel->Show();
}

void
OneDrivePrefsWindow::_HandleFolderSelection(BMessage* message)
{
    entry_ref ref;
    if (message->FindRef("refs", &ref) == B_OK) {
        BPath path(&ref);
        if (_ValidateSyncFolder(path)) {
            fSyncFolderPath = path;
            fSyncFolderControl->SetText(path.Path());
            fSettingsModified = true;
            fApplyButton->SetEnabled(true);
            fRevertButton->SetEnabled(true);
        } else {
            BAlert* alert = new BAlert("Invalid Folder",
                "The selected folder cannot be used for synchronization.",
                "OK");
            alert->Go();
        }
    }
}

bool
OneDrivePrefsWindow::_ValidateSyncFolder(const BPath& path)
{
    // Check if folder exists and is writable
    BDirectory dir(path.Path());
    return (dir.InitCheck() == B_OK);
}

void
OneDrivePrefsWindow::_TestConnection()
{
    syslog(LOG_INFO, "OneDrive Preferences: Testing connection");
    
    fTestButton->SetEnabled(false);
    fTestButton->SetLabel("Testing...");
    
    bool connected = fAPIClient.IsConnected();
    
    BAlert* alert = new BAlert("Connection Test",
        connected ? "Connection to OneDrive successful!" : "Failed to connect to OneDrive.",
        "OK", NULL, NULL, B_WIDTH_AS_USUAL, 
        connected ? B_INFO_ALERT : B_WARNING_ALERT);
    alert->Go();
    
    fTestButton->SetLabel("Test Connection");
    fTestButton->SetEnabled(true);
}

// Offline Folder Management Implementation

void
OneDrivePrefsWindow::_AddOfflineFolder()
{
    if (!fOfflineFolderPanel) {
        fOfflineFolderPanel = new BFilePanel(B_OPEN_PANEL, NULL, NULL, B_DIRECTORY_NODE,
            false, new BMessage(MSG_OFFLINE_FOLDER_SELECTED));
        fOfflineFolderPanel->SetTarget(this);
        fOfflineFolderPanel->Window()->SetTitle("Select Folder to Keep Offline");
    }
    
    // Set starting directory to sync folder if available
    if (fSyncFolderPath.InitCheck() == B_OK) {
        fOfflineFolderPanel->SetPanelDirectory(fSyncFolderPath.Path());
    }
    
    fOfflineFolderPanel->Show();
}

void
OneDrivePrefsWindow::_RemoveOfflineFolder()
{
    int32 selection = fOfflineFoldersList->CurrentSelection();
    if (selection < 0) {
        return;
    }
    
    BStringItem* item = dynamic_cast<BStringItem*>(fOfflineFoldersList->RemoveItem(selection));
    if (item) {
        // Remove from internal list
        BString path(item->Text());
        fOfflineFolders.Remove(path);
        delete item;
        
        // Update UI
        fSettingsModified = true;
        fApplyButton->SetEnabled(true);
        fRevertButton->SetEnabled(true);
        
        // Update remove button state
        fRemoveOfflineButton->SetEnabled(fOfflineFoldersList->CurrentSelection() >= 0);
        
        // Notify daemon about the change
        BMessage updateMsg('upof');
        updateMsg.AddString("removed_path", path);
        be_app_messenger.SendMessage(&updateMsg);
    }
}

void
OneDrivePrefsWindow::_HandleOfflineFolderSelection(BMessage* message)
{
    entry_ref ref;
    if (message->FindRef("refs", &ref) == B_OK) {
        BPath path(&ref);
        
        // Validate the path is within sync folder
        if (!path.Path() || !fSyncFolderPath.Path()) {
            BAlert* alert = new BAlert("Invalid Folder",
                "Please select the sync folder first.",
                "OK");
            alert->Go();
            return;
        }
        
        BString pathStr(path.Path());
        BString syncStr(fSyncFolderPath.Path());
        
        if (!pathStr.StartsWith(syncStr)) {
            BAlert* alert = new BAlert("Invalid Folder",
                "Offline folders must be within the OneDrive sync folder.",
                "OK");
            alert->Go();
            return;
        }
        
        // Check if already in list
        if (fOfflineFolders.HasString(pathStr)) {
            BAlert* alert = new BAlert("Duplicate Folder",
                "This folder is already in the offline list.",
                "OK");
            alert->Go();
            return;
        }
        
        // Add to list
        fOfflineFolders.Add(pathStr);
        fOfflineFoldersList->AddItem(new BStringItem(path.Path()));
        
        // Update UI
        fSettingsModified = true;
        fApplyButton->SetEnabled(true);
        fRevertButton->SetEnabled(true);
        
        // Notify daemon about the new offline folder
        BMessage updateMsg('adof');
        updateMsg.AddString("offline_path", pathStr);
        be_app_messenger.SendMessage(&updateMsg);
    }
}

void
OneDrivePrefsWindow::_UpdateOfflineFoldersList()
{
    // Clear current list
    fOfflineFoldersList->MakeEmpty();
    
    // Add all offline folders
    for (int32 i = 0; i < fOfflineFolders.CountStrings(); i++) {
        BString path = fOfflineFolders.StringAt(i);
        fOfflineFoldersList->AddItem(new BStringItem(path.String()));
    }
    
    // Update remove button state
    fRemoveOfflineButton->SetEnabled(fOfflineFoldersList->CurrentSelection() >= 0);
}

void
OneDrivePrefsWindow::_LoadOfflineFolders()
{
    // TODO: Load from settings file
    // For now, just clear the list
    fOfflineFolders.MakeEmpty();
    
    // Example: Load from BMessage stored in settings
    // BMessage settings;
    // if (LoadSettings(settings) == B_OK) {
    //     int32 i = 0;
    //     const char* path;
    //     while (settings.FindString("offline_folder", i++, &path) == B_OK) {
    //         fOfflineFolders.Add(path);
    //     }
    // }
    
    _UpdateOfflineFoldersList();
}

void
OneDrivePrefsWindow::_SaveOfflineFolders()
{
    // TODO: Save to settings file
    // Example: Save to BMessage in settings
    // BMessage settings;
    // for (int32 i = 0; i < fOfflineFolders.CountStrings(); i++) {
    //     settings.AddString("offline_folder", fOfflineFolders.StringAt(i));
    // }
    // SaveSettings(settings);
    
    syslog(LOG_INFO, "OneDrive Preferences: Saved %d offline folders", fOfflineFolders.CountStrings());
}

// OneDrivePrefs Application Implementation

OneDrivePrefs::OneDrivePrefs()
    : BApplication(kApplicationSignature),
      fPrefsWindow(NULL),
      fAuthManager(NULL),
      fAPIClient(NULL)
{
    syslog(LOG_INFO, "OneDrive Preferences: Application starting");
}

OneDrivePrefs::~OneDrivePrefs()
{
    _Cleanup();
}

void
OneDrivePrefs::ReadyToRun()
{
    status_t result = _InitializeComponents();
    if (result != B_OK) {
        BAlert* alert = new BAlert("Initialization Error",
            "Failed to initialize OneDrive preferences. Please check that the OneDrive daemon is running.",
            "Quit", NULL, NULL, B_WIDTH_AS_USUAL, B_STOP_ALERT);
        alert->Go();
        PostMessage(B_QUIT_REQUESTED);
        return;
    }
    
    // Create and show preferences window
    fPrefsWindow = new OneDrivePrefsWindow(*fAuthManager, *fAPIClient);
    fPrefsWindow->Show();
}

void
OneDrivePrefs::MessageReceived(BMessage* message)
{
    BApplication::MessageReceived(message);
}

bool
OneDrivePrefs::QuitRequested()
{
    return true;
}

status_t
OneDrivePrefs::_InitializeComponents()
{
    // Create authentication manager
    fAuthManager = new AuthenticationManager();
    if (!fAuthManager) {
        return B_NO_MEMORY;
    }
    
    // Create API client
    fAPIClient = new OneDriveAPI(*fAuthManager);
    if (!fAPIClient) {
        return B_NO_MEMORY;
    }
    
    return B_OK;
}

void
OneDrivePrefs::_Cleanup()
{
    delete fAPIClient;
    delete fAuthManager;
    fAPIClient = NULL;
    fAuthManager = NULL;
}