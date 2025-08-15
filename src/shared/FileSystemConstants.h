/**
 * @file FileSystemConstants.h
 * @brief File system related constants for the Haiku OneDrive project
 * @author Haiku OneDrive Team
 * @version 1.0.0
 * @date 2025-08-15
 * 
 * This file consolidates all file system related constants including
 * permissions, buffer sizes, and file operation parameters.
 */

#ifndef FILESYSTEM_CONSTANTS_H
#define FILESYSTEM_CONSTANTS_H

#include <SupportDefs.h>
#include <StorageDefs.h>

namespace OneDrive {
namespace FileSystem {

/**
 * @brief Directory and file permissions
 */
constexpr mode_t kDefaultDirectoryMode = 0755;
constexpr mode_t kDefaultFileMode = 0644;
constexpr mode_t kPrivateDirectoryMode = 0700;

/**
 * @brief Buffer sizes for file operations
 */
constexpr size_t kSmallBufferSize = 256;
constexpr size_t kMediumBufferSize = 1024;
constexpr size_t kLargeBufferSize = 8192;
const size_t kPathBufferSize = B_PATH_NAME_LENGTH;

/**
 * @brief File size thresholds
 */
constexpr off_t kLargeFileThreshold = 4 * 1024 * 1024;  ///< 4MB
constexpr off_t kChunkSize = 320 * 1024;                ///< 320KB for upload chunks
constexpr off_t kMaxCachedFileSize = 100 * 1024 * 1024; ///< 100MB max for caching

/**
 * @brief Special file indicators
 */
constexpr char kHiddenFilePrefix = '.';
constexpr const char* kTempFileExtension = ".tmp";
constexpr const char* kConflictFileSuffix = "_conflict";

/**
 * @brief Sync-related timeouts (in microseconds)
 */
constexpr bigtime_t kFileWatchTimeout = 1000000;     ///< 1 second
constexpr bigtime_t kSyncRetryDelay = 5000000;       ///< 5 seconds
constexpr bigtime_t kIconUpdateDelay = 100000;       ///< 100ms

/**
 * @brief Icon overlay positioning
 */
constexpr float kOverlaySizeRatio = 0.5f;            ///< Overlay is half the icon size
constexpr float kOverlayPositionBottomRight = 3;      ///< Position index

/**
 * @brief Drawing constants for icon overlays
 */
namespace IconMetrics {
    constexpr float kPenSizeThin = 1.0f / 12.0f;     ///< Thin pen relative to icon size
    constexpr float kPenSizeMedium = 1.0f / 8.0f;    ///< Medium pen relative to icon size
    constexpr float kPenSizeThick = 1.0f / 6.0f;     ///< Thick pen relative to icon size
    
    constexpr float kCheckmarkStart = 0.25f;         ///< Checkmark start position ratio
    constexpr float kCheckmarkMid = 0.45f;           ///< Checkmark middle position ratio
    constexpr float kCheckmarkEnd = 0.75f;           ///< Checkmark end position ratio
    
    constexpr float kInsetSmall = 0.1f;              ///< Small inset ratio
    constexpr float kInsetMedium = 0.2f;             ///< Medium inset ratio
    constexpr float kInsetLarge = 0.25f;             ///< Large inset ratio
}

} // namespace FileSystem
} // namespace OneDrive

#endif // FILESYSTEM_CONSTANTS_H