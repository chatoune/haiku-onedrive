/**
 * @file ColorConstants.h
 * @brief Color constants for UI elements in the Haiku OneDrive project
 * @author Haiku OneDrive Team
 * @version 1.0.0
 * @date 2025-08-15
 * 
 * This file defines all color constants used throughout the application
 * for consistent theming and visual feedback.
 */

#ifndef COLOR_CONSTANTS_H
#define COLOR_CONSTANTS_H

#include <GraphicsDefs.h>

namespace OneDrive {
namespace Colors {

/**
 * @brief Sync state indicator colors
 */
namespace SyncState {
    constexpr rgb_color kSynced = {0, 200, 0, 255};          ///< Green for synced
    constexpr rgb_color kSyncing = {0, 100, 200, 255};       ///< Blue for syncing
    constexpr rgb_color kError = {200, 0, 0, 255};           ///< Red for errors
    constexpr rgb_color kPending = {200, 200, 0, 255};       ///< Yellow for pending
    constexpr rgb_color kConflict = {255, 140, 0, 255};      ///< Orange for conflicts
    constexpr rgb_color kOffline = {128, 128, 128, 255};     ///< Gray for offline
    constexpr rgb_color kOnlineOnly = {100, 150, 200, 255};  ///< Light blue for cloud
}

/**
 * @brief UI element colors
 */
namespace UI {
    constexpr rgb_color kBackground = {255, 255, 255, 255};  ///< White background
    const rgb_color kTransparent = B_TRANSPARENT_COLOR;       ///< Transparent
    constexpr rgb_color kHighlight = {0, 120, 215, 255};     ///< Highlight blue
    constexpr rgb_color kDisabled = {180, 180, 180, 255};    ///< Disabled gray
}

/**
 * @brief Text colors
 */
namespace Text {
    constexpr rgb_color kNormal = {0, 0, 0, 255};            ///< Black text
    constexpr rgb_color kSuccess = {0, 128, 0, 255};         ///< Green success
    constexpr rgb_color kError = {200, 0, 0, 255};           ///< Red error
    constexpr rgb_color kWarning = {200, 100, 0, 255};       ///< Orange warning
    constexpr rgb_color kInfo = {0, 100, 200, 255};          ///< Blue info
}

} // namespace Colors
} // namespace OneDrive

#endif // COLOR_CONSTANTS_H