/**
 * @file OneDriveDaemon.h
 * @brief Main OneDrive daemon class for Haiku OS
 * @author Haiku OneDrive Team
 * @version 1.0.0
 * @date 2025-08-14
 * 
 * This file contains the main OneDriveDaemon class which serves as the core
 * background service for OneDrive synchronization on Haiku OS.
 */

#ifndef ONEDRIVE_DAEMON_H
#define ONEDRIVE_DAEMON_H

#include <Application.h>
#include <Message.h>
#include <String.h>
#include <Looper.h>
#include <Catalog.h>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "OneDriveDaemon"

// Forward declarations for component classes
class OneDriveSyncEngine;    ///< Handles file synchronization logic
class AttributeManager;      ///< Manages BFS attribute synchronization
class CacheManager;          ///< Handles local file caching
class AuthenticationManager; ///< Manages OAuth2 authentication
class NetworkMonitor;        ///< Monitors network connectivity

/**
 * @brief Message constants for inter-component communication
 * 
 * These message constants are used for communication between the daemon
 * and other components, as well as for internal state management.
 */
enum {
    MSG_START_SYNC = 'stsy',              ///< Request to start synchronization
    MSG_STOP_SYNC = 'spsy',               ///< Request to stop synchronization
    MSG_PAUSE_SYNC = 'pasy',              ///< Request to pause synchronization
    MSG_FORCE_SYNC = 'fosy',              ///< Request immediate synchronization
    MSG_AUTHENTICATION_REQUIRED = 'aurq', ///< Authentication is required
    MSG_AUTHENTICATION_SUCCESS = 'ausc',  ///< Authentication succeeded
    MSG_AUTHENTICATION_FAILED = 'aufl',   ///< Authentication failed
    MSG_NETWORK_STATUS_CHANGED = 'nsch',  ///< Network connectivity changed
    MSG_SYNC_STATUS_CHANGED = 'ssch',     ///< Sync status has changed
    MSG_FILE_CONFLICT = 'flcf',           ///< File conflict detected
    MSG_SETTINGS_CHANGED = 'stch',        ///< Configuration settings changed
    MSG_SHUTDOWN_DAEMON = 'shdn'          ///< Request daemon shutdown
};

/**
 * @brief Enumeration of possible synchronization states
 * 
 * Represents the current state of the synchronization engine.
 */
enum SyncState {
    SYNC_STOPPED = 0,   ///< Synchronization is stopped
    SYNC_STARTING,      ///< Synchronization is starting up
    SYNC_RUNNING,       ///< Synchronization is actively running
    SYNC_PAUSED,        ///< Synchronization is paused
    SYNC_ERROR          ///< Synchronization encountered an error
};

/**
 * @brief Main OneDrive daemon class for Haiku OS
 * 
 * The OneDriveDaemon class serves as the core background service for OneDrive
 * synchronization on Haiku OS. It extends BApplication to provide a standard
 * Haiku application framework while running as a system daemon.
 * 
 * Key responsibilities:
 * - Manage authentication with Microsoft OneDrive
 * - Handle bidirectional file synchronization
 * - Synchronize Haiku BFS extended attributes
 * - Manage local file caching
 * - Provide system integration through Haiku APIs
 * 
 * @see BApplication
 * @see OneDriveSyncEngine
 * @see AuthenticationManager
 * @since 1.0.0
 */
class OneDriveDaemon : public BApplication {
public:
    /**
     * @brief Default constructor
     * 
     * Initializes the daemon with the application signature and sets up
     * initial state variables. Component initialization is deferred until
     * ReadyToRun() is called.
     */
    OneDriveDaemon();
    
    /**
     * @brief Destructor
     * 
     * Cleans up all components and ensures graceful shutdown of the daemon.
     */
    virtual ~OneDriveDaemon();
    
    // BApplication overrides
    
    /**
     * @brief Handle incoming BMessage objects
     * 
     * Processes messages for sync control, authentication events, network
     * status changes, and configuration updates.
     * 
     * @param message The BMessage to process
     * @see BApplication::MessageReceived()
     */
    virtual void MessageReceived(BMessage* message) override;
    
    /**
     * @brief Called when the application is ready to run
     * 
     * Performs daemon initialization including component setup and
     * settings loading.
     * 
     * @see BApplication::ReadyToRun()
     */
    virtual void ReadyToRun() override;
    
    /**
     * @brief Handle quit requests
     * 
     * Performs cleanup operations before allowing the daemon to quit.
     * 
     * @return true if the daemon can quit, false to cancel the quit request
     * @see BApplication::QuitRequested()
     */
    virtual bool QuitRequested() override;
    
    /**
     * @brief Handle about requests
     * 
     * Responds to requests for daemon information.
     * 
     * @see BApplication::AboutRequested()
     */
    virtual void AboutRequested() override;
    
    // Daemon control methods
    
    /**
     * @brief Start the synchronization process
     * 
     * Initiates file synchronization between local storage and OneDrive.
     * This method is idempotent - calling it multiple times while sync
     * is already running will not cause errors.
     * 
     * @return B_OK on success, error code on failure
     * @retval B_OK Synchronization started successfully
     * @retval B_NOT_ALLOWED Authentication required
     * @retval B_NO_MEMORY Insufficient resources
     */
    status_t StartSync();
    
    /**
     * @brief Stop the synchronization process
     * 
     * Halts all synchronization activities and saves current state.
     * 
     * @return B_OK on success, error code on failure
     */
    status_t StopSync();
    
    /**
     * @brief Pause the synchronization process
     * 
     * Temporarily suspends synchronization without stopping it completely.
     * Synchronization can be resumed by calling StartSync().
     * 
     * @return B_OK on success, B_ERROR if sync is not running
     */
    status_t PauseSync();
    
    /**
     * @brief Force immediate synchronization
     * 
     * Triggers an immediate sync operation, bypassing normal scheduling.
     * Useful for responding to user requests or critical changes.
     * 
     * @return B_OK on success, error code on failure
     */
    status_t ForceSync();
    
    // Status query methods
    
    /**
     * @brief Get current synchronization state
     * 
     * @return The current SyncState value
     * @see SyncState
     */
    SyncState GetSyncState() const { return fSyncState; }
    
    /**
     * @brief Check if user is authenticated with OneDrive
     * 
     * @return true if authenticated, false otherwise
     */
    bool IsAuthenticated() const;
    
    /**
     * @brief Check network connectivity status
     * 
     * @return true if network is available, false otherwise
     */
    bool IsNetworkAvailable() const { return fNetworkAvailable; }
    
    // Settings management methods
    
    /**
     * @brief Load configuration settings from disk
     * 
     * Loads daemon settings from the standard Haiku settings location.
     * 
     * @return B_OK on success, error code on failure
     */
    status_t LoadSettings();
    
    /**
     * @brief Save configuration settings to disk
     * 
     * Persists current daemon settings to the standard Haiku settings location.
     * 
     * @return B_OK on success, error code on failure
     */
    status_t SaveSettings();
    
    /**
     * @brief Apply new configuration settings
     * 
     * Updates daemon behavior based on new settings provided in a BMessage.
     * 
     * @param settings BMessage containing new configuration values
     * @return B_OK on success, error code on failure
     */
    status_t ApplySettings(const BMessage& settings);
    
private:
    /// @name Core Components
    /// @{
    OneDriveSyncEngine*     fSyncEngine;        ///< Handles file synchronization logic
    AttributeManager*       fAttributeManager;  ///< Manages BFS attribute synchronization
    CacheManager*          fCacheManager;       ///< Handles local file caching
    AuthenticationManager* fAuthManager;        ///< Manages OAuth2 authentication
    NetworkMonitor*        fNetworkMonitor;     ///< Monitors network connectivity
    /// @}
    
    /// @name State Management Variables
    /// @{
    SyncState               fSyncState;         ///< Current synchronization state
    bool                    fNetworkAvailable;  ///< Whether network is currently available
    bool                    fInitialized;       ///< Whether daemon has been initialized
    BString                 fSyncFolder;        ///< Path to local OneDrive sync folder
    /// @}
    
    /// @name Internal Methods
    /// @{
    
    /**
     * @brief Log a message to system log and console
     * 
     * @param level Log level (INFO, WARN, ERROR, DEBUG)
     * @param message The message to log
     */
    void _LogMessage(const char* level, const char* message);
    
    /**
     * @brief Initialize the daemon on startup
     * 
     * Called during ReadyToRun() to set up all daemon components.
     */
    void _InitializeDaemon();
    
    /**
     * @brief Clean up daemon resources on shutdown
     * 
     * Called during QuitRequested() to ensure graceful shutdown.
     */
    void _CleanupDaemon();
    
    /**
     * @brief Initialize all daemon components
     * 
     * Creates and sets up sync engine, auth manager, cache manager, etc.
     */
    void _InitializeComponents();
    
    /**
     * @brief Clean up all daemon components
     * 
     * Destroys component instances and frees resources.
     */
    void _CleanupComponents();
    
    /**
     * @brief Handle authentication requirement notification
     * 
     * Called when authentication is needed to access OneDrive.
     */
    void _HandleAuthenticationRequired();
    
    /**
     * @brief Handle successful authentication
     * 
     * @param message BMessage containing authentication details
     */
    void _HandleAuthenticationSuccess(BMessage* message);
    
    /**
     * @brief Handle authentication failure
     * 
     * @param message BMessage containing error details
     */
    void _HandleAuthenticationFailed(BMessage* message);
    
    /**
     * @brief Handle network connectivity changes
     * 
     * @param message BMessage containing network status information
     */
    void _HandleNetworkStatusChanged(BMessage* message);
    
    /**
     * @brief Handle sync status changes
     * 
     * @param message BMessage containing sync status details
     */
    void _HandleSyncStatusChanged(BMessage* message);
    
    /**
     * @brief Handle file conflict notifications
     * 
     * @param message BMessage containing conflict details
     */
    void _HandleFileConflict(BMessage* message);
    
    /**
     * @brief Handle configuration changes
     * 
     * @param message BMessage containing new settings
     */
    void _HandleSettingsChanged(BMessage* message);
    
    /**
     * @brief Update the current sync state
     * 
     * Updates internal state and notifies observers of the change.
     * 
     * @param newState The new synchronization state
     */
    void _UpdateSyncState(SyncState newState);
    
    /// @}
    
    /// @name Settings Keys
    /// Static constants for configuration setting names
    /// @{
    static const char* kSettingSyncFolder;      ///< Setting key for sync folder path
    static const char* kSettingSyncEnabled;     ///< Setting key for sync enable/disable
    static const char* kSettingCacheSize;       ///< Setting key for cache size limit
    static const char* kSettingBandwidthLimit;  ///< Setting key for bandwidth throttling
    /// @}
};

/**
 * @brief Global daemon instance pointer
 * 
 * Provides global access to the daemon instance for signal handlers
 * and other system integration points.
 */
extern OneDriveDaemon* gOneDriveDaemon;

#endif // ONEDRIVE_DAEMON_H