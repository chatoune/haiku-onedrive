/**
 * @file SyncStateIcons.h
 * @brief Icon management for OneDrive sync states
 * @author Haiku OneDrive Team
 * @version 1.0.0
 * @date 2025-08-15
 * 
 * This class manages the creation and caching of icons for different
 * synchronization states, providing visual feedback in Tracker.
 */

#ifndef SYNC_STATE_ICONS_H
#define SYNC_STATE_ICONS_H

#include <Bitmap.h>
#include <List.h>
#include <Locker.h>
#include <String.h>
#include <Mime.h>

#include "VirtualFolder.h"

class BBitmap;

/**
 * @brief Icon overlay types for sync states
 * 
 * Different overlay badges that can be applied to file icons
 * to indicate their synchronization status.
 */
enum IconOverlayType {
    kOverlayNone = 0,       ///< No overlay
    kOverlaySynced,         ///< Green checkmark
    kOverlaySyncing,        ///< Blue sync arrows
    kOverlayError,          ///< Red X
    kOverlayPending,        ///< Yellow clock
    kOverlayConflict,       ///< Orange warning
    kOverlayOffline,        ///< Gray pin
    kOverlayOnlineOnly      ///< Cloud icon
};

/**
 * @brief Manages icons for OneDrive sync states
 * 
 * This class creates and caches icon overlays for different sync states,
 * applying them to file and folder icons to provide visual feedback
 * about synchronization status in Tracker.
 * 
 * @see VirtualFolder
 * @since 1.0.0
 */
class SyncStateIcons {
public:
    /**
     * @brief Get the singleton instance
     * 
     * @return Reference to the singleton instance
     */
    static SyncStateIcons& Instance();
    
    /**
     * @brief Initialize the icon system
     * 
     * Loads or creates all sync state icons.
     * 
     * @return B_OK on success, error code otherwise
     */
    status_t Initialize();
    
    /**
     * @brief Get icon overlay for a sync state
     * 
     * @param state The sync state
     * @return Overlay type for the state
     */
    IconOverlayType GetOverlayForState(OneDriveSyncState state) const;
    
    /**
     * @brief Apply overlay to an icon
     * 
     * Creates a new bitmap with the overlay applied to the base icon.
     * 
     * @param baseIcon The original icon
     * @param overlay The overlay type to apply
     * @param result Output bitmap with overlay applied
     * @return B_OK on success, error code otherwise
     */
    status_t ApplyOverlay(const BBitmap* baseIcon, IconOverlayType overlay,
                          BBitmap** result);
    
    /**
     * @brief Update file/folder icon with sync state
     * 
     * Updates the icon attribute of a file or folder to reflect
     * its current sync state.
     * 
     * @param ref Entry reference
     * @param state Current sync state
     * @return B_OK on success, error code otherwise
     */
    status_t UpdateFileIcon(const entry_ref& ref, OneDriveSyncState state);
    
    /**
     * @brief Create vector icon data for overlay
     * 
     * Generates HVIF data for a specific overlay type.
     * 
     * @param overlay The overlay type
     * @param data Output buffer for HVIF data
     * @param size Size of the output buffer
     * @return Actual size of HVIF data, or negative error
     */
    ssize_t CreateOverlayData(IconOverlayType overlay, uint8* data, 
                              size_t size);
    
    /**
     * @brief Get cached overlay bitmap
     * 
     * @param overlay The overlay type
     * @param size Icon size (B_MINI_ICON or B_LARGE_ICON)
     * @return Cached bitmap or NULL if not cached
     */
    BBitmap* GetCachedOverlay(IconOverlayType overlay, icon_size size);
    
    /**
     * @brief Clear icon cache
     * 
     * Frees all cached overlay bitmaps.
     */
    void ClearCache();

private:
    /**
     * @brief Private constructor (singleton)
     */
    SyncStateIcons();
    
    /**
     * @brief Destructor
     */
    ~SyncStateIcons();
    
    /**
     * @brief Create overlay bitmap
     * 
     * @param overlay The overlay type
     * @param size Icon size
     * @return Created bitmap or NULL on failure
     */
    BBitmap* _CreateOverlayBitmap(IconOverlayType overlay, icon_size size);
    
    /**
     * @brief Draw checkmark overlay
     * 
     * @param bitmap Target bitmap
     * @param color Checkmark color
     */
    void _DrawCheckmark(BBitmap* bitmap, rgb_color color);
    
    /**
     * @brief Draw sync arrows overlay
     * 
     * @param bitmap Target bitmap
     * @param color Arrow color
     */
    void _DrawSyncArrows(BBitmap* bitmap, rgb_color color);
    
    /**
     * @brief Draw error X overlay
     * 
     * @param bitmap Target bitmap
     * @param color X color
     */
    void _DrawErrorX(BBitmap* bitmap, rgb_color color);
    
    /**
     * @brief Draw clock overlay
     * 
     * @param bitmap Target bitmap
     * @param color Clock color
     */
    void _DrawClock(BBitmap* bitmap, rgb_color color);
    
    /**
     * @brief Draw warning triangle overlay
     * 
     * @param bitmap Target bitmap
     * @param color Triangle color
     */
    void _DrawWarning(BBitmap* bitmap, rgb_color color);
    
    /**
     * @brief Draw pin overlay
     * 
     * @param bitmap Target bitmap
     * @param color Pin color
     */
    void _DrawPin(BBitmap* bitmap, rgb_color color);
    
    /**
     * @brief Draw cloud overlay
     * 
     * @param bitmap Target bitmap
     * @param color Cloud color
     */
    void _DrawCloud(BBitmap* bitmap, rgb_color color);
    
    /**
     * @brief Composite overlay onto base icon
     * 
     * @param base Base icon bitmap
     * @param overlay Overlay bitmap
     * @param position Position for overlay (0-3 for corners)
     * @return Composited bitmap
     */
    BBitmap* _CompositeIcons(const BBitmap* base, const BBitmap* overlay,
                             int32 position = 3);

private:
    static SyncStateIcons* sInstance;   ///< Singleton instance
    BList fOverlayCache;                ///< Cached overlay bitmaps
    mutable BLocker fCacheLock;         ///< Lock for thread-safe access
    bool fInitialized;                  ///< Initialization flag
    
    // Prevent copying
    SyncStateIcons(const SyncStateIcons&);
    SyncStateIcons& operator=(const SyncStateIcons&);
};

#endif // SYNC_STATE_ICONS_H