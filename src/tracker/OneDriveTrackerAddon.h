/**
 * @file OneDriveTrackerAddon.h
 * @brief Tracker add-on for OneDrive context menu integration
 * @author Haiku OneDrive Team
 * @version 1.0.0
 * @date 2025-08-15
 * 
 * This add-on provides context menu items in Tracker for OneDrive operations
 * such as sync status, pinning files for offline access, and sharing.
 */

#ifndef ONEDRIVE_TRACKER_ADDON_H
#define ONEDRIVE_TRACKER_ADDON_H

#include <Entry.h>
#include <Menu.h>
#include <MenuItem.h>
#include <Message.h>
#include <String.h>

namespace OneDrive {

/**
 * @brief Message constants for Tracker add-on commands
 */
enum {
    kMsgSyncNow = 'sync',           ///< Force sync of selected items
    kMsgPinOffline = 'pino',        ///< Pin for offline access
    kMsgUnpinOffline = 'unpo',      ///< Unpin from offline access
    kMsgMakeOnlineOnly = 'onlo',   ///< Make file online-only
    kMsgShareLink = 'shrl',         ///< Share OneDrive link
    kMsgViewOnline = 'view',        ///< View in OneDrive web
    kMsgShowSyncStatus = 'stat',   ///< Show sync status window
    kMsgResolveConflict = 'conf',  ///< Resolve sync conflict
    kMsgViewHistory = 'hist',       ///< View version history
    kMsgExcludeFromSync = 'excl'   ///< Exclude from sync
};

/**
 * @brief Tracker context menu builder
 * 
 * Creates appropriate menu items based on the selected files
 * and their current sync state.
 */
class OneDriveMenuBuilder {
public:
    /**
     * @brief Build context menu for selected items
     * 
     * @param refs Message containing selected entry_refs
     * @param menu Menu to populate
     * @param handler Target handler for menu messages
     * @return B_OK on success
     */
    static status_t BuildMenu(BMessage* refs, BMenu* menu, BHandler* handler);
    
    /**
     * @brief Check if path is within OneDrive sync folder
     * 
     * @param ref Entry reference to check
     * @return true if within sync folder
     */
    static bool IsOneDrivePath(const entry_ref& ref);
    
    /**
     * @brief Get sync state for entry
     * 
     * @param ref Entry reference
     * @return Sync state or kSyncStateUnknown
     */
    static int32 GetSyncState(const entry_ref& ref);
    
private:
    /**
     * @brief Add sync submenu
     * 
     * @param menu Parent menu
     * @param refs Selected items
     * @param handler Target handler
     */
    static void _AddSyncSubmenu(BMenu* menu, BMessage* refs, BHandler* handler);
    
    /**
     * @brief Add offline access submenu
     * 
     * @param menu Parent menu
     * @param refs Selected items
     * @param handler Target handler
     */
    static void _AddOfflineSubmenu(BMenu* menu, BMessage* refs, BHandler* handler);
    
    /**
     * @brief Add sharing submenu
     * 
     * @param menu Parent menu
     * @param refs Selected items
     * @param handler Target handler
     */
    static void _AddSharingSubmenu(BMenu* menu, BMessage* refs, BHandler* handler);
    
    /**
     * @brief Create menu item with icon
     * 
     * @param label Menu item label
     * @param message Menu item message
     * @param iconName Icon resource name
     * @param enabled Whether item is enabled
     * @return Created menu item
     */
    static BMenuItem* _CreateMenuItem(const char* label, BMessage* message,
                                     const char* iconName = NULL,
                                     bool enabled = true);
    
    /**
     * @brief Check if all refs have same state
     * 
     * @param refs Entry references
     * @param state State to check
     * @return true if all have the same state
     */
    static bool _AllHaveState(BMessage* refs, int32 state);
    
    /**
     * @brief Check if any refs have state
     * 
     * @param refs Entry references
     * @param state State to check
     * @return true if any have the state
     */
    static bool _AnyHaveState(BMessage* refs, int32 state);
};

/**
 * @brief Message handler for Tracker add-on
 * 
 * Processes messages from Tracker context menu items.
 */
class OneDriveMessageHandler {
public:
    /**
     * @brief Handle message from Tracker
     * 
     * @param message Message to handle
     * @return B_OK if handled
     */
    static status_t HandleMessage(BMessage* message);
    
private:
    /**
     * @brief Handle sync now command
     * 
     * @param refs Selected items to sync
     */
    static void _HandleSyncNow(BMessage* refs);
    
    /**
     * @brief Handle pin offline command
     * 
     * @param refs Items to pin
     */
    static void _HandlePinOffline(BMessage* refs);
    
    /**
     * @brief Handle unpin offline command
     * 
     * @param refs Items to unpin
     */
    static void _HandleUnpinOffline(BMessage* refs);
    
    /**
     * @brief Handle make online-only command
     * 
     * @param refs Items to make online-only
     */
    static void _HandleMakeOnlineOnly(BMessage* refs);
    
    /**
     * @brief Handle share link command
     * 
     * @param refs Items to share
     */
    static void _HandleShareLink(BMessage* refs);
    
    /**
     * @brief Handle view online command
     * 
     * @param refs Items to view online
     */
    static void _HandleViewOnline(BMessage* refs);
    
    /**
     * @brief Show sync status window
     */
    static void _ShowSyncStatus();
    
    /**
     * @brief Handle resolve conflict command
     * 
     * @param refs Conflicted items
     */
    static void _HandleResolveConflict(BMessage* refs);
    
    /**
     * @brief Send message to daemon
     * 
     * @param message Message to send
     * @return B_OK on success
     */
    static status_t _SendToDaemon(BMessage* message);
};

} // namespace OneDrive

#endif // ONEDRIVE_TRACKER_ADDON_H