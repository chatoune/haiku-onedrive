/**
 * @file VirtualFolder.h
 * @brief Virtual folder representation for OneDrive sync folders
 * @author Haiku OneDrive Team
 * @version 1.0.0
 * @date 2025-08-15
 * 
 * This class represents OneDrive folders in the local filesystem, providing
 * virtual file management, sync state tracking, and Tracker integration.
 * It monitors filesystem changes and coordinates with the sync engine.
 */

#ifndef ONEDRIVE_VIRTUAL_FOLDER_H
#define ONEDRIVE_VIRTUAL_FOLDER_H

#include <Directory.h>
#include <Entry.h>
#include <Handler.h>
#include <List.h>
#include <Locker.h>
#include <Message.h>
#include <NodeMonitor.h>
#include <Path.h>
#include <String.h>
#include <Autolock.h>

#include "../shared/OneDriveConstants.h"

class OneDriveDaemon;
class SyncStateTracker;

/**
 * @brief Sync state for individual files and folders
 * 
 * Tracks the current synchronization status of items in the OneDrive folder.
 * Used for visual indicators and sync decision making.
 */
enum OneDriveSyncState {
    kSyncStateUnknown = 0,      ///< Initial state, not yet checked
    kSyncStateSynced,           ///< Fully synchronized with cloud
    kSyncStateSyncing,          ///< Currently synchronizing
    kSyncStateError,            ///< Sync error occurred
    kSyncStatePending,          ///< Waiting to sync
    kSyncStateConflict,         ///< Conflict detected
    kSyncStateOffline,          ///< Available offline
    kSyncStateOnlineOnly,       ///< Cloud-only, not downloaded
    kSyncStateIgnored           ///< Excluded from sync
};

/**
 * @brief Information about a virtual file or folder
 * 
 * Contains metadata about items in the OneDrive folder, including
 * their sync state, cloud ID, and modification times.
 */
struct VirtualItem {
    entry_ref       ref;            ///< Local filesystem reference
    BString         cloudId;        ///< OneDrive item ID
    OneDriveSyncState state;        ///< Current sync state
    off_t           size;           ///< File size (0 for folders)
    time_t          localModTime;   ///< Local modification time
    time_t          cloudModTime;   ///< Cloud modification time
    bool            isFolder;       ///< True if item is a folder
    bool            isPinned;       ///< True if pinned for offline access
    BString         eTag;           ///< OneDrive ETag for change detection
    uint32          attributes;     ///< Cached file attributes
};

/**
 * @brief Virtual folder representation for OneDrive sync
 * 
 * Manages a local folder that syncs with OneDrive, tracking file states,
 * handling filesystem notifications, and coordinating with the sync engine.
 * Provides integration with Haiku's Tracker for visual sync indicators.
 * 
 * @see OneDriveDaemon
 * @see SyncStateTracker
 * @since 1.0.0
 */
class VirtualFolder : public BHandler {
public:
    /**
     * @brief Construct a new Virtual Folder
     * 
     * @param localPath Path to the local sync folder
     * @param remotePath OneDrive path (e.g., "/Documents")
     * @param daemon Reference to the daemon for sync operations
     */
    VirtualFolder(const BPath& localPath, const BString& remotePath,
                  OneDriveDaemon* daemon);
    
    /**
     * @brief Destructor
     * 
     * Stops monitoring and cleans up resources.
     */
    virtual ~VirtualFolder();
    
    /**
     * @brief Initialize the virtual folder
     * 
     * Creates the local folder if needed, starts monitoring,
     * and performs initial scan of contents.
     * 
     * @return B_OK on success, error code otherwise
     */
    status_t Initialize();
    
    /**
     * @brief Start monitoring the folder for changes
     * 
     * Sets up node monitoring for the folder and its contents,
     * watching for file additions, removals, and modifications.
     * 
     * @return B_OK on success, error code otherwise
     * @see StopMonitoring()
     */
    status_t StartMonitoring();
    
    /**
     * @brief Stop monitoring the folder
     * 
     * Removes all node monitors and stops watching for changes.
     * 
     * @see StartMonitoring()
     */
    void StopMonitoring();
    
    /**
     * @brief Handle filesystem notification messages
     * 
     * Processes B_NODE_MONITOR messages for file system changes.
     * 
     * @param message The notification message
     */
    virtual void MessageReceived(BMessage* message);
    
    /**
     * @brief Get the sync state of a file or folder
     * 
     * @param ref Entry reference to check
     * @return Current sync state
     */
    OneDriveSyncState GetSyncState(const entry_ref& ref) const;
    
    /**
     * @brief Set the sync state of a file or folder
     * 
     * Updates the sync state and notifies Tracker for icon updates.
     * 
     * @param ref Entry reference to update
     * @param state New sync state
     * @param updateIcon Whether to update the Tracker icon
     */
    void SetSyncState(const entry_ref& ref, OneDriveSyncState state, 
                      bool updateIcon = true);
    
    /**
     * @brief Get virtual item information
     * 
     * @param ref Entry reference
     * @param item Output parameter for item info
     * @return B_OK if found, B_ENTRY_NOT_FOUND otherwise
     */
    status_t GetVirtualItem(const entry_ref& ref, VirtualItem& item) const;
    
    /**
     * @brief Update virtual item information
     * 
     * @param item Updated item information
     * @return B_OK on success, error code otherwise
     */
    status_t UpdateVirtualItem(const VirtualItem& item);
    
    /**
     * @brief Pin a file for offline access
     * 
     * Ensures the file is downloaded and kept locally.
     * 
     * @param ref Entry reference to pin
     * @return B_OK on success, error code otherwise
     */
    status_t PinForOffline(const entry_ref& ref);
    
    /**
     * @brief Unpin a file from offline access
     * 
     * Allows the file to be removed from local storage if needed.
     * 
     * @param ref Entry reference to unpin
     * @return B_OK on success, error code otherwise
     */
    status_t UnpinFromOffline(const entry_ref& ref);
    
    /**
     * @brief Make a file online-only
     * 
     * Removes local content but keeps metadata and placeholder.
     * 
     * @param ref Entry reference to make online-only
     * @return B_OK on success, error code otherwise
     */
    status_t MakeOnlineOnly(const entry_ref& ref);
    
    /**
     * @brief Request download of an online-only file
     * 
     * Triggers download when user accesses an online-only file.
     * 
     * @param ref Entry reference to download
     * @return B_OK on success, error code otherwise
     */
    status_t RequestDownload(const entry_ref& ref);
    
    /**
     * @brief Scan folder contents and update sync states
     * 
     * Performs a full scan of the folder to detect changes
     * and update sync states.
     * 
     * @return B_OK on success, error code otherwise
     */
    status_t ScanContents();
    
    /**
     * @brief Get list of items needing sync
     * 
     * @param items Output list of items requiring synchronization
     * @return Number of items added to the list
     */
    int32 GetPendingItems(BList& items) const;
    
    /**
     * @brief Update Tracker icon for a file
     * 
     * Sets the appropriate icon overlay based on sync state.
     * 
     * @param ref Entry reference to update
     * @param state Sync state for icon selection
     */
    void UpdateTrackerIcon(const entry_ref& ref, OneDriveSyncState state);
    
    /**
     * @brief Get the local folder path
     * 
     * @return Local filesystem path
     */
    const BPath& GetLocalPath() const { return fLocalPath; }
    
    /**
     * @brief Get the remote OneDrive path
     * 
     * @return Remote path on OneDrive
     */
    const BString& GetRemotePath() const { return fRemotePath; }
    
    /**
     * @brief Check if folder is being monitored
     * 
     * @return true if monitoring is active
     */
    bool IsMonitoring() const { return fIsMonitoring; }
    
    /**
     * @brief Get total size of local content
     * 
     * @return Total size in bytes
     */
    off_t GetLocalSize() const;
    
    /**
     * @brief Get number of items in folder
     * 
     * @param includeSubfolders Whether to count recursively
     * @return Number of items
     */
    int32 GetItemCount(bool includeSubfolders = true) const;

private:
    /**
     * @brief Handle file creation notification
     * 
     * @param message Node monitor message
     */
    void _HandleEntryCreated(BMessage* message);
    
    /**
     * @brief Handle file removal notification
     * 
     * @param message Node monitor message
     */
    void _HandleEntryRemoved(BMessage* message);
    
    /**
     * @brief Handle file move/rename notification
     * 
     * @param message Node monitor message
     */
    void _HandleEntryMoved(BMessage* message);
    
    /**
     * @brief Handle file modification notification
     * 
     * @param message Node monitor message
     */
    void _HandleStatChanged(BMessage* message);
    
    /**
     * @brief Handle attribute change notification
     * 
     * @param message Node monitor message
     */
    void _HandleAttrChanged(BMessage* message);
    
    /**
     * @brief Add item to monitoring
     * 
     * @param entry Entry to monitor
     * @return B_OK on success
     */
    status_t _AddToMonitoring(const BEntry& entry);
    
    /**
     * @brief Remove item from monitoring
     * 
     * @param ref Entry reference to stop monitoring
     */
    void _RemoveFromMonitoring(const entry_ref& ref);
    
    /**
     * @brief Create virtual item from entry
     * 
     * @param entry Source entry
     * @param item Output virtual item
     * @return B_OK on success
     */
    status_t _CreateVirtualItem(const BEntry& entry, VirtualItem& item);
    
    /**
     * @brief Load sync state from cache
     * 
     * @return B_OK on success
     */
    status_t _LoadSyncState();
    
    /**
     * @brief Save sync state to cache
     * 
     * @return B_OK on success
     */
    status_t _SaveSyncState();
    
    /**
     * @brief Notify daemon of changes
     * 
     * @param changes List of changed items
     */
    void _NotifyDaemon(const BList& changes);

private:
    BPath               fLocalPath;         ///< Local folder path
    BString             fRemotePath;        ///< OneDrive folder path
    OneDriveDaemon*     fDaemon;           ///< Daemon reference
    BList               fVirtualItems;      ///< List of VirtualItem pointers
    mutable BLocker     fItemsLock;        ///< Lock for thread-safe access
    bool                fIsMonitoring;      ///< Monitoring active flag
    node_ref            fNodeRef;           ///< Folder's node reference
    SyncStateTracker*   fStateTracker;      ///< Sync state persistence
};

#endif // ONEDRIVE_VIRTUAL_FOLDER_H