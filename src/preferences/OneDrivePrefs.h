/**
 * @file OneDrivePrefs.h
 * @brief OneDrive preferences application for account management and settings
 * @author Haiku OneDrive Team
 * @version 1.0.0
 * @date 2025-08-14
 * 
 * This file contains the OneDrivePrefs application class which provides a native
 * Haiku preferences interface for configuring OneDrive synchronization, managing
 * user accounts, and adjusting sync settings.
 */

#ifndef ONEDRIVE_PREFS_H
#define ONEDRIVE_PREFS_H

#include <Application.h>
#include <Window.h>
#include <View.h>
#include <TabView.h>
#include <Button.h>
#include <TextControl.h>
#include <CheckBox.h>
#include <StringView.h>
#include <FilePanel.h>
#include <Path.h>
#include <Directory.h>
#include <ScrollView.h>
#include <ListView.h>
#include <StatusBar.h>
#include <GroupLayout.h>
#include <CardLayout.h>
#include <Locker.h>
#include <Message.h>
#include <StringList.h>

// Forward declarations
class AuthenticationManager;
class OneDriveAPI;

/**
 * @brief Preference window message constants
 */
enum PrefsMessages {
    MSG_AUTHENTICATE = 'auth',          ///< Start authentication process
    MSG_LOGOUT = 'logo',                ///< Logout from OneDrive account
    MSG_SELECT_FOLDER = 'sfld',         ///< Select sync folder
    MSG_FOLDER_SELECTED = 'fsel',       ///< Folder selection completed
    MSG_SETTINGS_CHANGED = 'schg',      ///< Settings value changed
    MSG_APPLY_SETTINGS = 'appl',        ///< Apply settings changes
    MSG_REVERT_SETTINGS = 'revt',       ///< Revert settings to saved values
    MSG_TEST_CONNECTION = 'test',       ///< Test OneDrive connection
    MSG_AUTH_COMPLETED = 'acmp',        ///< Authentication completed
    MSG_AUTH_FAILED = 'afld',           ///< Authentication failed
    MSG_ADD_OFFLINE_FOLDER = 'aoff',    ///< Add folder to offline list
    MSG_REMOVE_OFFLINE_FOLDER = 'roff', ///< Remove folder from offline list
    MSG_OFFLINE_FOLDER_SELECTED = 'ofsl' ///< Offline folder selection completed
};

/**
 * @brief Authentication status states
 */
enum AuthStatus {
    AUTH_STATUS_NOT_CONNECTED = 0,      ///< Not authenticated
    AUTH_STATUS_CONNECTING,             ///< Authentication in progress
    AUTH_STATUS_CONNECTED,              ///< Successfully authenticated
    AUTH_STATUS_ERROR                   ///< Authentication error
};

/**
 * @brief OneDrive preferences window
 * 
 * The OneDrivePrefsWindow class provides a tabbed interface for configuring
 * OneDrive synchronization settings. It includes account management, sync
 * folder selection, and various synchronization options.
 * 
 * Features:
 * - OAuth2 authentication flow integration
 * - Sync folder selection with file panel
 * - Bandwidth and sync frequency settings
 * - Account information display
 * - Real-time connection testing
 * - Settings persistence across sessions
 * 
 * The window follows Haiku's interface guidelines with proper layout
 * management, keyboard navigation, and native look and feel.
 * 
 * @see OneDrivePrefs
 * @see AuthenticationManager
 * @see OneDriveAPI
 * @since 1.0.0
 */
class OneDrivePrefsWindow : public BWindow {
public:
    /**
     * @brief Constructor
     * 
     * Creates the preferences window with all necessary UI components
     * and initializes the connection to authentication and API managers.
     * 
     * @param authManager Reference to authentication manager
     * @param apiClient Reference to OneDrive API client
     */
    OneDrivePrefsWindow(AuthenticationManager& authManager, OneDriveAPI& apiClient);
    
    /**
     * @brief Destructor
     */
    virtual ~OneDrivePrefsWindow();
    
    // BWindow overrides
    
    /**
     * @brief Handle window messages
     * 
     * Processes authentication, settings, and UI interaction messages.
     * 
     * @param message Received BMessage
     */
    virtual void MessageReceived(BMessage* message);
    
    /**
     * @brief Handle window close request
     * 
     * Saves settings and performs cleanup before closing.
     * 
     * @return true to allow close, false to prevent
     */
    virtual bool QuitRequested();

private:
    /// @name UI Construction
    /// @{
    
    /**
     * @brief Build the main UI interface
     * 
     * Creates the tabbed interface with all preference panels.
     */
    void _BuildUI();
    
    /**
     * @brief Create account management tab
     * 
     * @return BView containing account management interface
     */
    BView* _CreateAccountTab();
    
    /**
     * @brief Create sync settings tab
     * 
     * @return BView containing sync configuration interface
     */
    BView* _CreateSyncTab();
    
    /**
     * @brief Create advanced settings tab
     * 
     * @return BView containing advanced options
     */
    BView* _CreateAdvancedTab();
    
    /// @}
    
    /// @name Authentication Handling
    /// @{
    
    /**
     * @brief Start OAuth2 authentication process
     * 
     * Initiates the authentication flow and updates UI accordingly.
     */
    void _StartAuthentication();
    
    /**
     * @brief Handle authentication completion
     * 
     * @param success Whether authentication was successful
     * @param errorMessage Error message if authentication failed
     */
    void _HandleAuthCompletion(bool success, const BString& errorMessage = "");
    
    /**
     * @brief Logout from OneDrive account
     * 
     * Clears stored credentials and updates UI to unauthenticated state.
     */
    void _Logout();
    
    /**
     * @brief Update authentication status display
     * 
     * @param status Current authentication status
     * @param message Status message to display
     */
    void _UpdateAuthStatus(AuthStatus status, const BString& message = "");
    
    /**
     * @brief Load and display user account information
     */
    void _LoadAccountInfo();
    
    /// @}
    
    /// @name Settings Management
    /// @{
    
    /**
     * @brief Load settings from storage
     * 
     * Retrieves stored preferences and populates UI controls.
     */
    void _LoadSettings();
    
    /**
     * @brief Save current settings to storage
     * 
     * Persists current UI state to Haiku settings.
     */
    void _SaveSettings();
    
    /**
     * @brief Apply settings changes
     * 
     * Validates and applies current settings, enabling/disabling
     * controls as appropriate.
     */
    void _ApplySettings();
    
    /**
     * @brief Revert settings to last saved state
     */
    void _RevertSettings();
    
    /**
     * @brief Check if settings have been modified
     * 
     * @return true if settings differ from saved values
     */
    bool _SettingsModified() const;
    
    /// @}
    
    /// @name Folder Selection
    /// @{
    
    /**
     * @brief Show folder selection dialog
     * 
     * Opens a file panel for selecting the sync folder location.
     */
    void _SelectSyncFolder();
    
    /**
     * @brief Handle folder selection completion
     * 
     * @param message BMessage containing selected folder path
     */
    void _HandleFolderSelection(BMessage* message);
    
    /**
     * @brief Validate selected sync folder
     * 
     * @param path Path to validate
     * @return true if folder is valid for synchronization
     */
    bool _ValidateSyncFolder(const BPath& path);
    
    /// @}
    
    /// @name UI Updates
    /// @{
    
    /**
     * @brief Update UI based on authentication state
     * 
     * Enables/disables controls based on whether user is authenticated.
     */
    void _UpdateUIForAuthState();
    
    /**
     * @brief Update sync folder display
     * 
     * @param path Currently selected sync folder path
     */
    void _UpdateSyncFolderDisplay(const BPath& path);
    
    /**
     * @brief Test connection to OneDrive
     * 
     * Performs a connectivity test and displays results.
     */
    void _TestConnection();
    
    /// @}
    
    /// @name Offline Folder Management
    /// @{
    
    /**
     * @brief Add a folder to the offline list
     * 
     * Opens a file panel to select a folder to keep offline.
     */
    void _AddOfflineFolder();
    
    /**
     * @brief Remove selected folder from offline list
     * 
     * Removes the currently selected folder from the offline list.
     */
    void _RemoveOfflineFolder();
    
    /**
     * @brief Handle offline folder selection
     * 
     * @param message BMessage containing selected folder path
     */
    void _HandleOfflineFolderSelection(BMessage* message);
    
    /**
     * @brief Update offline folder list display
     * 
     * Refreshes the list view with current offline folders.
     */
    void _UpdateOfflineFoldersList();
    
    /**
     * @brief Load offline folders from settings
     * 
     * Retrieves the list of offline folders from storage.
     */
    void _LoadOfflineFolders();
    
    /**
     * @brief Save offline folders to settings
     * 
     * Persists the list of offline folders to storage.
     */
    void _SaveOfflineFolders();
    
    /// @}
    
    /// @name Member Variables - External Dependencies
    /// @{
    
    AuthenticationManager&  fAuthManager;      ///< Authentication manager reference
    OneDriveAPI&           fAPIClient;         ///< OneDrive API client reference
    BLocker                fLock;              ///< Thread safety lock
    
    /// @}
    
    /// @name Member Variables - UI Components
    /// @{
    
    BTabView*              fTabView;           ///< Main tabbed interface
    
    // Account tab components
    BView*                 fAccountTab;        ///< Account management tab
    BStringView*           fStatusLabel;       ///< Authentication status display
    BButton*               fAuthButton;        ///< Authenticate/Logout button
    BStringView*           fUserNameLabel;     ///< User name display
    BStringView*           fUserEmailLabel;    ///< User email display
    BStringView*           fQuotaLabel;        ///< Storage quota display
    BButton*               fTestButton;        ///< Test connection button
    
    // Sync tab components  
    BView*                 fSyncTab;           ///< Sync settings tab
    BTextControl*          fSyncFolderControl; ///< Sync folder path control
    BButton*               fSelectFolderButton; ///< Select folder button
    BCheckBox*             fAutoSyncCheckBox;  ///< Enable automatic sync
    BCheckBox*             fSyncAttributesBox; ///< Sync BFS attributes
    BTextControl*          fSyncIntervalControl; ///< Sync interval setting
    
    // Advanced tab components
    BView*                 fAdvancedTab;       ///< Advanced settings tab
    BTextControl*          fBandwidthLimit;    ///< Bandwidth limit setting
    BCheckBox*             fSyncHiddenFiles;   ///< Sync hidden files option
    BCheckBox*             fSyncOnlyWiFi;      ///< Sync only on WiFi
    BButton*               fResetButton;       ///< Reset to defaults button
    
    // Offline folders components
    BListView*             fOfflineFoldersList; ///< List of offline folders
    BScrollView*           fOfflineScrollView;  ///< Scroll view for offline list
    BButton*               fAddOfflineButton;   ///< Add offline folder button
    BButton*               fRemoveOfflineButton;///< Remove offline folder button
    
    // Common buttons
    BButton*               fApplyButton;       ///< Apply changes button
    BButton*               fRevertButton;      ///< Revert changes button
    
    /// @}
    
    /// @name Member Variables - Settings State
    /// @{
    
    BPath                  fSyncFolderPath;    ///< Selected sync folder path
    AuthStatus             fAuthStatus;        ///< Current authentication status
    BFilePanel*            fFilePanel;         ///< Folder selection panel
    BFilePanel*            fOfflineFolderPanel;///< Offline folder selection panel
    bool                   fSettingsModified;  ///< Whether settings have changes
    BStringList            fOfflineFolders;    ///< List of offline folder paths
    
    /// @}
    
    /// @name Static Constants
    /// @{
    
    static const char* kSettingsFileName;      ///< Settings file name
    static const char* kDefaultSyncFolder;     ///< Default sync folder name
    static const int32 kDefaultSyncInterval;   ///< Default sync interval (minutes)
    static const int32 kDefaultBandwidth;      ///< Default bandwidth limit (KB/s)
    
    /// @}
};

/**
 * @brief OneDrive preferences application
 * 
 * The OneDrivePrefs application class manages the preferences window and
 * handles application lifecycle for the OneDrive configuration interface.
 * It integrates with the OneDrive daemon components and provides a native
 * Haiku preferences experience.
 * 
 * Features:
 * - Single-instance application (prevents multiple preference windows)
 * - Integration with system preferences architecture
 * - Proper application signature and MIME handling
 * - Resource management and cleanup
 * - Message passing with OneDrive daemon
 * 
 * The application follows Haiku conventions for preferences applications
 * and integrates seamlessly with the system preferences panel.
 * 
 * @see OneDrivePrefsWindow
 * @see AuthenticationManager
 * @see OneDriveAPI
 * @since 1.0.0
 */
class OneDrivePrefs : public BApplication {
public:
    /**
     * @brief Constructor
     * 
     * Initializes the preferences application with proper signature
     * and creates necessary manager objects.
     */
    OneDrivePrefs();
    
    /**
     * @brief Destructor
     */
    virtual ~OneDrivePrefs();
    
    // BApplication overrides
    
    /**
     * @brief Application startup
     * 
     * Creates the preferences window and initializes components.
     */
    virtual void ReadyToRun();
    
    /**
     * @brief Handle application messages
     * 
     * Processes inter-application communication and system messages.
     * 
     * @param message Received BMessage
     */
    virtual void MessageReceived(BMessage* message);
    
    /**
     * @brief Check if application should quit
     * 
     * @return true to allow quit, false to prevent
     */
    virtual bool QuitRequested();

private:
    /// @name Initialization
    /// @{
    
    /**
     * @brief Initialize application components
     * 
     * Creates authentication manager, API client, and other dependencies.
     * 
     * @return B_OK on success, error code on failure
     */
    status_t _InitializeComponents();
    
    /**
     * @brief Cleanup application resources
     */
    void _Cleanup();
    
    /// @}
    
    /// @name Member Variables
    /// @{
    
    OneDrivePrefsWindow*   fPrefsWindow;       ///< Main preferences window
    AuthenticationManager* fAuthManager;       ///< Authentication manager
    OneDriveAPI*           fAPIClient;         ///< OneDrive API client
    
    /// @}
    
    /// @name Static Constants
    /// @{
    
    static const char* kApplicationSignature;  ///< Application MIME signature
    
    /// @}
};

#endif // ONEDRIVE_PREFS_H