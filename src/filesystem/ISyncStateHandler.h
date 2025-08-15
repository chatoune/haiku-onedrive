/**
 * @file ISyncStateHandler.h
 * @brief Interface for handling synchronization state changes
 * @author Haiku OneDrive Team
 * @version 1.0.0
 * @date 2025-08-15
 * 
 * This interface defines the contract for handling sync state changes,
 * making the system more testable and extensible.
 */

#ifndef ISYNC_STATE_HANDLER_H
#define ISYNC_STATE_HANDLER_H

#include <Entry.h>
#include "VirtualFolder.h"

namespace OneDrive {

/**
 * @brief Interface for handling synchronization state changes
 * 
 * This interface allows different implementations of sync state handling,
 * making it easier to test and extend the synchronization system.
 * 
 * @since 1.0.0
 */
class ISyncStateHandler {
public:
    /**
     * @brief Virtual destructor
     */
    virtual ~ISyncStateHandler() {}
    
    /**
     * @brief Handle sync state change for an item
     * 
     * @param ref Entry reference
     * @param oldState Previous sync state
     * @param newState New sync state
     * @return B_OK on success, error code otherwise
     */
    virtual status_t OnSyncStateChanged(const entry_ref& ref,
                                       OneDriveSyncState oldState,
                                       OneDriveSyncState newState) = 0;
    
    /**
     * @brief Handle sync conflict
     * 
     * @param ref Entry reference
     * @param localModTime Local modification time
     * @param remoteModTime Remote modification time
     * @return Resolution action to take
     */
    virtual int32 OnSyncConflict(const entry_ref& ref,
                                time_t localModTime,
                                time_t remoteModTime) = 0;
    
    /**
     * @brief Handle sync error
     * 
     * @param ref Entry reference
     * @param error Error code
     * @param errorMessage Error description
     * @return Whether to retry the operation
     */
    virtual bool OnSyncError(const entry_ref& ref,
                            status_t error,
                            const char* errorMessage) = 0;
    
    /**
     * @brief Handle sync progress update
     * 
     * @param ref Entry reference
     * @param bytesCompleted Bytes transferred
     * @param totalBytes Total bytes to transfer
     */
    virtual void OnSyncProgress(const entry_ref& ref,
                               off_t bytesCompleted,
                               off_t totalBytes) = 0;
    
    /**
     * @brief Check if sync should be performed
     * 
     * @param ref Entry reference
     * @return true if sync should proceed
     */
    virtual bool ShouldSync(const entry_ref& ref) = 0;
    
    /**
     * @brief Get priority for sync operation
     * 
     * @param ref Entry reference
     * @return Priority value (higher = more urgent)
     */
    virtual int32 GetSyncPriority(const entry_ref& ref) = 0;
};

/**
 * @brief Default implementation of sync state handler
 * 
 * Provides basic sync state handling with configurable behavior.
 */
class DefaultSyncStateHandler : public ISyncStateHandler {
public:
    DefaultSyncStateHandler();
    virtual ~DefaultSyncStateHandler();
    
    virtual status_t OnSyncStateChanged(const entry_ref& ref,
                                       OneDriveSyncState oldState,
                                       OneDriveSyncState newState);
    
    virtual int32 OnSyncConflict(const entry_ref& ref,
                                time_t localModTime,
                                time_t remoteModTime);
    
    virtual bool OnSyncError(const entry_ref& ref,
                            status_t error,
                            const char* errorMessage);
    
    virtual void OnSyncProgress(const entry_ref& ref,
                               off_t bytesCompleted,
                               off_t totalBytes);
    
    virtual bool ShouldSync(const entry_ref& ref);
    
    virtual int32 GetSyncPriority(const entry_ref& ref);
    
    /**
     * @brief Set conflict resolution strategy
     * 
     * @param strategy Strategy to use (0=ask, 1=local wins, 2=remote wins)
     */
    void SetConflictStrategy(int32 strategy) { fConflictStrategy = strategy; }
    
    /**
     * @brief Set maximum retry count
     * 
     * @param count Maximum number of retries
     */
    void SetMaxRetries(int32 count) { fMaxRetries = count; }

private:
    int32 fConflictStrategy;
    int32 fMaxRetries;
    int32 fCurrentRetries;
};

} // namespace OneDrive

#endif // ISYNC_STATE_HANDLER_H