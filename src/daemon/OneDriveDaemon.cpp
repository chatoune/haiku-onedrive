#include "OneDriveDaemon.h"
#include "AttributeManager.h"
#include "../shared/OneDriveConstants.h"
#include "../api/OneDriveAPI.h"
#include "../api/AuthManager.h"
#include <syslog.h>
#include <stdio.h>
#include <Path.h>
#include <Directory.h>
#include <FindDirectory.h>

// Global daemon instance
OneDriveDaemon* gOneDriveDaemon = nullptr;

// Static settings keys
const char* OneDriveDaemon::kSettingSyncFolder = "sync_folder";
const char* OneDriveDaemon::kSettingSyncEnabled = "sync_enabled";
const char* OneDriveDaemon::kSettingCacheSize = "cache_size";
const char* OneDriveDaemon::kSettingBandwidthLimit = "bandwidth_limit";

OneDriveDaemon::OneDriveDaemon()
    : BApplication(APP_SIGNATURE),
      fSyncEngine(nullptr),
      fAttributeManager(nullptr),
      fCacheManager(nullptr),
      fAuthManager(nullptr),
      fNetworkMonitor(nullptr),
      fSyncState(SYNC_STOPPED),
      fNetworkAvailable(false),
      fInitialized(false)
{
    gOneDriveDaemon = this;
    _LogMessage("INFO", "OneDrive daemon starting");
}

OneDriveDaemon::~OneDriveDaemon()
{
    _LogMessage("INFO", "OneDrive daemon shutting down");
    _CleanupDaemon();
    gOneDriveDaemon = nullptr;
}

void
OneDriveDaemon::ReadyToRun()
{
    _LogMessage("INFO", "OneDrive daemon ready to run");
    _InitializeDaemon();
}

void
OneDriveDaemon::MessageReceived(BMessage* message)
{
    switch (message->what) {
        case MSG_START_SYNC:
            StartSync();
            break;
            
        case MSG_STOP_SYNC:
            StopSync();
            break;
            
        case MSG_PAUSE_SYNC:
            PauseSync();
            break;
            
        case MSG_FORCE_SYNC:
            ForceSync();
            break;
            
        case MSG_AUTHENTICATION_REQUIRED:
            _HandleAuthenticationRequired();
            break;
            
        case MSG_AUTHENTICATION_SUCCESS:
            _HandleAuthenticationSuccess(message);
            break;
            
        case MSG_AUTHENTICATION_FAILED:
            _HandleAuthenticationFailed(message);
            break;
            
        case MSG_NETWORK_STATUS_CHANGED:
            _HandleNetworkStatusChanged(message);
            break;
            
        case MSG_SYNC_STATUS_CHANGED:
            _HandleSyncStatusChanged(message);
            break;
            
        case MSG_FILE_CONFLICT:
            _HandleFileConflict(message);
            break;
            
        case MSG_SETTINGS_CHANGED:
            _HandleSettingsChanged(message);
            break;
            
        case MSG_SHUTDOWN_DAEMON:
            PostMessage(B_QUIT_REQUESTED);
            break;
            
        default:
            BApplication::MessageReceived(message);
            break;
    }
}

bool
OneDriveDaemon::QuitRequested()
{
    _LogMessage("INFO", "OneDrive daemon quit requested");
    _CleanupDaemon();
    return true;
}

void
OneDriveDaemon::AboutRequested()
{
    // TODO: Implement about dialog or info
    _LogMessage("INFO", "OneDrive daemon about requested");
}

// Daemon control methods
status_t
OneDriveDaemon::StartSync()
{
    _LogMessage("INFO", "Starting sync");
    if (fSyncState == SYNC_RUNNING) {
        _LogMessage("WARN", "Sync already running");
        return B_OK;
    }
    
    _UpdateSyncState(SYNC_STARTING);
    // TODO: Start sync engine
    _UpdateSyncState(SYNC_RUNNING);
    return B_OK;
}

status_t
OneDriveDaemon::StopSync()
{
    _LogMessage("INFO", "Stopping sync");
    if (fSyncState == SYNC_STOPPED) {
        _LogMessage("WARN", "Sync already stopped");
        return B_OK;
    }
    
    // TODO: Stop sync engine
    _UpdateSyncState(SYNC_STOPPED);
    return B_OK;
}

status_t
OneDriveDaemon::PauseSync()
{
    _LogMessage("INFO", "Pausing sync");
    if (fSyncState != SYNC_RUNNING) {
        _LogMessage("WARN", "Sync not running");
        return B_ERROR;
    }
    
    // TODO: Pause sync engine
    _UpdateSyncState(SYNC_PAUSED);
    return B_OK;
}

status_t
OneDriveDaemon::ForceSync()
{
    _LogMessage("INFO", "Forcing immediate sync");
    // TODO: Trigger immediate sync
    return B_OK;
}

bool
OneDriveDaemon::IsAuthenticated() const
{
    // TODO: Check authentication manager
    return false;
}

status_t
OneDriveDaemon::LoadSettings()
{
    _LogMessage("INFO", "Loading settings");
    // TODO: Load settings from file
    return B_OK;
}

status_t
OneDriveDaemon::SaveSettings()
{
    _LogMessage("INFO", "Saving settings");
    // TODO: Save settings to file
    return B_OK;
}

status_t
OneDriveDaemon::ApplySettings(const BMessage& settings)
{
    _LogMessage("INFO", "Applying settings");
    // TODO: Apply new settings
    return B_OK;
}

// Internal methods
void
OneDriveDaemon::_LogMessage(const char* level, const char* message)
{
    // Safety check for null pointers
    if (!level) level = "INFO";
    if (!message) message = "(null message)";
    
    // Log to both syslog and stdout for debugging
    int syslog_level = LOG_INFO;
    if (strcmp(level, "ERROR") == 0) syslog_level = LOG_ERR;
    else if (strcmp(level, "WARN") == 0) syslog_level = LOG_WARNING;
    else if (strcmp(level, "DEBUG") == 0) syslog_level = LOG_DEBUG;
    
    syslog(syslog_level, "OneDrive [%s]: %s", level, message);
    printf("OneDrive [%s]: %s\n", level, message);
}

void
OneDriveDaemon::_InitializeDaemon()
{
    if (fInitialized)
        return;
        
    _LogMessage("INFO", "Initializing OneDrive daemon");
    
    // Load settings first
    LoadSettings();
    
    // Initialize components (stubs for now)
    _InitializeComponents();
    
    fInitialized = true;
    _LogMessage("INFO", "OneDrive daemon initialized successfully");
}

void
OneDriveDaemon::_CleanupDaemon()
{
    if (!fInitialized)
        return;
        
    _LogMessage("INFO", "Cleaning up OneDrive daemon");
    
    // Stop sync if running
    if (fSyncState == SYNC_RUNNING || fSyncState == SYNC_PAUSED) {
        StopSync();
    }
    
    // Cleanup components
    _CleanupComponents();
    
    // Save final state
    SaveSettings();
    
    fInitialized = false;
}

void
OneDriveDaemon::_InitializeComponents()
{
    _LogMessage("INFO", "Initializing daemon components");
    
    // Initialize authentication manager
    fAuthManager = new AuthenticationManager();
    if (!fAuthManager) {
        _LogMessage("ERROR", "Failed to create authentication manager");
        return;
    }
    _LogMessage("INFO", "AuthenticationManager created successfully");
    
    // Initialize OneDrive API client (requires auth manager)
    OneDriveAPI* apiClient = new OneDriveAPI(*fAuthManager);
    if (!apiClient) {
        _LogMessage("ERROR", "Failed to create OneDrive API client");
        return;
    }
    _LogMessage("INFO", "OneDriveAPI created successfully");
    
    // Initialize attribute manager
    fAttributeManager = new AttributeManager();
    if (fAttributeManager && fAttributeManager->Initialize(apiClient) != B_OK) {
        _LogMessage("ERROR", "Failed to initialize attribute manager");
        delete fAttributeManager;
        fAttributeManager = nullptr;
    } else {
        _LogMessage("INFO", "AttributeManager initialized successfully");
        
        // Start monitoring the sync folder if it exists
        if (!fSyncFolder.IsEmpty()) {
            status_t result = fAttributeManager->StartMonitoring(fSyncFolder, true);
            if (result == B_OK) {
                _LogMessage("INFO", "Started attribute monitoring for sync folder");
            } else {
                _LogMessage("WARN", "Could not start attribute monitoring");
            }
        }
    }
    
    // TODO: Initialize sync engine
    // fSyncEngine = new OneDriveSyncEngine();
    
    // TODO: Initialize cache manager
    // fCacheManager = new CacheManager();
    
    // TODO: Initialize network monitor
    // fNetworkMonitor = new NetworkMonitor();
    
    _LogMessage("INFO", "Component initialization completed");
}

void
OneDriveDaemon::_CleanupComponents()
{
    _LogMessage("INFO", "Cleaning up daemon components");
    
    // Cleanup in reverse order of initialization
    // TODO: Cleanup network monitor
    // delete fNetworkMonitor; fNetworkMonitor = nullptr;
    
    // TODO: Cleanup cache manager
    // delete fCacheManager; fCacheManager = nullptr;
    
    // Cleanup attribute manager
    if (fAttributeManager) {
        fAttributeManager->Shutdown();
        delete fAttributeManager;
        fAttributeManager = nullptr;
        _LogMessage("INFO", "AttributeManager cleaned up");
    }
    
    // TODO: Cleanup sync engine
    // delete fSyncEngine; fSyncEngine = nullptr;
    
    // Cleanup authentication manager
    if (fAuthManager) {
        delete fAuthManager;
        fAuthManager = nullptr;
        _LogMessage("INFO", "AuthenticationManager cleaned up");
    }
    
    _LogMessage("INFO", "Component cleanup completed");
}

void
OneDriveDaemon::_UpdateSyncState(SyncState newState)
{
    if (fSyncState != newState) {
        fSyncState = newState;
        _LogMessage("INFO", "Sync state updated");
        
        // Notify observers of state change
        BMessage stateMsg(MSG_SYNC_STATUS_CHANGED);
        stateMsg.AddInt32("sync_state", (int32)newState);
        // TODO: Send to interested parties
    }
}

// Message handlers (stubs for now)
void
OneDriveDaemon::_HandleAuthenticationRequired()
{
    _LogMessage("INFO", "Authentication required");
    // TODO: Handle authentication requirement
}

void
OneDriveDaemon::_HandleAuthenticationSuccess(BMessage* message)
{
    _LogMessage("INFO", "Authentication succeeded");
    // TODO: Handle successful authentication
}

void
OneDriveDaemon::_HandleAuthenticationFailed(BMessage* message)
{
    _LogMessage("ERROR", "Authentication failed");
    // TODO: Handle authentication failure
}

void
OneDriveDaemon::_HandleNetworkStatusChanged(BMessage* message)
{
    bool available = false;
    if (message->FindBool("network_available", &available) == B_OK) {
        fNetworkAvailable = available;
        _LogMessage("INFO", available ? 
            "Network available" : 
            "Network unavailable");
    }
}

void
OneDriveDaemon::_HandleSyncStatusChanged(BMessage* message)
{
    _LogMessage("DEBUG", "Sync status changed");
    // TODO: Handle sync status changes
}

void
OneDriveDaemon::_HandleFileConflict(BMessage* message)
{
    _LogMessage("WARN", "File conflict detected");
    // TODO: Handle file conflicts
}

void
OneDriveDaemon::_HandleSettingsChanged(BMessage* message)
{
    _LogMessage("INFO", "Settings changed");
    // TODO: Handle settings changes
}