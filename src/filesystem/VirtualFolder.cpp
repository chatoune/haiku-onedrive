/**
 * @file VirtualFolder.cpp
 * @brief Implementation of virtual folder representation for OneDrive sync
 * @author Haiku OneDrive Team
 * @version 1.0.0
 * @date 2025-08-15
 */

#include "VirtualFolder.h"

#include <Alert.h>
#include <Autolock.h>
#include <Catalog.h>
#include <Directory.h>
#include <Entry.h>
#include <File.h>
#include <FindDirectory.h>
#include <Mime.h>
#include <Node.h>
#include <NodeInfo.h>
#include <Roster.h>
#include <Volume.h>
#include <fs_attr.h>

// Forward declaration instead of including the header to avoid circular dependency
class OneDriveDaemon;
#include "../shared/JSONSerializer.h"
#include "SyncStateIcons.h"
#include "../shared/FileSystemConstants.h"
#include "../shared/ErrorLogger.h"
#include "../shared/AttributeHelper.h"

#include <stdio.h>
#include <stdlib.h>

using namespace OneDrive;
using namespace OneDrive::FileSystem;

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "VirtualFolder"

// Custom attributes for sync state storage
static const char* kSyncStateAttr = "OneDrive:SyncState";
static const char* kCloudIdAttr = "OneDrive:CloudId";
static const char* kETagAttr = "OneDrive:ETag";
static const char* kPinnedAttr = "OneDrive:Pinned";

// Icon attribute for Tracker
static const char* kTrackerIconAttr = "BEOS:ICON";
static const char* kMiniIconAttr = "BEOS:M:ICON";
static const char* kVectorIconAttr = "BEOS:VECTOR_ICON";

/**
 * @brief Construct a new Virtual Folder
 */
VirtualFolder::VirtualFolder(const BPath& localPath, const BString& remotePath,
                             OneDriveDaemon* daemon)
    : BHandler("VirtualFolder"),
      fLocalPath(localPath),
      fRemotePath(remotePath),
      fDaemon(daemon),
      fIsMonitoring(false),
      fStateTracker(NULL)
{
    // Items list will be populated during Initialize()
}

/**
 * @brief Destructor
 */
VirtualFolder::~VirtualFolder()
{
    StopMonitoring();
    
    // Clean up virtual items
    fItemsLock.Lock();
    for (int32 i = 0; i < fVirtualItems.CountItems(); i++) {
        VirtualItem* item = static_cast<VirtualItem*>(fVirtualItems.ItemAt(i));
        delete item;
    }
    fVirtualItems.MakeEmpty();
    fItemsLock.Unlock();
    
    // delete fStateTracker; // TODO: Implement SyncStateTracker class
}

/**
 * @brief Initialize the virtual folder
 */
status_t
VirtualFolder::Initialize()
{
    // Initialize the icon system
    SyncStateIcons::Instance().Initialize();
    
    // Create local folder if it doesn't exist
    BDirectory dir;
    status_t result = dir.SetTo(fLocalPath.Path());
    if (result == B_ENTRY_NOT_FOUND) {
        result = create_directory(fLocalPath.Path(), kDefaultDirectoryMode);
        if (result != B_OK) {
            ErrorLogger::Instance().LogError("VirtualFolder", result,
                    "Failed to create directory %s", fLocalPath.Path());
            return result;
        }
        result = dir.SetTo(fLocalPath.Path());
    }
    
    if (result != B_OK) {
        return result;
    }
    
    // Get node reference for monitoring  
    BEntry dirEntry;
    dir.GetEntry(&dirEntry);
    BNode node(&dirEntry);
    result = node.GetNodeRef(&fNodeRef);
    if (result != B_OK) {
        return result;
    }
    
    // Load existing sync state
    _LoadSyncState();
    
    // Scan current contents
    result = ScanContents();
    if (result != B_OK) {
        ErrorLogger::Instance().LogError("VirtualFolder", result,
                "Initial scan of %s failed", fLocalPath.Path());
    }
    
    return B_OK;
}

/**
 * @brief Start monitoring the folder for changes
 */
status_t
VirtualFolder::StartMonitoring()
{
    if (fIsMonitoring) {
        return B_OK;
    }
    
    // Start watching the main folder
    status_t result = watch_node(&fNodeRef, 
                                 B_WATCH_DIRECTORY | B_WATCH_ATTR | B_WATCH_STAT,
                                 this);
    if (result != B_OK) {
        ErrorLogger::Instance().LogError("VirtualFolder", result,
                "Failed to start monitoring %s", fLocalPath.Path());
        return result;
    }
    
    // Monitor all existing items
    fItemsLock.Lock();
    for (int32 i = 0; i < fVirtualItems.CountItems(); i++) {
        VirtualItem* item = static_cast<VirtualItem*>(fVirtualItems.ItemAt(i));
        if (item) {
            BEntry entry(&item->ref);
            _AddToMonitoring(entry);
        }
    }
    fItemsLock.Unlock();
    
    fIsMonitoring = true;
    LOG_INFO("VirtualFolder", "Started monitoring %s", fLocalPath.Path());
    
    return B_OK;
}

/**
 * @brief Stop monitoring the folder
 */
void
VirtualFolder::StopMonitoring()
{
    if (!fIsMonitoring) {
        return;
    }
    
    stop_watching(this);
    fIsMonitoring = false;
    
    LOG_INFO("VirtualFolder", "Stopped monitoring %s", fLocalPath.Path());
}

/**
 * @brief Handle filesystem notification messages
 */
void
VirtualFolder::MessageReceived(BMessage* message)
{
    switch (message->what) {
        case B_NODE_MONITOR:
        {
            int32 opcode;
            if (message->FindInt32("opcode", &opcode) == B_OK) {
                switch (opcode) {
                    case B_ENTRY_CREATED:
                        _HandleEntryCreated(message);
                        break;
                        
                    case B_ENTRY_REMOVED:
                        _HandleEntryRemoved(message);
                        break;
                        
                    case B_ENTRY_MOVED:
                        _HandleEntryMoved(message);
                        break;
                        
                    case B_STAT_CHANGED:
                        _HandleStatChanged(message);
                        break;
                        
                    case B_ATTR_CHANGED:
                        _HandleAttrChanged(message);
                        break;
                        
                    default:
                        break;
                }
            }
            break;
        }
        
        default:
            BHandler::MessageReceived(message);
            break;
    }
}

/**
 * @brief Get the sync state of a file or folder
 */
OneDriveSyncState
VirtualFolder::GetSyncState(const entry_ref& ref) const
{
    BAutolock lock(fItemsLock);
    
    for (int32 i = 0; i < fVirtualItems.CountItems(); i++) {
        VirtualItem* item = static_cast<VirtualItem*>(fVirtualItems.ItemAt(i));
        if (item && item->ref == ref) {
            return item->state;
        }
    }
    
    return kSyncStateUnknown;
}

/**
 * @brief Set the sync state of a file or folder
 */
void
VirtualFolder::SetSyncState(const entry_ref& ref, OneDriveSyncState state, 
                            bool updateIcon)
{
    BAutolock lock(fItemsLock);
    
    VirtualItem* item = NULL;
    for (int32 i = 0; i < fVirtualItems.CountItems(); i++) {
        VirtualItem* current = static_cast<VirtualItem*>(fVirtualItems.ItemAt(i));
        if (current && current->ref == ref) {
            item = current;
            break;
        }
    }
    
    if (!item) {
        // Create new item if not found
        item = new VirtualItem();
        item->ref = ref;
        item->state = state;
        fVirtualItems.AddItem(item);
    } else {
        item->state = state;
    }
    
    // Store state as attribute
    BNode node(&ref);
    if (node.InitCheck() == B_OK) {
        AttributeHelper::WriteInt32Attribute(node, kSyncStateAttr,
                                           static_cast<int32>(state));
    }
    
    lock.Unlock();
    
    if (updateIcon) {
        UpdateTrackerIcon(ref, state);
    }
    
    _SaveSyncState();
}

/**
 * @brief Get virtual item information
 */
status_t
VirtualFolder::GetVirtualItem(const entry_ref& ref, VirtualItem& item) const
{
    BAutolock lock(fItemsLock);
    
    for (int32 i = 0; i < fVirtualItems.CountItems(); i++) {
        VirtualItem* current = static_cast<VirtualItem*>(fVirtualItems.ItemAt(i));
        if (current && current->ref == ref) {
            item = *current;
            return B_OK;
        }
    }
    
    return B_ENTRY_NOT_FOUND;
}

/**
 * @brief Update virtual item information
 */
status_t
VirtualFolder::UpdateVirtualItem(const VirtualItem& item)
{
    BAutolock lock(fItemsLock);
    
    for (int32 i = 0; i < fVirtualItems.CountItems(); i++) {
        VirtualItem* current = static_cast<VirtualItem*>(fVirtualItems.ItemAt(i));
        if (current && current->ref == item.ref) {
            *current = item;
            
            // Update attributes
            BNode node(&item.ref);
            if (node.InitCheck() == B_OK) {
                // Sync state
                AttributeHelper::WriteInt32Attribute(node, kSyncStateAttr,
                                                   static_cast<int32>(item.state));
                
                // Cloud ID
                if (item.cloudId.Length() > 0) {
                    AttributeHelper::WriteStringAttribute(node, kCloudIdAttr,
                                                        item.cloudId);
                }
                
                // ETag
                if (item.eTag.Length() > 0) {
                    AttributeHelper::WriteStringAttribute(node, kETagAttr,
                                                        item.eTag);
                }
                
                // Pinned status
                AttributeHelper::WriteBoolAttribute(node, kPinnedAttr,
                                                  item.isPinned);
            }
            
            lock.Unlock();
            _SaveSyncState();
            return B_OK;
        }
    }
    
    // Item not found, add it
    VirtualItem* newItem = new VirtualItem(item);
    fVirtualItems.AddItem(newItem);
    
    lock.Unlock();
    _SaveSyncState();
    return B_OK;
}

/**
 * @brief Pin a file for offline access
 */
status_t
VirtualFolder::PinForOffline(const entry_ref& ref)
{
    VirtualItem item;
    status_t result = GetVirtualItem(ref, item);
    if (result != B_OK) {
        return result;
    }
    
    item.isPinned = true;
    result = UpdateVirtualItem(item);
    if (result != B_OK) {
        return result;
    }
    
    // Request download if online-only
    if (item.state == kSyncStateOnlineOnly) {
        return RequestDownload(ref);
    }
    
    return B_OK;
}

/**
 * @brief Unpin a file from offline access
 */
status_t
VirtualFolder::UnpinFromOffline(const entry_ref& ref)
{
    VirtualItem item;
    status_t result = GetVirtualItem(ref, item);
    if (result != B_OK) {
        return result;
    }
    
    item.isPinned = false;
    return UpdateVirtualItem(item);
}

/**
 * @brief Make a file online-only
 */
status_t
VirtualFolder::MakeOnlineOnly(const entry_ref& ref)
{
    VirtualItem item;
    status_t result = GetVirtualItem(ref, item);
    if (result != B_OK) {
        return result;
    }
    
    // Can't make pinned files online-only
    if (item.isPinned) {
        return B_NOT_ALLOWED;
    }
    
    // Remove local content (placeholder implementation)
    // In production, this would create a sparse file or placeholder
    item.state = kSyncStateOnlineOnly;
    return UpdateVirtualItem(item);
}

/**
 * @brief Request download of an online-only file
 */
status_t
VirtualFolder::RequestDownload(const entry_ref& ref)
{
    VirtualItem item;
    status_t result = GetVirtualItem(ref, item);
    if (result != B_OK) {
        return result;
    }
    
    if (item.state != kSyncStateOnlineOnly) {
        return B_OK; // Already downloaded
    }
    
    // Notify daemon to download
    BMessage request(kMsgDownloadFile);
    request.AddRef("ref", &ref);
    request.AddString("cloud_id", item.cloudId);
    
    // Notify daemon to download (placeholder for now)
    // TODO: Implement proper daemon notification mechanism
    if (fDaemon) {
        // Will be implemented when daemon interface is complete
    }
    
    // Update state to pending
    SetSyncState(ref, kSyncStatePending);
    
    return B_OK;
}

/**
 * @brief Scan folder contents and update sync states
 */
status_t
VirtualFolder::ScanContents()
{
    BDirectory dir(fLocalPath.Path());
    status_t result = dir.InitCheck();
    if (result != B_OK) {
        return result;
    }
    
    BEntry entry;
    while (dir.GetNextEntry(&entry) == B_OK) {
        if (entry.IsDirectory()) {
            // Skip hidden directories
            char name[B_FILE_NAME_LENGTH];
            if (entry.GetName(name) == B_OK && name[0] == kHiddenFilePrefix) {
                continue;
            }
        }
        
        VirtualItem item;
        result = _CreateVirtualItem(entry, item);
        if (result == B_OK) {
            UpdateVirtualItem(item);
        }
    }
    
    return B_OK;
}

/**
 * @brief Get list of items needing sync
 */
int32
VirtualFolder::GetPendingItems(BList& items) const
{
    BAutolock lock(fItemsLock);
    
    int32 count = 0;
    for (int32 i = 0; i < fVirtualItems.CountItems(); i++) {
        VirtualItem* item = static_cast<VirtualItem*>(fVirtualItems.ItemAt(i));
        if (item && (item->state == kSyncStatePending ||
                     item->state == kSyncStateError ||
                     item->state == kSyncStateConflict)) {
            items.AddItem(new VirtualItem(*item));
            count++;
        }
    }
    
    return count;
}

/**
 * @brief Update Tracker icon for a file
 */
void
VirtualFolder::UpdateTrackerIcon(const entry_ref& ref, OneDriveSyncState state)
{
    // Use SyncStateIcons to update the file icon
    status_t result = SyncStateIcons::Instance().UpdateFileIcon(ref, state);
    if (result != B_OK) {
        ErrorLogger::Instance().LogError("VirtualFolder", result,
                "Failed to update icon for file");
    }
    
    // Send Tracker update message to refresh the display
    BMessage update('ICON');
    update.AddRef("ref", &ref);
    be_roster->Broadcast(&update);
}

/**
 * @brief Get total size of local content
 */
off_t
VirtualFolder::GetLocalSize() const
{
    BAutolock lock(fItemsLock);
    
    off_t totalSize = 0;
    for (int32 i = 0; i < fVirtualItems.CountItems(); i++) {
        VirtualItem* item = static_cast<VirtualItem*>(fVirtualItems.ItemAt(i));
        if (item && !item->isFolder && 
            item->state != kSyncStateOnlineOnly) {
            totalSize += item->size;
        }
    }
    
    return totalSize;
}

/**
 * @brief Get number of items in folder
 */
int32
VirtualFolder::GetItemCount(bool includeSubfolders) const
{
    BAutolock lock(fItemsLock);
    
    if (includeSubfolders) {
        return fVirtualItems.CountItems();
    }
    
    // Count only direct children
    int32 count = 0;
    BPath folderPath(fLocalPath);
    
    for (int32 i = 0; i < fVirtualItems.CountItems(); i++) {
        VirtualItem* item = static_cast<VirtualItem*>(fVirtualItems.ItemAt(i));
        if (item) {
            BPath itemPath(&item->ref);
            BPath parent;
            itemPath.GetParent(&parent);
            if (parent == folderPath) {
                count++;
            }
        }
    }
    
    return count;
}

/**
 * @brief Handle file creation notification
 */
void
VirtualFolder::_HandleEntryCreated(BMessage* message)
{
    entry_ref ref;
    const char* name;
    
    if (message->FindInt32("device", &ref.device) != B_OK ||
        message->FindInt64("directory", &ref.directory) != B_OK ||
        message->FindString("name", &name) != B_OK) {
        return;
    }
    
    ref.set_name(name);
    
    BEntry entry(&ref);
    if (entry.InitCheck() != B_OK) {
        return;
    }
    
    // Create virtual item for new entry
    VirtualItem item;
    if (_CreateVirtualItem(entry, item) == B_OK) {
        item.state = kSyncStatePending;
        UpdateVirtualItem(item);
        
        // Start monitoring the new item
        _AddToMonitoring(entry);
        
        // Notify daemon
        BList changes;
        changes.AddItem(new VirtualItem(item));
        _NotifyDaemon(changes);
    }
}

/**
 * @brief Handle file removal notification
 */
void
VirtualFolder::_HandleEntryRemoved(BMessage* message)
{
    entry_ref ref;
    
    if (message->FindInt32("device", &ref.device) != B_OK ||
        message->FindInt64("node", &ref.directory) != B_OK) {
        return;
    }
    
    // Find and remove the item
    BAutolock lock(fItemsLock);
    
    for (int32 i = fVirtualItems.CountItems() - 1; i >= 0; i--) {
        VirtualItem* item = static_cast<VirtualItem*>(fVirtualItems.ItemAt(i));
        if (item && item->ref.device == ref.device) {
            // Stop monitoring
            _RemoveFromMonitoring(item->ref);
            
            // Notify daemon about deletion
            BList changes;
            item->state = kSyncStatePending; // Mark for deletion sync
            changes.AddItem(new VirtualItem(*item));
            _NotifyDaemon(changes);
            
            // Remove from list
            fVirtualItems.RemoveItem(i);
            delete item;
            break;
        }
    }
}

/**
 * @brief Handle file move/rename notification
 */
void
VirtualFolder::_HandleEntryMoved(BMessage* message)
{
    // Handle both the removal from old location and creation at new
    _HandleEntryRemoved(message);
    _HandleEntryCreated(message);
}

/**
 * @brief Handle file modification notification
 */
void
VirtualFolder::_HandleStatChanged(BMessage* message)
{
    node_ref nref;
    if (message->FindInt32("device", &nref.device) != B_OK ||
        message->FindInt64("node", &nref.node) != B_OK) {
        return;
    }
    
    // Find the item
    BAutolock lock(fItemsLock);
    
    for (int32 i = 0; i < fVirtualItems.CountItems(); i++) {
        VirtualItem* item = static_cast<VirtualItem*>(fVirtualItems.ItemAt(i));
        if (item) {
            BNode node(&item->ref);
            node_ref itemNodeRef;
            if (node.GetNodeRef(&itemNodeRef) == B_OK &&
                itemNodeRef == nref) {
                
                // Update modification time and size
                BEntry entry(&item->ref);
                struct stat st;
                if (entry.GetStat(&st) == B_OK) {
                    item->localModTime = st.st_mtime;
                    item->size = st.st_size;
                    
                    // Mark as pending sync
                    if (item->state == kSyncStateSynced) {
                        item->state = kSyncStatePending;
                    }
                    
                    lock.Unlock();
                    
                    // Notify daemon
                    BList changes;
                    changes.AddItem(new VirtualItem(*item));
                    _NotifyDaemon(changes);
                    
                    UpdateTrackerIcon(item->ref, item->state);
                }
                break;
            }
        }
    }
}

/**
 * @brief Handle attribute change notification
 */
void
VirtualFolder::_HandleAttrChanged(BMessage* message)
{
    // Similar to stat changed, but for attributes
    _HandleStatChanged(message);
}

/**
 * @brief Add item to monitoring
 */
status_t
VirtualFolder::_AddToMonitoring(const BEntry& entry)
{
    node_ref nref;
    status_t result = entry.GetNodeRef(&nref);
    if (result != B_OK) {
        return result;
    }
    
    uint32 flags = B_WATCH_STAT | B_WATCH_ATTR;
    if (entry.IsDirectory()) {
        flags |= B_WATCH_DIRECTORY;
    }
    
    return watch_node(&nref, flags, this);
}

/**
 * @brief Remove item from monitoring
 */
void
VirtualFolder::_RemoveFromMonitoring(const entry_ref& ref)
{
    BNode node(&ref);
    node_ref nref;
    if (node.GetNodeRef(&nref) == B_OK) {
        watch_node(&nref, B_STOP_WATCHING, this);
    }
}

/**
 * @brief Create virtual item from entry
 */
status_t
VirtualFolder::_CreateVirtualItem(const BEntry& entry, VirtualItem& item)
{
    entry_ref ref;
    status_t result = entry.GetRef(&ref);
    if (result != B_OK) {
        return result;
    }
    
    item.ref = ref;
    
    // Get basic info
    struct stat st;
    result = entry.GetStat(&st);
    if (result != B_OK) {
        return result;
    }
    
    item.isFolder = S_ISDIR(st.st_mode);
    item.size = st.st_size;
    item.localModTime = st.st_mtime;
    item.cloudModTime = 0;
    item.state = kSyncStateUnknown;
    item.isPinned = false;
    
    // Read stored attributes
    BNode node(&entry);
    if (node.InitCheck() == B_OK) {
        // Sync state
        int32 stateValue;
        if (AttributeHelper::ReadInt32Attribute(node, kSyncStateAttr,
                                              stateValue) == B_OK) {
            item.state = static_cast<OneDriveSyncState>(stateValue);
        }
        
        // Cloud ID
        AttributeHelper::ReadStringAttribute(node, kCloudIdAttr, item.cloudId);
        
        // ETag
        AttributeHelper::ReadStringAttribute(node, kETagAttr, item.eTag);
        
        // Pinned status
        AttributeHelper::ReadBoolAttribute(node, kPinnedAttr, item.isPinned);
    }
    
    return B_OK;
}

/**
 * @brief Load sync state from cache
 */
status_t
VirtualFolder::_LoadSyncState()
{
    // In production, this would load from a persistent cache
    // For now, we just scan the folder
    return B_OK;
}

/**
 * @brief Save sync state to cache
 */
status_t
VirtualFolder::_SaveSyncState()
{
    // In production, this would save to a persistent cache
    // For now, attributes are sufficient
    return B_OK;
}

/**
 * @brief Notify daemon of changes
 */
void
VirtualFolder::_NotifyDaemon(const BList& changes)
{
    if (!fDaemon) {
        return;
    }
    
    // TODO: Implement proper daemon notification mechanism
    // For now, just clean up the temporary items
    
    BMessage notification(kMsgSyncRequired);
    notification.AddString("folder", fLocalPath.Path());
    notification.AddInt32("change_count", changes.CountItems());
    
    // Add change details
    for (int32 i = 0; i < changes.CountItems(); i++) {
        VirtualItem* item = static_cast<VirtualItem*>(changes.ItemAt(i));
        if (item) {
            BMessage change;
            change.AddRef("ref", &item->ref);
            change.AddString("cloud_id", item->cloudId);
            change.AddInt32("state", item->state);
            change.AddBool("is_folder", item->isFolder);
            
            notification.AddMessage("change", &change);
        }
    }
    
    // Will be implemented when daemon interface is complete
    
    // Clean up temporary items
    for (int32 i = 0; i < changes.CountItems(); i++) {
        delete static_cast<VirtualItem*>(changes.ItemAt(i));
    }
}