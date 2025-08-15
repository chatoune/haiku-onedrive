/**
 * @file OneDriveTrackerAddon.cpp
 * @brief Implementation of Tracker add-on for OneDrive
 * @author Haiku OneDrive Team
 * @version 1.0.0
 * @date 2025-08-15
 */

#include "OneDriveTrackerAddon.h"

#include <Alert.h>
#include <Catalog.h>
#include <Clipboard.h>
#include <Directory.h>
#include <FindDirectory.h>
#include <MenuItem.h>
#include <Messenger.h>
#include <NodeInfo.h>
#include <Path.h>
#include <Roster.h>
#include <fs_attr.h>

#include "../shared/OneDriveConstants.h"
#include "../shared/ErrorLogger.h"
#include "../shared/AttributeHelper.h"

#include <stdio.h>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "OneDriveTrackerAddon"

using namespace OneDrive;

// Attribute names
static const char* kSyncStateAttr = "OneDrive:SyncState";
static const char* kPinnedAttr = "OneDrive:Pinned";

// OneDrive folder path (should be configurable)
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
    
    // Default to ~/OneDrive
    BPath homePath;
    if (find_directory(B_USER_DIRECTORY, &homePath) == B_OK) {
        sOneDrivePath = homePath;
        sOneDrivePath.Append("OneDrive");
    }
    
    sOneDrivePathInitialized = true;
}

// Export C functions for Tracker
extern "C" {

/**
 * @brief Process selected refs (not used for context menu)
 */
void
process_refs(entry_ref directory, BMessage* refs, void* reserved)
{
    // This is called when the add-on is invoked directly
    // We use populate_menu for context menu integration
}

/**
 * @brief Populate context menu
 */
void
populate_menu(BMessage* msg, BMenu* menu, BHandler* handler)
{
    if (!msg || !menu || !handler) {
        return;
    }
    
    InitializeOneDrivePath();
    
    // Build OneDrive menu items
    OneDriveMenuBuilder::BuildMenu(msg, menu, handler);
}

/**
 * @brief Handle messages from Tracker
 */
void
message_received(BMessage* msg)
{
    if (!msg) {
        return;
    }
    
    OneDriveMessageHandler::HandleMessage(msg);
}

} // extern "C"

// OneDriveMenuBuilder implementation

/**
 * @brief Build context menu for selected items
 */
status_t
OneDriveMenuBuilder::BuildMenu(BMessage* refs, BMenu* menu, BHandler* handler)
{
    if (!refs || !menu) {
        return B_BAD_VALUE;
    }
    
    // Check if any selected items are in OneDrive folder
    entry_ref ref;
    bool hasOneDriveItems = false;
    
    for (int32 i = 0; refs->FindRef("refs", i, &ref) == B_OK; i++) {
        if (IsOneDrivePath(ref)) {
            hasOneDriveItems = true;
            break;
        }
    }
    
    if (!hasOneDriveItems) {
        return B_OK; // No OneDrive items selected
    }
    
    // Add separator if menu already has items
    if (menu->CountItems() > 0) {
        menu->AddSeparatorItem();
    }
    
    // Create OneDrive submenu
    BMenu* oneDriveMenu = new BMenu(B_TRANSLATE("OneDrive"));
    
    // Add submenus
    _AddSyncSubmenu(oneDriveMenu, refs, handler);
    oneDriveMenu->AddSeparatorItem();
    _AddOfflineSubmenu(oneDriveMenu, refs, handler);
    oneDriveMenu->AddSeparatorItem();
    _AddSharingSubmenu(oneDriveMenu, refs, handler);
    
    menu->AddItem(oneDriveMenu);
    
    return B_OK;
}

/**
 * @brief Check if path is within OneDrive sync folder
 */
bool
OneDriveMenuBuilder::IsOneDrivePath(const entry_ref& ref)
{
    BPath path(&ref);
    if (path.InitCheck() != B_OK) {
        return false;
    }
    
    // Check if path starts with OneDrive folder
    BString pathStr(path.Path());
    BString oneDriveStr(sOneDrivePath.Path());
    
    return pathStr.StartsWith(oneDriveStr);
}

/**
 * @brief Get sync state for entry
 */
int32
OneDriveMenuBuilder::GetSyncState(const entry_ref& ref)
{
    BNode node(&ref);
    if (node.InitCheck() != B_OK) {
        return 0; // kSyncStateUnknown
    }
    
    int32 state = 0;
    AttributeHelper::ReadInt32Attribute(node, kSyncStateAttr, state);
    
    return state;
}

/**
 * @brief Add sync submenu
 */
void
OneDriveMenuBuilder::_AddSyncSubmenu(BMenu* menu, BMessage* refs, BHandler* handler)
{
    // Sync Now
    BMessage* syncMsg = new BMessage(kMsgSyncNow);
    // Copy refs to message
    entry_ref ref;
    for (int32 i = 0; refs->FindRef("refs", i, &ref) == B_OK; i++) {
        syncMsg->AddRef("refs", &ref);
    }
    BMenuItem* syncItem = _CreateMenuItem(B_TRANSLATE("Sync now"), syncMsg);
    menu->AddItem(syncItem);
    
    // Show Sync Status
    BMessage* statusMsg = new BMessage(kMsgShowSyncStatus);
    BMenuItem* statusItem = _CreateMenuItem(B_TRANSLATE("Show sync status"), statusMsg);
    menu->AddItem(statusItem);
    
    // Resolve Conflicts (only if conflicts exist)
    if (_AnyHaveState(refs, 5)) { // kSyncStateConflict
        BMessage* conflictMsg = new BMessage(kMsgResolveConflict);
        // Copy refs to message
        entry_ref ref;
        for (int32 i = 0; refs->FindRef("refs", i, &ref) == B_OK; i++) {
            conflictMsg->AddRef("refs", &ref);
        }
        BMenuItem* conflictItem = _CreateMenuItem(B_TRANSLATE("Resolve conflicts"), 
                                                  conflictMsg);
        menu->AddItem(conflictItem);
    }
}

/**
 * @brief Add offline access submenu
 */
void
OneDriveMenuBuilder::_AddOfflineSubmenu(BMenu* menu, BMessage* refs, BHandler* handler)
{
    bool hasPinned = false;
    bool hasUnpinned = false;
    bool hasOnlineOnly = false;
    
    // Check states of selected items
    entry_ref ref;
    for (int32 i = 0; refs->FindRef("refs", i, &ref) == B_OK; i++) {
        BNode node(&ref);
        if (node.InitCheck() == B_OK) {
            bool isPinned = false;
            AttributeHelper::ReadBoolAttribute(node, kPinnedAttr, isPinned);
            
            if (isPinned) {
                hasPinned = true;
            } else {
                hasUnpinned = true;
            }
            
            if (GetSyncState(ref) == 7) { // kSyncStateOnlineOnly
                hasOnlineOnly = true;
            }
        }
    }
    
    // Pin for offline
    if (hasUnpinned) {
        BMessage* pinMsg = new BMessage(kMsgPinOffline);
        // Copy refs to message
        entry_ref ref;
        for (int32 i = 0; refs->FindRef("refs", i, &ref) == B_OK; i++) {
            pinMsg->AddRef("refs", &ref);
        }
        BMenuItem* pinItem = _CreateMenuItem(B_TRANSLATE("Always keep on this device"), 
                                           pinMsg);
        menu->AddItem(pinItem);
    }
    
    // Unpin from offline
    if (hasPinned) {
        BMessage* unpinMsg = new BMessage(kMsgUnpinOffline);
        // Copy refs to message
        entry_ref ref;
        for (int32 i = 0; refs->FindRef("refs", i, &ref) == B_OK; i++) {
            unpinMsg->AddRef("refs", &ref);
        }
        BMenuItem* unpinItem = _CreateMenuItem(B_TRANSLATE("Free up space"), 
                                             unpinMsg);
        menu->AddItem(unpinItem);
    }
    
    // Make online-only
    if (!hasOnlineOnly && hasUnpinned) {
        BMessage* onlineMsg = new BMessage(kMsgMakeOnlineOnly);
        // Copy refs to message
        entry_ref ref;
        for (int32 i = 0; refs->FindRef("refs", i, &ref) == B_OK; i++) {
            onlineMsg->AddRef("refs", &ref);
        }
        BMenuItem* onlineItem = _CreateMenuItem(B_TRANSLATE("Make available online-only"), 
                                              onlineMsg);
        menu->AddItem(onlineItem);
    }
}

/**
 * @brief Add sharing submenu
 */
void
OneDriveMenuBuilder::_AddSharingSubmenu(BMenu* menu, BMessage* refs, BHandler* handler)
{
    entry_ref ref;
    
    // Share link
    BMessage* shareMsg = new BMessage(kMsgShareLink);
    // Copy refs to message
    for (int32 i = 0; refs->FindRef("refs", i, &ref) == B_OK; i++) {
        shareMsg->AddRef("refs", &ref);
    }
    BMenuItem* shareItem = _CreateMenuItem(B_TRANSLATE("Share OneDrive link"), 
                                         shareMsg);
    menu->AddItem(shareItem);
    
    // View online
    BMessage* viewMsg = new BMessage(kMsgViewOnline);
    // Copy refs to message
    for (int32 i = 0; refs->FindRef("refs", i, &ref) == B_OK; i++) {
        viewMsg->AddRef("refs", &ref);
    }
    BMenuItem* viewItem = _CreateMenuItem(B_TRANSLATE("View on OneDrive.com"), 
                                        viewMsg);
    menu->AddItem(viewItem);
    
    // View history
    BMessage* historyMsg = new BMessage(kMsgViewHistory);
    // Copy refs to message
    for (int32 i = 0; refs->FindRef("refs", i, &ref) == B_OK; i++) {
        historyMsg->AddRef("refs", &ref);
    }
    BMenuItem* historyItem = _CreateMenuItem(B_TRANSLATE("Version history"), 
                                           historyMsg);
    menu->AddItem(historyItem);
}

/**
 * @brief Create menu item with icon
 */
BMenuItem*
OneDriveMenuBuilder::_CreateMenuItem(const char* label, BMessage* message,
                                    const char* iconName, bool enabled)
{
    BMenuItem* item = new BMenuItem(label, message);
    item->SetEnabled(enabled);
    
    // TODO: Add icon support when resources are available
    
    return item;
}

/**
 * @brief Check if all refs have same state
 */
bool
OneDriveMenuBuilder::_AllHaveState(BMessage* refs, int32 state)
{
    entry_ref ref;
    for (int32 i = 0; refs->FindRef("refs", i, &ref) == B_OK; i++) {
        if (GetSyncState(ref) != state) {
            return false;
        }
    }
    return true;
}

/**
 * @brief Check if any refs have state
 */
bool
OneDriveMenuBuilder::_AnyHaveState(BMessage* refs, int32 state)
{
    entry_ref ref;
    for (int32 i = 0; refs->FindRef("refs", i, &ref) == B_OK; i++) {
        if (GetSyncState(ref) == state) {
            return true;
        }
    }
    return false;
}

// OneDriveMessageHandler implementation

/**
 * @brief Handle message from Tracker
 */
status_t
OneDriveMessageHandler::HandleMessage(BMessage* message)
{
    if (!message) {
        return B_BAD_VALUE;
    }
    
    switch (message->what) {
        case kMsgSyncNow:
            _HandleSyncNow(message);
            break;
            
        case kMsgPinOffline:
            _HandlePinOffline(message);
            break;
            
        case kMsgUnpinOffline:
            _HandleUnpinOffline(message);
            break;
            
        case kMsgMakeOnlineOnly:
            _HandleMakeOnlineOnly(message);
            break;
            
        case kMsgShareLink:
            _HandleShareLink(message);
            break;
            
        case kMsgViewOnline:
            _HandleViewOnline(message);
            break;
            
        case kMsgShowSyncStatus:
            _ShowSyncStatus();
            break;
            
        case kMsgResolveConflict:
            _HandleResolveConflict(message);
            break;
            
        default:
            return B_ERROR;
    }
    
    return B_OK;
}

/**
 * @brief Handle sync now command
 */
void
OneDriveMessageHandler::_HandleSyncNow(BMessage* refs)
{
    BMessage daemonMsg(kMsgSyncRequired);
    daemonMsg.AddBool("force", true);
    
    // Add all refs to daemon message
    entry_ref ref;
    for (int32 i = 0; refs->FindRef("refs", i, &ref) == B_OK; i++) {
        daemonMsg.AddRef("refs", &ref);
    }
    
    _SendToDaemon(&daemonMsg);
}

/**
 * @brief Handle pin offline command
 */
void
OneDriveMessageHandler::_HandlePinOffline(BMessage* refs)
{
    entry_ref ref;
    for (int32 i = 0; refs->FindRef("refs", i, &ref) == B_OK; i++) {
        BNode node(&ref);
        if (node.InitCheck() == B_OK) {
            bool pinned = true;
            AttributeHelper::WriteBoolAttribute(node, kPinnedAttr, pinned);
            
            // Notify daemon
            BMessage daemonMsg(kMsgDownloadFile);
            daemonMsg.AddRef("ref", &ref);
            daemonMsg.AddBool("pin", true);
            _SendToDaemon(&daemonMsg);
        }
    }
}

/**
 * @brief Handle unpin offline command
 */
void
OneDriveMessageHandler::_HandleUnpinOffline(BMessage* refs)
{
    entry_ref ref;
    for (int32 i = 0; refs->FindRef("refs", i, &ref) == B_OK; i++) {
        BNode node(&ref);
        if (node.InitCheck() == B_OK) {
            bool pinned = false;
            AttributeHelper::WriteBoolAttribute(node, kPinnedAttr, pinned);
            
            // Notify daemon
            BMessage daemonMsg('unpn');
            daemonMsg.AddRef("ref", &ref);
            _SendToDaemon(&daemonMsg);
        }
    }
}

/**
 * @brief Handle make online-only command
 */
void
OneDriveMessageHandler::_HandleMakeOnlineOnly(BMessage* refs)
{
    entry_ref ref;
    for (int32 i = 0; refs->FindRef("refs", i, &ref) == B_OK; i++) {
        // Notify daemon to make file online-only
        BMessage daemonMsg('mkol');
        daemonMsg.AddRef("ref", &ref);
        _SendToDaemon(&daemonMsg);
    }
}

/**
 * @brief Handle share link command
 */
void
OneDriveMessageHandler::_HandleShareLink(BMessage* refs)
{
    // Get sharing link from daemon
    BMessage daemonMsg('shrl');
    entry_ref ref;
    if (refs->FindRef("refs", &ref) == B_OK) {
        daemonMsg.AddRef("ref", &ref);
        
        BMessage reply;
        BMessenger daemon(APP_SIGNATURE);
        if (daemon.SendMessage(&daemonMsg, &reply) == B_OK) {
            const char* link;
            if (reply.FindString("link", &link) == B_OK) {
                // Copy to clipboard
                BClipboard clipboard("system");
                if (clipboard.Lock()) {
                    clipboard.Clear();
                    BMessage* clip = clipboard.Data();
                    clip->AddData("text/plain", B_MIME_TYPE, link, strlen(link));
                    clipboard.Commit();
                    clipboard.Unlock();
                    
                    // Show notification
                    BAlert* alert = new BAlert("OneDrive",
                        B_TRANSLATE("OneDrive link copied to clipboard"),
                        B_TRANSLATE("OK"));
                    alert->Go();
                }
            }
        }
    }
}

/**
 * @brief Handle view online command
 */
void
OneDriveMessageHandler::_HandleViewOnline(BMessage* refs)
{
    // Get online URL from daemon
    BMessage daemonMsg('vwol');
    entry_ref ref;
    if (refs->FindRef("refs", &ref) == B_OK) {
        daemonMsg.AddRef("ref", &ref);
        
        BMessage reply;
        BMessenger daemon(APP_SIGNATURE);
        if (daemon.SendMessage(&daemonMsg, &reply) == B_OK) {
            const char* url;
            if (reply.FindString("url", &url) == B_OK) {
                // Open in browser
                be_roster->Launch("text/html", 1, (char**)&url);
            }
        }
    }
}

/**
 * @brief Show sync status window
 */
void
OneDriveMessageHandler::_ShowSyncStatus()
{
    // Launch preferences app with status tab
    be_roster->Launch(APP_SIGNATURE "_Preferences", (BMessage*)NULL, NULL);
}

/**
 * @brief Handle resolve conflict command
 */
void
OneDriveMessageHandler::_HandleResolveConflict(BMessage* refs)
{
    // TODO: Show conflict resolution dialog
    BAlert* alert = new BAlert("OneDrive",
        B_TRANSLATE("Conflict resolution dialog not yet implemented"),
        B_TRANSLATE("OK"));
    alert->Go();
}

/**
 * @brief Send message to daemon
 */
status_t
OneDriveMessageHandler::_SendToDaemon(BMessage* message)
{
    BMessenger daemon(APP_SIGNATURE);
    if (!daemon.IsValid()) {
        LOG_ERROR("OneDriveTrackerAddon", "OneDrive daemon not running");
        return B_ERROR;
    }
    
    return daemon.SendMessage(message);
}