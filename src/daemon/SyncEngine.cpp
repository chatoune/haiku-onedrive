/**
 * @file SyncEngine.cpp
 * @brief Implementation of bidirectional synchronization engine
 * @author Haiku OneDrive Team
 * @version 1.0.0
 * @date 2025-08-15
 */

#include "SyncEngine.h"
#include "../shared/OneDriveConstants.h"
#include "../shared/ErrorLogger.h"
#include "../shared/FileSystemConstants.h"

#include <Autolock.h>
#include <Directory.h>
#include <Entry.h>
#include <File.h>
#include <FindDirectory.h>
#include <NodeInfo.h>
#include <Volume.h>
#include <VolumeRoster.h>
#include <fs_attr.h>

#include <private/libroot/SHA256.h>
#include <stdio.h>
#include <unistd.h>

using namespace OneDrive;
using namespace OneDrive::FileSystem;

// Message constants
static const uint32 kMsgSyncTimer = 'synT';
static const uint32 kMsgProcessQueue = 'prcQ';
static const uint32 kMsgSyncComplete = 'synC';
static const uint32 kMsgSyncError = 'synE';
static const uint32 kMsgProgressUpdate = 'prog';

// Default configuration
static const int32 kDefaultSyncInterval = 300;  // 5 minutes
static const int32 kDefaultMaxRetries = 3;
static const off_t kDefaultBandwidthLimit = 0;  // Unlimited

/**
 * @brief Constructor
 */
OneDriveSyncEngine::OneDriveSyncEngine(OneDriveAPI& api, CacheManager& cache,
                                     AttributeManager& attributes, const BPath& syncPath)
    : BHandler("SyncEngine"),
      fAPI(api),
      fCache(cache),
      fAttributes(attributes),
      fSyncPath(syncPath),
      fLock("SyncEngine Lock"),
      fInitialized(false),
      fIsSyncing(false),
      fIsPaused(false),
      fStopRequested(false),
      fSyncTimer(NULL),
      fConflictHandler(NULL),
      fProgressHandler(NULL),
      fLastSyncTime(0)
{
    // Initialize default configuration
    fConfig.direction = kSyncBidirectional;
    fConfig.conflictMode = kConflictAsk;
    fConfig.syncHiddenFiles = false;
    fConfig.syncSystemFiles = false;
    fConfig.syncAttributes = true;
    fConfig.maxRetries = kDefaultMaxRetries;
    fConfig.syncInterval = kDefaultSyncInterval;
    fConfig.bandwidthLimit = kDefaultBandwidthLimit;
    fConfig.wifiOnly = false;
    
    // Initialize statistics
    memset(&fStats, 0, sizeof(fStats));
}

/**
 * @brief Destructor
 */
OneDriveSyncEngine::~OneDriveSyncEngine()
{
    Shutdown();
}

/**
 * @brief Initialize the sync engine
 */
status_t
OneDriveSyncEngine::Initialize()
{
    BAutolock lock(fLock);
    
    if (fInitialized) {
        return B_OK;
    }
    
    LOG_INFO("SyncEngine", "Initializing sync engine for path: %s", fSyncPath.Path());
    
    // Verify sync path exists
    BDirectory syncDir(fSyncPath.Path());
    if (syncDir.InitCheck() != B_OK) {
        ErrorLogger::Instance().LogError("SyncEngine", syncDir.InitCheck(),
            "Sync path does not exist: %s", fSyncPath.Path());
        return syncDir.InitCheck();
    }
    
    // TODO: Create virtual folder handler when integrated with daemon
    status_t result = B_OK;
    
    // Load sync state
    result = _LoadSyncState();
    if (result != B_OK) {
        LOG_WARNING("SyncEngine", "Failed to load sync state, starting fresh");
        // Not fatal, we can start fresh
    }
    
    // Start sync timer
    BMessage timerMsg(kMsgSyncTimer);
    fSyncTimer = new BMessageRunner(this, &timerMsg, 
        fConfig.syncInterval * 1000000LL); // Convert to microseconds
    
    if (!fSyncTimer || fSyncTimer->InitCheck() != B_OK) {
        ErrorLogger::Instance().LogError("SyncEngine", B_ERROR,
            "Failed to create sync timer");
        delete fSyncTimer;
        fSyncTimer = NULL;
        return B_ERROR;
    }
    
    fInitialized = true;
    LOG_INFO("SyncEngine", "Sync engine initialized successfully");
    
    return B_OK;
}

/**
 * @brief Shutdown the sync engine
 */
void
OneDriveSyncEngine::Shutdown()
{
    BAutolock lock(fLock);
    
    if (!fInitialized) {
        return;
    }
    
    LOG_INFO("SyncEngine", "Shutting down sync engine");
    
    // Stop sync operations
    StopSync();
    
    // Stop sync timer
    delete fSyncTimer;
    fSyncTimer = NULL;
    
    // Save sync state
    _SaveSyncState();
    
    // TODO: Shutdown virtual folder when integrated
    
    fInitialized = false;
}

/**
 * @brief Start synchronization
 */
status_t
OneDriveSyncEngine::StartSync(bool fullSync)
{
    BAutolock lock(fLock);
    
    if (!fInitialized) {
        return B_NOT_INITIALIZED;
    }
    
    if (fIsSyncing) {
        return B_BUSY;
    }
    
    LOG_INFO("SyncEngine", "Starting %s sync", fullSync ? "full" : "delta");
    
    fIsSyncing = true;
    fStopRequested = false;
    fStats.startTime = time(NULL);
    fStats.endTime = 0;
    
    // Clear delta token for full sync
    if (fullSync) {
        fDeltaToken.SetTo("");
    }
    
    // Start sync in separate thread
    thread_id syncThread = spawn_thread(
        [](void* data) -> status_t {
            OneDriveSyncEngine* engine = static_cast<OneDriveSyncEngine*>(data);
            
            // Scan for changes
            status_t result = engine->_ScanLocalChanges();
            if (result != B_OK) {
                ErrorLogger::Instance().LogError("SyncEngine", result,
                    "Failed to scan local changes");
                return result;
            }
            
            result = engine->_ScanRemoteChanges();
            if (result != B_OK) {
                ErrorLogger::Instance().LogError("SyncEngine", result,
                    "Failed to scan remote changes");
                return result;
            }
            
            // Process sync queue
            engine->_ProcessSyncQueue();
            
            return B_OK;
        },
        "sync_thread", B_NORMAL_PRIORITY, this
    );
    
    if (syncThread < 0) {
        fIsSyncing = false;
        return syncThread;
    }
    
    resume_thread(syncThread);
    return B_OK;
}

/**
 * @brief Stop synchronization
 */
void
OneDriveSyncEngine::StopSync()
{
    BAutolock lock(fLock);
    
    if (!fIsSyncing) {
        return;
    }
    
    LOG_INFO("SyncEngine", "Stopping sync");
    
    fStopRequested = true;
    
    // Wait for sync to stop (with timeout)
    int32 timeout = 30; // 30 seconds
    while (fIsSyncing && timeout > 0) {
        lock.Unlock();
        snooze(1000000); // 1 second
        lock.Lock();
        timeout--;
    }
    
    if (fIsSyncing) {
        LOG_WARNING("SyncEngine", "Sync did not stop gracefully");
    }
}

/**
 * @brief Pause synchronization
 */
void
OneDriveSyncEngine::PauseSync()
{
    BAutolock lock(fLock);
    
    if (!fIsSyncing || fIsPaused) {
        return;
    }
    
    LOG_INFO("SyncEngine", "Pausing sync");
    fIsPaused = true;
}

/**
 * @brief Resume synchronization
 */
void
OneDriveSyncEngine::ResumeSync()
{
    BAutolock lock(fLock);
    
    if (!fIsPaused) {
        return;
    }
    
    LOG_INFO("SyncEngine", "Resuming sync");
    fIsPaused = false;
    
    // Notify processing thread
    if (Looper()) {
        Looper()->PostMessage(kMsgProcessQueue, this);
    }
}

/**
 * @brief Force sync of specific path
 */
status_t
OneDriveSyncEngine::SyncPath(const BPath& path, bool recursive)
{
    BAutolock lock(fLock);
    
    if (!fInitialized) {
        return B_NOT_INITIALIZED;
    }
    
    LOG_INFO("SyncEngine", "Force syncing path: %s", path.Path());
    
    // Create sync item for the path
    SyncItem item;
    item.localPath = path.Path();
    item.status = kSyncStatusPending;
    item.retryCount = 0;
    
    // Determine operation based on file existence
    BEntry entry(path.Path());
    if (entry.Exists()) {
        // Check if it's in OneDrive
        OneDriveItem fileInfo;
        if (fAPI.GetItemInfo(path.Path(), fileInfo) == ONEDRIVE_OK) {
            item.operation = kSyncOpUpdate;
            item.fileId = fileInfo.id;
        } else {
            item.operation = kSyncOpUpload;
        }
    } else {
        // Check if it exists in OneDrive
        OneDriveItem fileInfo;
        if (fAPI.GetItemInfo(path.Path(), fileInfo) == ONEDRIVE_OK) {
            item.operation = kSyncOpDownload;
            item.fileId = fileInfo.id;
        } else {
            return B_ENTRY_NOT_FOUND;
        }
    }
    
    // Add to priority queue
    _AddToQueue(item);
    
    // Process immediately if not syncing
    if (!fIsSyncing) {
        StartSync(false);
    }
    
    return B_OK;
}

/**
 * @brief Set sync configuration
 */
void
OneDriveSyncEngine::SetConfig(const SyncConfig& config)
{
    BAutolock lock(fLock);
    
    fConfig = config;
    
    // Update sync timer interval
    if (fSyncTimer) {
        delete fSyncTimer;
        BMessage timerMsg(kMsgSyncTimer);
        fSyncTimer = new BMessageRunner(this, &timerMsg,
            fConfig.syncInterval * 1000000LL);
    }
    
    LOG_INFO("SyncEngine", "Sync configuration updated");
}

/**
 * @brief Get sync statistics
 */
SyncStats
OneDriveSyncEngine::GetStats() const
{
    BAutolock lock(fLock);
    
    SyncStats stats = fStats;
    
    // Calculate throughput if syncing
    if (fIsSyncing && stats.startTime > 0) {
        time_t elapsed = time(NULL) - stats.startTime;
        if (elapsed > 0) {
            off_t totalBytes = stats.bytesUploaded + stats.bytesDownloaded;
            stats.throughput = (float)totalBytes / elapsed / 1024.0f; // KB/s
        }
    }
    
    return stats;
}

/**
 * @brief Get sync queue size
 */
int32
OneDriveSyncEngine::GetQueueSize() const
{
    BAutolock lock(fLock);
    return fSyncQueue.size();
}

/**
 * @brief Handle node monitor message
 */
void
OneDriveSyncEngine::HandleNodeMonitor(BMessage* message)
{
    if (!message || !fInitialized) {
        return;
    }
    
    int32 opcode;
    if (message->FindInt32("opcode", &opcode) != B_OK) {
        return;
    }
    
    // Handle different node monitor events
    switch (opcode) {
        case B_ENTRY_CREATED:
        case B_ENTRY_MOVED:
        {
            entry_ref ref;
            const char* name;
            if (message->FindInt32("device", &ref.device) == B_OK &&
                message->FindInt64("directory", &ref.directory) == B_OK &&
                message->FindString("name", &name) == B_OK) {
                
                ref.set_name(name);
                BPath path(&ref);
                
                if (_ShouldSync(path)) {
                    LOG_INFO("SyncEngine", "File created/moved: %s", path.Path());
                    SyncPath(path, false);
                }
            }
            break;
        }
        
        case B_ENTRY_REMOVED:
        {
            entry_ref ref;
            if (message->FindInt32("device", &ref.device) == B_OK &&
                message->FindInt64("node", &ref.directory) == B_OK) {
                
                LOG_INFO("SyncEngine", "File removed");
                // Handle deletion
                // TODO: Track path for deleted items
            }
            break;
        }
        
        case B_STAT_CHANGED:
        case B_ATTR_CHANGED:
        {
            entry_ref ref;
            if (message->FindInt32("device", &ref.device) == B_OK &&
                message->FindInt64("node", &ref.directory) == B_OK) {
                
                LOG_INFO("SyncEngine", "File/attr changed");
                // Handle modification
                // TODO: Track path for modified items
            }
            break;
        }
    }
}

/**
 * @brief Handle messages
 */
void
OneDriveSyncEngine::MessageReceived(BMessage* message)
{
    switch (message->what) {
        case kMsgSyncTimer:
            if (!fIsSyncing && !fIsPaused) {
                StartSync(false);
            }
            break;
            
        case kMsgProcessQueue:
            if (!fStopRequested) {
                _ProcessSyncQueue();
            }
            break;
            
        case B_NODE_MONITOR:
            HandleNodeMonitor(message);
            break;
            
        default:
            BHandler::MessageReceived(message);
            break;
    }
}

/**
 * @brief Scan local changes
 */
status_t
OneDriveSyncEngine::_ScanLocalChanges()
{
    LOG_INFO("SyncEngine", "Scanning local changes in %s", fSyncPath.Path());
    
    // TODO: Implement comprehensive local change scanning
    // For now, use simple directory traversal
    
    BDirectory dir(fSyncPath.Path());
    if (dir.InitCheck() != B_OK) {
        return dir.InitCheck();
    }
    
    return B_OK;
}

/**
 * @brief Scan remote changes
 */
status_t
OneDriveSyncEngine::_ScanRemoteChanges()
{
    LOG_INFO("SyncEngine", "Scanning remote changes");
    
    // Use delta token if available
    BList items;
    status_t result;
    
    if (fDeltaToken.IsEmpty()) {
        // Full scan
        result = fAPI.ListFolder("/", items);
    } else {
        // Delta scan
        // TODO: Implement delta API call
        result = fAPI.ListFolder("/", items);
    }
    
    if (result != ONEDRIVE_OK) {
        ErrorLogger::Instance().LogError("SyncEngine", result,
            "Failed to get remote changes");
        return result;
    }
    
    // TODO: Update delta token from response
    
    // Process changes
    // TODO: Compare with local state and queue sync items
    
    return B_OK;
}

/**
 * @brief Process sync queue
 */
void
OneDriveSyncEngine::_ProcessSyncQueue()
{
    while (!fSyncQueue.empty() && !fStopRequested) {
        if (fIsPaused) {
            // Wait for resume
            snooze(1000000); // 1 second
            continue;
        }
        
        BAutolock lock(fLock);
        
        // Get next item
        SyncItem item = fSyncQueue.front();
        fSyncQueue.pop();
        
        // Process item
        lock.Unlock();
        status_t result = _ProcessSyncItem(item);
        lock.Lock();
        
        if (result != B_OK) {
            item.retryCount++;
            if (item.retryCount < fConfig.maxRetries) {
                // Retry later
                _AddToQueue(item);
            } else {
                // Max retries reached
                fStats.failedItems++;
                LOG_ERROR("SyncEngine", "Failed to sync %s after %d retries",
                    item.localPath.String(), fConfig.maxRetries);
            }
        } else {
            fStats.completedItems++;
        }
        
        // Send progress update
        _SendProgressUpdate(item);
    }
    
    // Sync complete
    fIsSyncing = false;
    fStats.endTime = time(NULL);
    fLastSyncTime = fStats.endTime;
    
    // Save state
    _SaveSyncState();
    
    // Notify completion
    BMessage complete(kMsgSyncComplete);
    complete.AddInt32("total", fStats.totalItems);
    complete.AddInt32("completed", fStats.completedItems);
    complete.AddInt32("failed", fStats.failedItems);
    
    if (fProgressHandler) {
        BMessenger(fProgressHandler).SendMessage(&complete);
    }
}

/**
 * @brief Process single sync item
 */
status_t
OneDriveSyncEngine::_ProcessSyncItem(SyncItem& item)
{
    LOG_INFO("SyncEngine", "Processing %s: %s",
        item.operation == kSyncOpUpload ? "upload" :
        item.operation == kSyncOpDownload ? "download" :
        item.operation == kSyncOpUpdate ? "update" :
        item.operation == kSyncOpDelete ? "delete" :
        item.operation == kSyncOpMove ? "move" :
        item.operation == kSyncOpCreateFolder ? "create folder" :
        "unknown",
        item.localPath.String());
    
    _UpdateItemStatus(item, kSyncStatusInProgress);
    
    status_t result = B_ERROR;
    
    switch (item.operation) {
        case kSyncOpUpload:
            result = _UploadFile(item);
            break;
            
        case kSyncOpDownload:
            result = _DownloadFile(item);
            break;
            
        case kSyncOpUpdate:
            result = _UpdateFile(item);
            break;
            
        case kSyncOpDelete:
            result = _DeleteFile(item);
            break;
            
        case kSyncOpMove:
            result = _MoveFile(item);
            break;
            
        case kSyncOpCreateFolder:
            result = _CreateFolder(item);
            break;
            
        case kSyncOpConflict:
            result = _HandleConflict(item);
            break;
            
        default:
            LOG_WARNING("SyncEngine", "Unknown sync operation: %d", item.operation);
            break;
    }
    
    if (result == B_OK) {
        _UpdateItemStatus(item, kSyncStatusCompleted);
    } else {
        _UpdateItemStatus(item, kSyncStatusError);
        item.errorMessage.SetToFormat("Error %s (0x%x)", strerror(result), result);
    }
    
    return result;
}

/**
 * @brief Upload file
 */
status_t
OneDriveSyncEngine::_UploadFile(SyncItem& item)
{
    BFile file(item.localPath.String(), B_READ_ONLY);
    if (file.InitCheck() != B_OK) {
        return file.InitCheck();
    }
    
    // Check if we should cache it
    if (item.isPinned || fCache.IsFolderPinned(BPath(item.localPath.String()))) {
        fCache.PinFile(item.fileId);
    }
    
    // Upload file
    status_t result = fAPI.UploadFile(item.localPath.String(), 
                                     item.remotePath.String());
    
    if (result == ONEDRIVE_OK) {
        // TODO: Get file ID from upload response
        
        // TODO: Update virtual folder state when integrated
        
        // Update stats
        fStats.bytesUploaded += item.size;
        
        // TODO: Sync attributes when fully implemented
    }
    
    return result;
}

/**
 * @brief Download file
 */
status_t
OneDriveSyncEngine::_DownloadFile(SyncItem& item)
{
    // Create parent directory if needed
    BPath parentPath;
    BPath localPath(item.localPath.String());
    localPath.GetParent(&parentPath);
    
    create_directory(parentPath.Path(), 0755);
    
    // Download file
    status_t result = fAPI.DownloadFile(item.fileId, item.localPath.String());
    
    if (result == ONEDRIVE_OK) {
        // Cache if pinned
        if (item.isPinned || fCache.IsFolderPinned(localPath)) {
            fCache.CacheFile(item.fileId, localPath, localPath, true);
        }
        
        // TODO: Update virtual folder state when integrated
        
        // Update stats
        fStats.bytesDownloaded += item.size;
        
        // TODO: Sync attributes when fully implemented
    }
    
    return result;
}

/**
 * @brief Update file
 */
status_t
OneDriveSyncEngine::_UpdateFile(SyncItem& item)
{
    // Detect conflict
    if (_DetectConflict(item)) {
        item.operation = kSyncOpConflict;
        return _HandleConflict(item);
    }
    
    // Determine update direction
    if (item.localModified > item.remoteModified) {
        // Upload local changes
        return _UploadFile(item);
    } else {
        // Download remote changes
        return _DownloadFile(item);
    }
}

/**
 * @brief Delete file
 */
status_t
OneDriveSyncEngine::_DeleteFile(SyncItem& item)
{
    // Delete from OneDrive
    status_t result = fAPI.DeleteItem(item.fileId);
    
    if (result == ONEDRIVE_OK) {
        // Remove from cache
        fCache.EvictFile(item.fileId);
        
        // Delete local file if it exists
        BEntry entry(item.localPath.String());
        if (entry.Exists()) {
            entry.Remove();
        }
    }
    
    return result;
}

/**
 * @brief Move file
 */
status_t
OneDriveSyncEngine::_MoveFile(SyncItem& item)
{
    // TODO: Implement move operation
    return B_NOT_SUPPORTED;
}

/**
 * @brief Create folder
 */
status_t
OneDriveSyncEngine::_CreateFolder(SyncItem& item)
{
    // Extract folder name from path
    BPath path(item.remotePath.String());
    BString folderName(path.Leaf());
    
    BPath parentPath;
    path.GetParent(&parentPath);
    
    status_t result = fAPI.CreateFolder(parentPath.Path(), folderName);
    
    if (result == ONEDRIVE_OK) {
        // TODO: Get folder ID from response
        
        // Create local folder if needed
        create_directory(item.localPath.String(), 0755);
        
        // TODO: Update virtual folder state when integrated
    }
    
    return result;
}

/**
 * @brief Handle conflict
 */
status_t
OneDriveSyncEngine::_HandleConflict(SyncItem& item)
{
    LOG_WARNING("SyncEngine", "Conflict detected for %s", item.localPath.String());
    
    fStats.conflictItems++;
    
    // Apply conflict resolution strategy
    switch (fConfig.conflictMode) {
        case kConflictLocalWins:
            return _UploadFile(item);
            
        case kConflictRemoteWins:
            return _DownloadFile(item);
            
        case kConflictRename:
        {
            // Rename local file with conflict suffix
            BString conflictPath = item.localPath;
            conflictPath << ".conflict." << time(NULL);
            
            BEntry entry(item.localPath.String());
            entry.Rename(conflictPath.String());
            
            // Download remote version
            return _DownloadFile(item);
        }
        
        case kConflictAsk:
        {
            // Send conflict notification
            if (fConflictHandler) {
                BMessage conflict(kMsgConflictDetected);
                conflict.AddString("path", item.localPath);
                conflict.AddString("fileId", item.fileId);
                conflict.AddInt64("localModified", item.localModified);
                conflict.AddInt64("remoteModified", item.remoteModified);
                
                BMessenger(fConflictHandler).SendMessage(&conflict);
            }
            
            // Skip for now
            _UpdateItemStatus(item, kSyncStatusConflict);
            return B_OK;
        }
        
        default:
            return B_ERROR;
    }
}

/**
 * @brief Detect conflicts
 */
bool
OneDriveSyncEngine::_DetectConflict(const SyncItem& item)
{
    // Simple conflict detection based on modification times and hashes
    if (item.localModified == 0 || item.remoteModified == 0) {
        return false;
    }
    
    // If both files modified since last sync
    if (item.localModified > fLastSyncTime && item.remoteModified > fLastSyncTime) {
        // Check hashes
        if (!item.localHash.IsEmpty() && !item.remoteHash.IsEmpty() &&
            item.localHash != item.remoteHash) {
            return true;
        }
    }
    
    return false;
}

/**
 * @brief Add item to sync queue
 */
void
OneDriveSyncEngine::_AddToQueue(const SyncItem& item)
{
    BAutolock lock(fLock);
    
    fSyncQueue.push(item);
    fStats.totalItems++;
    
    // Notify processing thread
    if (Looper()) {
        Looper()->PostMessage(kMsgProcessQueue, this);
    }
}

/**
 * @brief Update sync item status
 */
void
OneDriveSyncEngine::_UpdateItemStatus(SyncItem& item, SyncItemStatus status)
{
    item.status = status;
    
    // TODO: Update virtual folder sync state when integrated
}

/**
 * @brief Send progress update
 */
void
OneDriveSyncEngine::_SendProgressUpdate(const SyncItem& item)
{
    if (!fProgressHandler) {
        return;
    }
    
    BMessage progress(kMsgProgressUpdate);
    progress.AddString("path", item.localPath);
    progress.AddInt32("operation", item.operation);
    progress.AddInt32("status", item.status);
    progress.AddInt32("totalItems", fStats.totalItems);
    progress.AddInt32("completedItems", fStats.completedItems);
    progress.AddInt32("failedItems", fStats.failedItems);
    progress.AddInt64("bytesUploaded", fStats.bytesUploaded);
    progress.AddInt64("bytesDownloaded", fStats.bytesDownloaded);
    
    if (!item.errorMessage.IsEmpty()) {
        progress.AddString("error", item.errorMessage);
    }
    
    BMessenger(fProgressHandler).SendMessage(&progress);
}

/**
 * @brief Load sync state
 */
status_t
OneDriveSyncEngine::_LoadSyncState()
{
    // TODO: Load from persistent storage
    // For now, return not found
    return B_ENTRY_NOT_FOUND;
}

/**
 * @brief Save sync state
 */
status_t
OneDriveSyncEngine::_SaveSyncState()
{
    // TODO: Save to persistent storage
    LOG_INFO("SyncEngine", "Saving sync state (delta token: %s)", 
        fDeltaToken.String());
    
    return B_OK;
}

/**
 * @brief Calculate file hash
 */
BString
OneDriveSyncEngine::_CalculateFileHash(const BPath& path)
{
    BFile file(path.Path(), B_READ_ONLY);
    if (file.InitCheck() != B_OK) {
        return "";
    }
    
    // Get file size
    off_t size;
    if (file.GetSize(&size) != B_OK) {
        return "";
    }
    
    // Initialize SHA256
    BPrivate::SHA256 sha;
    sha.Init();
    
    // Read and hash file in chunks
    const size_t kBufferSize = 64 * 1024; // 64KB chunks
    uint8* buffer = new uint8[kBufferSize];
    
    off_t remaining = size;
    while (remaining > 0) {
        ssize_t toRead = min_c(remaining, kBufferSize);
        ssize_t bytesRead = file.Read(buffer, toRead);
        
        if (bytesRead <= 0) {
            delete[] buffer;
            return "";
        }
        
        sha.Update(buffer, bytesRead);
        remaining -= bytesRead;
    }
    
    delete[] buffer;
    
    // Get digest
    const uint8* digest = sha.Digest();
    
    // Convert to hex string
    BString hashStr;
    char hexBuf[3];
    for (int i = 0; i < 32; i++) { // SHA256 produces 32 bytes
        sprintf(hexBuf, "%02x", digest[i]);
        hashStr << hexBuf;
    }
    
    return hashStr;
}

/**
 * @brief Check if path should be synced
 */
bool
OneDriveSyncEngine::_ShouldSync(const BPath& path)
{
    if (!path.Path()) {
        return false;
    }
    
    BString pathStr(path.Path());
    
    // Check if within sync folder
    BString syncStr(fSyncPath.Path());
    if (!pathStr.StartsWith(syncStr)) {
        return false;
    }
    
    // Get filename
    BString filename(path.Leaf());
    
    // Check hidden files
    if (!fConfig.syncHiddenFiles && filename.StartsWith(".")) {
        return false;
    }
    
    // Check exclude patterns
    for (int32 i = 0; i < fConfig.excludePatterns.CountStrings(); i++) {
        // TODO: Implement pattern matching
        if (filename == fConfig.excludePatterns.StringAt(i)) {
            return false;
        }
    }
    
    // Check include patterns if any
    if (fConfig.includePatterns.CountStrings() > 0) {
        bool included = false;
        for (int32 i = 0; i < fConfig.includePatterns.CountStrings(); i++) {
            // TODO: Implement pattern matching
            if (filename == fConfig.includePatterns.StringAt(i)) {
                included = true;
                break;
            }
        }
        if (!included) {
            return false;
        }
    }
    
    return true;
}

/**
 * @brief Apply bandwidth limit
 */
void
OneDriveSyncEngine::_ApplyBandwidthLimit(off_t bytesTransferred)
{
    if (fConfig.bandwidthLimit <= 0) {
        return; // No limit
    }
    
    // Simple bandwidth limiting - sleep if we're going too fast
    static bigtime_t lastCheck = 0;
    static off_t lastBytes = 0;
    
    bigtime_t now = system_time();
    if (lastCheck == 0) {
        lastCheck = now;
        lastBytes = bytesTransferred;
        return;
    }
    
    bigtime_t elapsed = now - lastCheck;
    if (elapsed >= 1000000) { // Check every second
        off_t bytesThisPeriod = bytesTransferred - lastBytes;
        float currentRate = (float)bytesThisPeriod / (elapsed / 1000000.0f);
        
        if (currentRate > fConfig.bandwidthLimit) {
            // Sleep to reduce rate
            bigtime_t sleepTime = (bigtime_t)((bytesThisPeriod - fConfig.bandwidthLimit) * 
                                             1000000.0f / fConfig.bandwidthLimit);
            snooze(sleepTime);
        }
        
        lastCheck = now;
        lastBytes = bytesTransferred;
    }
}

/**
 * @brief Sync timer tick
 */
void
OneDriveSyncEngine::_SyncTimerTick()
{
    // Start sync if not already running
    if (!fIsSyncing && !fIsPaused) {
        StartSync(false);
    }
}