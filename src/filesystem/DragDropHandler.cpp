/**
 * @file DragDropHandler.cpp
 * @brief Implementation of drag and drop support for OneDrive
 * @author Haiku OneDrive Team
 * @version 1.0.0
 * @date 2025-08-15
 */

#include "DragDropHandler.h"

#include <Alert.h>
#include <Autolock.h>
#include <Catalog.h>
#include <Directory.h>
#include <FindDirectory.h>
#include <Messenger.h>
#include <NodeInfo.h>
#include <Path.h>
#include <Roster.h>

#include "../shared/OneDriveConstants.h"
#include "../shared/ErrorLogger.h"
#include "../shared/FileSystemConstants.h"

#include <stdio.h>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "DragDropHandler"

using namespace OneDrive;
using namespace OneDrive::FileSystem;

// Static member
DragDropHandler* OneDriveTrackerView::sDragDropHandler = NULL;

// OneDrive folder path (should match TrackerAddon)
static BPath sOneDrivePath;
static bool sOneDrivePathInitialized = false;

/**
 * @brief Initialize OneDrive path
 */
static void
InitializeOneDrivePath()
{
    if (sOneDrivePathInitialized) {
        return;
    }
    
    BPath homePath;
    if (find_directory(B_USER_DIRECTORY, &homePath) == B_OK) {
        sOneDrivePath = homePath;
        sOneDrivePath.Append("OneDrive");
    }
    
    sOneDrivePathInitialized = true;
}

/**
 * @brief Constructor
 */
DragDropHandler::DragDropHandler()
    : BHandler("DragDropHandler"),
      fNextOperationId(1)
{
    InitializeOneDrivePath();
}

/**
 * @brief Destructor
 */
DragDropHandler::~DragDropHandler()
{
    // Cancel all active operations
    BAutolock lock(fLock);
    for (int32 i = 0; i < fActiveOperations.CountItems(); i++) {
        DragDropInfo* info = static_cast<DragDropInfo*>(fActiveOperations.ItemAt(i));
        delete info;
    }
    fActiveOperations.MakeEmpty();
}

/**
 * @brief Handle drag and drop message
 */
void
DragDropHandler::MessageReceived(BMessage* message)
{
    switch (message->what) {
        case B_SIMPLE_DATA:
        case B_REFS_RECEIVED:
        {
            // Extract drop target
            entry_ref destination;
            if (message->FindRef("directory", &destination) != B_OK) {
                LOG_WARNING("DragDropHandler", "No destination in drop message");
                break;
            }
            
            // Get keyboard modifiers
            uint32 modifiers = 0;
            message->FindInt32("modifiers", (int32*)&modifiers);
            
            // Process the drop
            ProcessDrop(message, destination, modifiers);
            break;
        }
        
        default:
            BHandler::MessageReceived(message);
            break;
    }
}

/**
 * @brief Check if path is OneDrive path
 */
bool
DragDropHandler::IsOneDrivePath(const entry_ref& ref)
{
    BPath path(&ref);
    if (path.InitCheck() != B_OK) {
        return false;
    }
    
    BString pathStr(path.Path());
    BString oneDriveStr(sOneDrivePath.Path());
    
    return pathStr.StartsWith(oneDriveStr);
}

/**
 * @brief Process drop operation
 */
status_t
DragDropHandler::ProcessDrop(BMessage* refs, const entry_ref& destination,
                            uint32 modifiers)
{
    if (!refs) {
        return B_BAD_VALUE;
    }
    
    // Check if destination is OneDrive
    bool isOneDriveTarget = IsOneDrivePath(destination);
    
    // Process each dropped item
    entry_ref ref;
    int32 itemCount = 0;
    DragDropOperation commonOp = kDragDropCopy;
    
    for (int32 i = 0; refs->FindRef("refs", i, &ref) == B_OK; i++) {
        DragDropInfo* info = new DragDropInfo();
        info->source = ref;
        info->destination = destination;
        info->isOneDriveSource = IsOneDrivePath(ref);
        info->isOneDriveTarget = isOneDriveTarget;
        
        // Determine operation
        info->operation = _DetermineOperation(ref, destination, modifiers);
        if (i == 0) {
            commonOp = info->operation;
        }
        
        // Get file size
        BEntry entry(&ref);
        struct stat st;
        if (entry.GetStat(&st) == B_OK) {
            info->size = st.st_size;
        }
        
        // Handle the operation
        status_t result = B_OK;
        
        if (info->isOneDriveSource && info->isOneDriveTarget) {
            // Move/copy within OneDrive
            if (info->operation == kDragDropMove) {
                result = _HandleOneDriveMove(*info);
            } else {
                result = _HandleOneDriveCopy(*info);
            }
        } else if (!info->isOneDriveSource && info->isOneDriveTarget) {
            // Upload to OneDrive
            result = _HandleUpload(*info);
        } else if (info->isOneDriveSource && !info->isOneDriveTarget) {
            // Download from OneDrive
            result = _HandleDownload(*info);
        }
        
        if (result == B_OK) {
            BAutolock lock(fLock);
            fActiveOperations.AddItem(info);
            itemCount++;
        } else {
            delete info;
        }
    }
    
    // Show feedback
    if (itemCount > 0) {
        _ShowOperationFeedback(commonOp, itemCount);
    }
    
    return B_OK;
}

/**
 * @brief Cancel ongoing operation
 */
void
DragDropHandler::CancelOperation(int32 operation)
{
    // TODO: Implement operation cancellation
    LOG_INFO("DragDropHandler", "Cancelling operation %d", operation);
}

/**
 * @brief Get operation progress
 */
bool
DragDropHandler::GetProgress(int32 operation, off_t& completed, off_t& total)
{
    // TODO: Implement progress tracking
    return false;
}

/**
 * @brief Determine operation type
 */
DragDropOperation
DragDropHandler::_DetermineOperation(const entry_ref& source,
                                    const entry_ref& destination,
                                    uint32 modifiers)
{
    bool isOneDriveSource = IsOneDrivePath(source);
    bool isOneDriveTarget = IsOneDrivePath(destination);
    
    // Check keyboard modifiers
    if (modifiers & B_OPTION_KEY) {
        return kDragDropCopy;  // Option key forces copy
    }
    
    if (modifiers & B_COMMAND_KEY) {
        return kDragDropLink;  // Command key creates link
    }
    
    // Default behavior
    if (isOneDriveSource && isOneDriveTarget) {
        // Within OneDrive: default to move
        return kDragDropMove;
    } else if (!isOneDriveSource && isOneDriveTarget) {
        // To OneDrive: default to upload (copy)
        return kDragDropUpload;
    } else if (isOneDriveSource && !isOneDriveTarget) {
        // From OneDrive: default to download (copy)
        return kDragDropDownload;
    }
    
    return kDragDropCopy;
}

/**
 * @brief Handle upload operation
 */
status_t
DragDropHandler::_HandleUpload(const DragDropInfo& info)
{
    LOG_INFO("DragDropHandler", "Uploading file to OneDrive");
    
    // Send upload request to daemon
    BMessage uploadMsg(kMsgSyncRequired);
    uploadMsg.AddRef("source", &info.source);
    uploadMsg.AddRef("destination", &info.destination);
    uploadMsg.AddInt32("operation", kDragDropUpload);
    
    BMessenger daemon(APP_SIGNATURE);
    return daemon.SendMessage(&uploadMsg);
}

/**
 * @brief Handle download operation
 */
status_t
DragDropHandler::_HandleDownload(const DragDropInfo& info)
{
    LOG_INFO("DragDropHandler", "Downloading file from OneDrive");
    
    // Send download request to daemon
    BMessage downloadMsg(kMsgDownloadFile);
    downloadMsg.AddRef("source", &info.source);
    downloadMsg.AddRef("destination", &info.destination);
    downloadMsg.AddBool("move", false);
    
    BMessenger daemon(APP_SIGNATURE);
    return daemon.SendMessage(&downloadMsg);
}

/**
 * @brief Handle move within OneDrive
 */
status_t
DragDropHandler::_HandleOneDriveMove(const DragDropInfo& info)
{
    LOG_INFO("DragDropHandler", "Moving file within OneDrive");
    
    // Local move first
    BEntry entry(&info.source);
    BDirectory destDir(&info.destination);
    
    status_t result = entry.MoveTo(&destDir);
    if (result != B_OK) {
        ErrorLogger::Instance().LogError("DragDropHandler", result, "Failed to move file");
        return result;
    }
    
    // Notify daemon of the move
    BMessage moveMsg('move');
    moveMsg.AddRef("source", &info.source);
    moveMsg.AddRef("destination", &info.destination);
    
    BMessenger daemon(APP_SIGNATURE);
    daemon.SendMessage(&moveMsg);
    
    return B_OK;
}

/**
 * @brief Handle copy within OneDrive
 */
status_t
DragDropHandler::_HandleOneDriveCopy(const DragDropInfo& info)
{
    LOG_INFO("DragDropHandler", "Copying file within OneDrive");
    
    // Send copy request to daemon (it will handle the cloud copy)
    BMessage copyMsg('copy');
    copyMsg.AddRef("source", &info.source);
    copyMsg.AddRef("destination", &info.destination);
    
    BMessenger daemon(APP_SIGNATURE);
    return daemon.SendMessage(&copyMsg);
}

/**
 * @brief Show operation feedback
 */
void
DragDropHandler::_ShowOperationFeedback(DragDropOperation operation, int32 itemCount)
{
    BString message;
    const char* opName = "";
    
    switch (operation) {
        case kDragDropUpload:
            opName = B_TRANSLATE("Uploading");
            break;
        case kDragDropDownload:
            opName = B_TRANSLATE("Downloading");
            break;
        case kDragDropMove:
            opName = B_TRANSLATE("Moving");
            break;
        case kDragDropCopy:
            opName = B_TRANSLATE("Copying");
            break;
        default:
            break;
    }
    
    if (itemCount == 1) {
        message.SetToFormat(B_TRANSLATE("%s 1 item to OneDrive"), opName);
    } else {
        message.SetToFormat(B_TRANSLATE("%s %d items to OneDrive"), opName, itemCount);
    }
    
    // TODO: Show notification instead of alert
    LOG_INFO("DragDropHandler", message.String());
}

/**
 * @brief Update operation progress
 */
void
DragDropHandler::_UpdateProgress(int32 operation, off_t completed, off_t total)
{
    // TODO: Update progress display
}

/**
 * @brief Send operation to daemon
 */
int32
DragDropHandler::_SendToDaemon(const DragDropInfo& info)
{
    int32 opId = fNextOperationId++;
    
    BMessage msg('drop');
    msg.AddInt32("operation_id", opId);
    msg.AddRef("source", &info.source);
    msg.AddRef("destination", &info.destination);
    msg.AddInt32("operation", info.operation);
    
    BMessenger daemon(APP_SIGNATURE);
    daemon.SendMessage(&msg);
    
    return opId;
}

// OneDriveTrackerView implementation

/**
 * @brief Install drag and drop support in Tracker
 */
status_t
OneDriveTrackerView::InstallDragDropSupport()
{
    if (sDragDropHandler) {
        return B_OK; // Already installed
    }
    
    sDragDropHandler = new DragDropHandler();
    
    // TODO: Hook into Tracker's drag and drop system
    // This would require Tracker modifications or a more sophisticated approach
    
    LOG_INFO("DragDropHandler", "Drag and drop support installed");
    return B_OK;
}

/**
 * @brief Remove drag and drop support from Tracker
 */
void
OneDriveTrackerView::RemoveDragDropSupport()
{
    if (sDragDropHandler) {
        delete sDragDropHandler;
        sDragDropHandler = NULL;
        
        LOG_INFO("DragDropHandler", "Drag and drop support removed");
    }
}

/**
 * @brief Check if drag and drop is supported
 */
bool
OneDriveTrackerView::IsDragDropSupported()
{
    return (sDragDropHandler != NULL);
}