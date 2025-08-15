/**
 * @file SyncEngine.h
 * @brief Bidirectional synchronization engine for OneDrive
 * @author Haiku OneDrive Team
 * @version 1.0.0
 * @date 2025-08-15
 * 
 * The SyncEngine class manages the core synchronization logic between
 * local files and OneDrive cloud storage, including conflict resolution,
 * delta sync, and change tracking.
 */

#ifndef SYNC_ENGINE_H
#define SYNC_ENGINE_H

#include <Handler.h>
#include <List.h>
#include <Locker.h>
#include <Message.h>
#include <MessageRunner.h>
#include <Path.h>
#include <String.h>
#include <StringList.h>
#include <NodeMonitor.h>

#include <map>
#include <queue>
#include <memory>

// Include necessary headers for the classes we use
#include "../api/OneDriveAPI.h"
#include "CacheManager.h"
#include "AttributeManager.h"

namespace OneDrive {

/**
 * @brief Sync operation types
 */
enum SyncOperation {
    kSyncOpUpload = 0,      ///< Upload local file to cloud
    kSyncOpDownload,        ///< Download cloud file to local
    kSyncOpUpdate,          ///< Update existing file
    kSyncOpDelete,          ///< Delete file
    kSyncOpMove,            ///< Move/rename file
    kSyncOpCreateFolder,    ///< Create folder
    kSyncOpConflict         ///< Handle conflict
};

/**
 * @brief Sync item status
 */
enum SyncItemStatus {
    kSyncStatusPending = 0, ///< Waiting to sync
    kSyncStatusInProgress,  ///< Currently syncing
    kSyncStatusCompleted,   ///< Successfully synced
    kSyncStatusError,       ///< Sync failed
    kSyncStatusConflict,    ///< Conflict detected
    kSyncStatusSkipped      ///< Skipped (filtered out)
};

/**
 * @brief Sync direction
 */
enum SyncDirection {
    kSyncBidirectional = 0, ///< Two-way sync
    kSyncDownloadOnly,      ///< Cloud to local only
    kSyncUploadOnly         ///< Local to cloud only
};

/**
 * @brief Sync conflict resolution strategy
 */
enum ConflictResolution {
    kConflictLocalWins = 0, ///< Keep local version
    kConflictRemoteWins,    ///< Keep remote version
    kConflictMerge,         ///< Attempt to merge
    kConflictRename,        ///< Keep both with rename
    kConflictAsk            ///< Ask user
};

/**
 * @brief Sync item information
 */
struct SyncItem {
    BString localPath;          ///< Local file path
    BString remotePath;         ///< Remote file path
    BString fileId;             ///< OneDrive file ID
    SyncOperation operation;    ///< Operation to perform
    SyncItemStatus status;      ///< Current status
    time_t localModified;       ///< Local modification time
    time_t remoteModified;      ///< Remote modification time
    off_t size;                 ///< File size
    BString eTag;               ///< Remote ETag
    BString localHash;          ///< Local file hash
    BString remoteHash;         ///< Remote file hash
    int32 retryCount;           ///< Number of retry attempts
    BString errorMessage;       ///< Error message if failed
    bool isPinned;              ///< Pinned for offline access
};

/**
 * @brief Sync statistics
 */
struct SyncStats {
    int32 totalItems;           ///< Total items to sync
    int32 completedItems;       ///< Completed items
    int32 failedItems;          ///< Failed items
    int32 conflictItems;        ///< Items with conflicts
    off_t bytesUploaded;        ///< Total bytes uploaded
    off_t bytesDownloaded;      ///< Total bytes downloaded
    time_t startTime;           ///< Sync start time
    time_t endTime;             ///< Sync end time
    float throughput;           ///< Average throughput (KB/s)
};

/**
 * @brief Sync engine configuration
 */
struct SyncConfig {
    SyncDirection direction;            ///< Sync direction
    ConflictResolution conflictMode;    ///< Conflict resolution mode
    bool syncHiddenFiles;               ///< Sync hidden files
    bool syncSystemFiles;               ///< Sync system files
    bool syncAttributes;                ///< Sync BFS attributes
    int32 maxRetries;                   ///< Max retry attempts
    int32 syncInterval;                 ///< Sync interval (seconds)
    off_t bandwidthLimit;               ///< Bandwidth limit (bytes/sec)
    bool wifiOnly;                      ///< Only sync on WiFi
    BStringList excludePatterns;        ///< File patterns to exclude
    BStringList includePatterns;        ///< File patterns to include
};

/**
 * @brief Manages bidirectional synchronization between local and cloud
 * 
 * The OneDriveSyncEngine class is the core component responsible for
 * keeping local files synchronized with OneDrive cloud storage. It handles:
 * - Change detection (local and remote)
 * - Conflict resolution
 * - Delta synchronization
 * - Retry logic and error handling
 * - Bandwidth management
 * - Progress reporting
 * 
 * @see OneDriveAPI
 * @see CacheManager
 * @see AttributeManager
 * @since 1.0.0
 */
class OneDriveSyncEngine : public BHandler {
public:
    /**
     * @brief Constructor
     * 
     * @param api OneDrive API client
     * @param cache Cache manager
     * @param attributes Attribute manager
     * @param syncPath Root sync folder path
     */
    OneDriveSyncEngine(OneDriveAPI& api, CacheManager& cache, 
                      AttributeManager& attributes, const BPath& syncPath);
    
    /**
     * @brief Destructor
     */
    virtual ~OneDriveSyncEngine();
    
    /**
     * @brief Initialize the sync engine
     * 
     * Sets up monitoring, loads sync state, and prepares for operation.
     * 
     * @return B_OK on success, error code otherwise
     */
    status_t Initialize();
    
    /**
     * @brief Shutdown the sync engine
     * 
     * Stops all sync operations and saves state.
     */
    void Shutdown();
    
    /**
     * @brief Start synchronization
     * 
     * Begins sync operations based on current configuration.
     * 
     * @param fullSync Whether to perform full sync (vs delta)
     * @return B_OK on success
     */
    status_t StartSync(bool fullSync = false);
    
    /**
     * @brief Stop synchronization
     * 
     * Gracefully stops ongoing sync operations.
     */
    void StopSync();
    
    /**
     * @brief Pause synchronization
     * 
     * Temporarily pauses sync without losing state.
     */
    void PauseSync();
    
    /**
     * @brief Resume synchronization
     * 
     * Resumes paused sync operations.
     */
    void ResumeSync();
    
    /**
     * @brief Check if sync is active
     * 
     * @return true if sync is running
     */
    bool IsSyncing() const { return fIsSyncing; }
    
    /**
     * @brief Check if sync is paused
     * 
     * @return true if sync is paused
     */
    bool IsPaused() const { return fIsPaused; }
    
    /**
     * @brief Force sync of specific file/folder
     * 
     * @param path Path to sync
     * @param recursive Whether to sync recursively
     * @return B_OK on success
     */
    status_t SyncPath(const BPath& path, bool recursive = true);
    
    /**
     * @brief Get sync configuration
     * 
     * @return Current sync configuration
     */
    const SyncConfig& GetConfig() const { return fConfig; }
    
    /**
     * @brief Set sync configuration
     * 
     * @param config New configuration
     */
    void SetConfig(const SyncConfig& config);
    
    /**
     * @brief Get sync statistics
     * 
     * @return Current sync statistics
     */
    SyncStats GetStats() const;
    
    /**
     * @brief Get sync queue size
     * 
     * @return Number of items in sync queue
     */
    int32 GetQueueSize() const;
    
    /**
     * @brief Handle node monitor message
     * 
     * @param message Node monitor message
     */
    void HandleNodeMonitor(BMessage* message);
    
    /**
     * @brief Handle message
     * 
     * @param message Message to handle
     */
    virtual void MessageReceived(BMessage* message);
    
    /**
     * @brief Set conflict resolution callback
     * 
     * @param target Handler to receive conflict messages
     */
    void SetConflictHandler(BHandler* target) { fConflictHandler = target; }
    
    /**
     * @brief Set progress callback
     * 
     * @param target Handler to receive progress messages
     */
    void SetProgressHandler(BHandler* target) { fProgressHandler = target; }

private:
    /**
     * @brief Scan local changes
     * 
     * @return B_OK on success
     */
    status_t _ScanLocalChanges();
    
    /**
     * @brief Scan remote changes
     * 
     * @return B_OK on success
     */
    status_t _ScanRemoteChanges();
    
    /**
     * @brief Process sync queue
     */
    void _ProcessSyncQueue();
    
    /**
     * @brief Process single sync item
     * 
     * @param item Item to process
     * @return B_OK on success
     */
    status_t _ProcessSyncItem(SyncItem& item);
    
    /**
     * @brief Upload file
     * 
     * @param item Sync item
     * @return B_OK on success
     */
    status_t _UploadFile(SyncItem& item);
    
    /**
     * @brief Download file
     * 
     * @param item Sync item
     * @return B_OK on success
     */
    status_t _DownloadFile(SyncItem& item);
    
    /**
     * @brief Update file
     * 
     * @param item Sync item
     * @return B_OK on success
     */
    status_t _UpdateFile(SyncItem& item);
    
    /**
     * @brief Delete file
     * 
     * @param item Sync item
     * @return B_OK on success
     */
    status_t _DeleteFile(SyncItem& item);
    
    /**
     * @brief Move file
     * 
     * @param item Sync item
     * @return B_OK on success
     */
    status_t _MoveFile(SyncItem& item);
    
    /**
     * @brief Create folder
     * 
     * @param item Sync item
     * @return B_OK on success
     */
    status_t _CreateFolder(SyncItem& item);
    
    /**
     * @brief Handle conflict
     * 
     * @param item Sync item with conflict
     * @return B_OK on success
     */
    status_t _HandleConflict(SyncItem& item);
    
    /**
     * @brief Detect conflicts
     * 
     * @param item Sync item to check
     * @return true if conflict exists
     */
    bool _DetectConflict(const SyncItem& item);
    
    /**
     * @brief Add item to sync queue
     * 
     * @param item Item to add
     */
    void _AddToQueue(const SyncItem& item);
    
    /**
     * @brief Update sync status
     * 
     * @param item Item to update
     * @param status New status
     */
    void _UpdateItemStatus(SyncItem& item, SyncItemStatus status);
    
    /**
     * @brief Send progress update
     * 
     * @param item Current sync item
     */
    void _SendProgressUpdate(const SyncItem& item);
    
    /**
     * @brief Load sync state
     * 
     * @return B_OK on success
     */
    status_t _LoadSyncState();
    
    /**
     * @brief Save sync state
     * 
     * @return B_OK on success
     */
    status_t _SaveSyncState();
    
    /**
     * @brief Calculate file hash
     * 
     * @param path File path
     * @return Hash string
     */
    BString _CalculateFileHash(const BPath& path);
    
    /**
     * @brief Check if path should be synced
     * 
     * @param path Path to check
     * @return true if should sync
     */
    bool _ShouldSync(const BPath& path);
    
    /**
     * @brief Apply bandwidth limit
     * 
     * @param bytesTransferred Bytes transferred so far
     */
    void _ApplyBandwidthLimit(off_t bytesTransferred);
    
    /**
     * @brief Sync timer tick
     */
    void _SyncTimerTick();

private:
    OneDriveAPI& fAPI;                      ///< OneDrive API client
    CacheManager& fCache;                   ///< Cache manager
    AttributeManager& fAttributes;          ///< Attribute manager
    BPath fSyncPath;                        ///< Root sync folder
    
    SyncConfig fConfig;                     ///< Sync configuration
    std::queue<SyncItem> fSyncQueue;        ///< Queue of items to sync
    std::map<BString, SyncItem> fActiveItems; ///< Currently syncing items
    mutable BLocker fLock;                  ///< Thread safety lock
    
    bool fInitialized;                      ///< Initialization flag
    bool fIsSyncing;                        ///< Currently syncing flag
    bool fIsPaused;                         ///< Sync paused flag
    bool fStopRequested;                    ///< Stop requested flag
    
    BMessageRunner* fSyncTimer;             ///< Periodic sync timer
    BHandler* fConflictHandler;             ///< Conflict resolution handler
    BHandler* fProgressHandler;             ///< Progress update handler
    
    SyncStats fStats;                       ///< Current sync statistics
    time_t fLastSyncTime;                   ///< Last successful sync
    BString fDeltaToken;                    ///< Delta sync token
};

} // namespace OneDrive

#endif // SYNC_ENGINE_H