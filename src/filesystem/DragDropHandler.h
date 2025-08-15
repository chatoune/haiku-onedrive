/**
 * @file DragDropHandler.h
 * @brief Drag and drop support for OneDrive folders in Tracker
 * @author Haiku OneDrive Team
 * @version 1.0.0
 * @date 2025-08-15
 * 
 * This class handles drag and drop operations for OneDrive folders,
 * including file uploads, downloads, and move operations.
 */

#ifndef DRAG_DROP_HANDLER_H
#define DRAG_DROP_HANDLER_H

#include <Entry.h>
#include <Handler.h>
#include <List.h>
#include <Locker.h>
#include <Message.h>
#include <String.h>

namespace OneDrive {

/**
 * @brief Drag and drop operation types
 */
enum DragDropOperation {
    kDragDropCopy = 0,      ///< Copy files to OneDrive
    kDragDropMove,          ///< Move files to OneDrive
    kDragDropLink,          ///< Create link in OneDrive
    kDragDropDownload,      ///< Download from OneDrive
    kDragDropUpload         ///< Upload to OneDrive
};

/**
 * @brief Drag and drop operation info
 */
struct DragDropInfo {
    entry_ref source;           ///< Source file/folder
    entry_ref destination;      ///< Destination folder
    DragDropOperation operation;///< Operation type
    bool isOneDriveSource;      ///< Source is in OneDrive
    bool isOneDriveTarget;      ///< Target is in OneDrive
    off_t size;                 ///< File size (for progress)
};

/**
 * @brief Handles drag and drop operations for OneDrive
 * 
 * This class processes drag and drop operations between OneDrive
 * folders and local filesystem, handling uploads, downloads, and
 * moves appropriately.
 * 
 * @see VirtualFolder
 * @since 1.0.0
 */
class DragDropHandler : public BHandler {
public:
    /**
     * @brief Constructor
     */
    DragDropHandler();
    
    /**
     * @brief Destructor
     */
    virtual ~DragDropHandler();
    
    /**
     * @brief Handle drag and drop message
     * 
     * @param message Drag and drop message from Tracker
     */
    virtual void MessageReceived(BMessage* message);
    
    /**
     * @brief Check if path is OneDrive path
     * 
     * @param ref Entry reference to check
     * @return true if path is within OneDrive folder
     */
    static bool IsOneDrivePath(const entry_ref& ref);
    
    /**
     * @brief Process drop operation
     * 
     * @param refs Files being dropped
     * @param destination Drop target folder
     * @param modifiers Keyboard modifiers (for copy/move/link)
     * @return B_OK on success
     */
    status_t ProcessDrop(BMessage* refs, const entry_ref& destination,
                        uint32 modifiers);
    
    /**
     * @brief Cancel ongoing operation
     * 
     * @param operation Operation ID to cancel
     */
    void CancelOperation(int32 operation);
    
    /**
     * @brief Get operation progress
     * 
     * @param operation Operation ID
     * @param completed Bytes completed
     * @param total Total bytes
     * @return true if operation is active
     */
    bool GetProgress(int32 operation, off_t& completed, off_t& total);

private:
    /**
     * @brief Determine operation type
     * 
     * @param source Source file
     * @param destination Destination folder
     * @param modifiers Keyboard modifiers
     * @return Operation type
     */
    DragDropOperation _DetermineOperation(const entry_ref& source,
                                         const entry_ref& destination,
                                         uint32 modifiers);
    
    /**
     * @brief Handle upload operation
     * 
     * @param info Operation info
     * @return B_OK on success
     */
    status_t _HandleUpload(const DragDropInfo& info);
    
    /**
     * @brief Handle download operation
     * 
     * @param info Operation info
     * @return B_OK on success
     */
    status_t _HandleDownload(const DragDropInfo& info);
    
    /**
     * @brief Handle move within OneDrive
     * 
     * @param info Operation info
     * @return B_OK on success
     */
    status_t _HandleOneDriveMove(const DragDropInfo& info);
    
    /**
     * @brief Handle copy within OneDrive
     * 
     * @param info Operation info
     * @return B_OK on success
     */
    status_t _HandleOneDriveCopy(const DragDropInfo& info);
    
    /**
     * @brief Show operation feedback
     * 
     * @param operation Operation type
     * @param itemCount Number of items
     */
    void _ShowOperationFeedback(DragDropOperation operation, int32 itemCount);
    
    /**
     * @brief Update operation progress
     * 
     * @param operation Operation ID
     * @param completed Bytes completed
     * @param total Total bytes
     */
    void _UpdateProgress(int32 operation, off_t completed, off_t total);
    
    /**
     * @brief Send operation to daemon
     * 
     * @param info Operation info
     * @return Operation ID
     */
    int32 _SendToDaemon(const DragDropInfo& info);

private:
    BList fActiveOperations;    ///< List of active operations
    mutable BLocker fLock;      ///< Thread safety lock
    int32 fNextOperationId;     ///< Next operation ID
};

/**
 * @brief Tracker view extension for drag and drop
 * 
 * This class extends Tracker views to handle OneDrive-specific
 * drag and drop operations.
 */
class OneDriveTrackerView {
public:
    /**
     * @brief Install drag and drop support in Tracker
     * 
     * @return B_OK on success
     */
    static status_t InstallDragDropSupport();
    
    /**
     * @brief Remove drag and drop support from Tracker
     */
    static void RemoveDragDropSupport();
    
    /**
     * @brief Check if drag and drop is supported
     * 
     * @return true if installed
     */
    static bool IsDragDropSupported();

private:
    static DragDropHandler* sDragDropHandler;
};

} // namespace OneDrive

#endif // DRAG_DROP_HANDLER_H