/**
 * @file OneDriveConstants.h
 * @brief Global constants for the Haiku OneDrive project
 * @author Haiku OneDrive Team
 * @version 1.0.0
 * @date 2025-08-14
 * 
 * This file contains application-wide constants used throughout the
 * OneDrive client for Haiku OS. These constants define the application
 * identity, version information, and other global values.
 */

#ifndef ONEDRIVE_CONSTANTS_H
#define ONEDRIVE_CONSTANTS_H

/**
 * @brief Application signature for Haiku system registration
 * 
 * This signature follows the Haiku convention for application identification
 * and is used for:
 * - Application registration with the system
 * - Localization catalog identification  
 * - Settings file organization
 * - MIME type associations
 */
#define APP_SIGNATURE "application/x-vnd.HaikuOneDrive"

/**
 * @brief Human-readable application name
 * 
 * Short name used in user interfaces, window titles, and notifications.
 */
#define APP_NAME "OneDrive"

/**
 * @brief Current version of the OneDrive client
 * 
 * Version string following semantic versioning (major.minor.patch).
 * This version is displayed in about dialogs, help output, and logs.
 */
#define APP_VERSION "1.0.0"

/**
 * @brief OneDrive client version (alternative naming for compatibility)
 * 
 * Same as APP_VERSION, provided for components that expect ONEDRIVE_VERSION.
 */
#define ONEDRIVE_VERSION APP_VERSION

/**
 * @brief Build date string
 * 
 * Compilation date for version identification and debugging.
 */
#define ONEDRIVE_BUILD_DATE __DATE__

/**
 * @brief Additional message constants for filesystem integration
 * 
 * These constants are used for communication between VirtualFolder
 * and the daemon for filesystem operations.
 */
enum {
    kMsgDownloadFile = 'dlfl',      ///< Request to download a file
    kMsgSyncRequired = 'syrq',      ///< Sync is required for folder changes
    kMsgUpdateIcon = 'upic',        ///< Update file/folder icon
    kMsgConflictDetected = 'cfdt',  ///< Conflict detected during sync
    kMsgFolderAdded = 'flad',       ///< New sync folder added
    kMsgFolderRemoved = 'flrm'      ///< Sync folder removed
};

#endif // ONEDRIVE_CONSTANTS_H