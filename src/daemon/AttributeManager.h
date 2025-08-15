/**
 * @file AttributeManager.h
 * @brief BFS Extended Attribute Manager for OneDrive synchronization
 * @author Haiku OneDrive Team
 * @version 1.0.0
 * @date 2025-08-14
 * 
 * This file contains the AttributeManager class which handles Haiku BFS extended
 * attribute synchronization with OneDrive. It provides reading, writing, monitoring,
 * and serialization of file attributes for seamless cloud integration.
 */

#ifndef ATTRIBUTE_MANAGER_H
#define ATTRIBUTE_MANAGER_H

#include <String.h>
#include <Message.h>
#include <Locker.h>
#include <List.h>
#include <Path.h>
#include <Node.h>
#include <NodeMonitor.h>
#include <Entry.h>
#include <fs_attr.h>
#include <queue>

// Forward declarations
class OneDriveAPI;

/**
 * @brief BFS attribute change operation types
 */
enum AttributeChangeType {
    ATTR_CREATED = 0,     ///< New attribute was created
    ATTR_MODIFIED,        ///< Existing attribute was modified
    ATTR_REMOVED,         ///< Attribute was removed
    ATTR_RENAMED          ///< Attribute was renamed
};

/**
 * @brief BFS attribute data types supported by Haiku
 */
enum BFSAttributeType {
    ATTR_TYPE_STRING = B_STRING_TYPE,      ///< String attribute
    ATTR_TYPE_INT32 = B_INT32_TYPE,        ///< 32-bit integer
    ATTR_TYPE_INT64 = B_INT64_TYPE,        ///< 64-bit integer
    ATTR_TYPE_FLOAT = B_FLOAT_TYPE,        ///< Float value
    ATTR_TYPE_DOUBLE = B_DOUBLE_TYPE,      ///< Double precision float
    ATTR_TYPE_BOOL = B_BOOL_TYPE,          ///< Boolean value
    ATTR_TYPE_RAW = B_RAW_TYPE,            ///< Raw binary data
    ATTR_TYPE_TIME = B_TIME_TYPE,          ///< Time value
    ATTR_TYPE_RECT = B_RECT_TYPE,          ///< Rectangle
    ATTR_TYPE_POINT = B_POINT_TYPE,        ///< Point coordinates
    ATTR_TYPE_MESSAGE = B_MESSAGE_TYPE     ///< Archived BMessage
};

/**
 * @brief Single BFS attribute information
 */
struct BFSAttribute {
    BString name;                   ///< Attribute name
    BFSAttributeType type;          ///< Attribute data type
    size_t size;                    ///< Data size in bytes
    void* data;                     ///< Attribute data (caller owns)
    time_t modificationTime;        ///< When attribute was last modified
    
    BFSAttribute();
    BFSAttribute(const BFSAttribute& other);
    BFSAttribute& operator=(const BFSAttribute& other);
    ~BFSAttribute();
    
    /**
     * @brief Deep copy attribute data
     * @param source Source attribute to copy from
     */
    void CopyFrom(const BFSAttribute& source);
    
    /**
     * @brief Free allocated data memory
     */
    void Clear();
};

/**
 * @brief Attribute change notification for queue processing
 */
struct AttributeChange {
    BString filePath;               ///< Path to file that changed
    BString attributeName;          ///< Name of changed attribute
    AttributeChangeType changeType; ///< Type of change that occurred
    time_t timestamp;               ///< When change occurred
    BFSAttribute oldValue;          ///< Previous value (for modifications)
    BFSAttribute newValue;          ///< New value (for creations/modifications)
    
    AttributeChange();
    ~AttributeChange();
};

/**
 * @brief Attribute conflict types for resolution
 */
enum AttributeConflictType {
    CONFLICT_VALUE_MISMATCH = 0,    ///< Same attribute, different values
    CONFLICT_TYPE_MISMATCH,         ///< Same name, different types
    CONFLICT_LOCAL_ONLY,            ///< Attribute exists only locally
    CONFLICT_REMOTE_ONLY,           ///< Attribute exists only remotely
    CONFLICT_TIMESTAMP              ///< Conflicting timestamps
};

/**
 * @brief Attribute conflict information for resolution
 */
struct AttributeConflict {
    BString attributeName;          ///< Name of conflicting attribute
    AttributeConflictType type;     ///< Type of conflict
    BFSAttribute localValue;        ///< Local attribute value
    BFSAttribute remoteValue;       ///< Remote attribute value
    time_t localTimestamp;          ///< When local attribute was modified
    time_t remoteTimestamp;         ///< When remote attribute was modified
    BString conflictDescription;    ///< Human-readable description
    
    AttributeConflict();
    ~AttributeConflict();
};

/**
 * @brief BFS Extended Attribute Manager for OneDrive synchronization
 * 
 * The AttributeManager class handles Haiku BFS extended attribute synchronization
 * with OneDrive. It provides comprehensive attribute management including:
 * 
 * - Reading and writing BFS extended attributes
 * - Real-time monitoring of attribute changes using node watching
 * - Queuing attribute changes for batch processing
 * - Serialization/deserialization for OneDrive metadata storage
 * - Conflict detection and resolution for attribute synchronization
 * - Integration with OneDrive API for metadata upload/download
 * 
 * The manager maintains a queue of attribute changes and processes them
 * asynchronously to avoid blocking file operations. It supports all standard
 * BFS attribute types and provides transparent handling of binary data.
 * 
 * @see BNode
 * @see OneDriveAPI
 * @see BNodeMonitor
 * @since 1.0.0
 */
class AttributeManager {
public:
    /**
     * @brief Constructor
     * 
     * Initializes the attribute manager with basic state. Component initialization
     * is deferred until Initialize() is called to allow for proper error handling.
     */
    AttributeManager();
    
    /**
     * @brief Destructor
     * 
     * Stops monitoring, cleans up resources, and ensures graceful shutdown.
     */
    ~AttributeManager();
    
    // Lifecycle Management
    
    /**
     * @brief Initialize the attribute manager
     * 
     * Sets up node monitoring, creates the attribute change processing thread,
     * and prepares the manager for operation.
     * 
     * @param apiClient Pointer to OneDriveAPI instance for metadata operations
     * @return B_OK on success, error code on failure
     * @retval B_OK Successfully initialized
     * @retval B_NO_MEMORY Insufficient memory for initialization
     * @retval B_NOT_ALLOWED Node monitoring not available
     */
    status_t Initialize(OneDriveAPI* apiClient);
    
    /**
     * @brief Shutdown the attribute manager
     * 
     * Stops all monitoring and processing threads, saves pending changes,
     * and prepares for destruction.
     * 
     * @return B_OK on success, error code on failure
     */
    status_t Shutdown();
    
    // Attribute Operations
    
    /**
     * @brief Read all attributes from a file
     * 
     * Retrieves all extended attributes from the specified file and returns
     * them in a BMessage for easy serialization and transport.
     * 
     * @param filePath Path to the file to read attributes from
     * @param attributes Output BMessage to store attributes
     * @return B_OK on success, error code on failure
     * @retval B_OK Attributes read successfully
     * @retval B_ENTRY_NOT_FOUND File does not exist
     * @retval B_NOT_ALLOWED Permission denied
     * @retval B_NO_MEMORY Insufficient memory
     */
    status_t ReadAttributes(const BString& filePath, BMessage& attributes);
    
    /**
     * @brief Write attributes to a file
     * 
     * Sets extended attributes on the specified file from a BMessage.
     * Existing attributes not present in the message are preserved.
     * 
     * @param filePath Path to the file to write attributes to
     * @param attributes BMessage containing attributes to write
     * @return B_OK on success, error code on failure
     * @retval B_OK Attributes written successfully
     * @retval B_ENTRY_NOT_FOUND File does not exist
     * @retval B_NOT_ALLOWED Permission denied
     * @retval B_NO_MEMORY Insufficient memory
     */
    status_t WriteAttributes(const BString& filePath, const BMessage& attributes);
    
    /**
     * @brief Read a specific attribute from a file
     * 
     * Retrieves a single named attribute from the specified file.
     * 
     * @param filePath Path to the file to read from
     * @param attributeName Name of the attribute to read
     * @param attribute Output structure to store attribute data
     * @return B_OK on success, error code on failure
     * @retval B_OK Attribute read successfully
     * @retval B_ENTRY_NOT_FOUND File or attribute does not exist
     * @retval B_NOT_ALLOWED Permission denied
     */
    status_t ReadAttribute(const BString& filePath, const BString& attributeName, BFSAttribute& attribute);
    
    /**
     * @brief Write a specific attribute to a file
     * 
     * Sets a single named attribute on the specified file.
     * 
     * @param filePath Path to the file to write to
     * @param attribute Attribute data to write
     * @return B_OK on success, error code on failure
     * @retval B_OK Attribute written successfully
     * @retval B_ENTRY_NOT_FOUND File does not exist
     * @retval B_NOT_ALLOWED Permission denied
     */
    status_t WriteAttribute(const BString& filePath, const BFSAttribute& attribute);
    
    /**
     * @brief Remove an attribute from a file
     * 
     * Deletes the specified attribute from the file.
     * 
     * @param filePath Path to the file to modify
     * @param attributeName Name of the attribute to remove
     * @return B_OK on success, error code on failure
     * @retval B_OK Attribute removed successfully
     * @retval B_ENTRY_NOT_FOUND File or attribute does not exist
     * @retval B_NOT_ALLOWED Permission denied
     */
    status_t RemoveAttribute(const BString& filePath, const BString& attributeName);
    
    // Monitoring and Change Detection
    
    /**
     * @brief Start monitoring a directory for attribute changes
     * 
     * Begins node monitoring for the specified directory and all subdirectories.
     * Attribute changes will be queued for processing.
     * 
     * @param directoryPath Path to directory to monitor
     * @param recursive Whether to monitor subdirectories recursively
     * @return B_OK on success, error code on failure
     * @retval B_OK Monitoring started successfully
     * @retval B_ENTRY_NOT_FOUND Directory does not exist
     * @retval B_NOT_ALLOWED Permission denied or monitoring unavailable
     */
    status_t StartMonitoring(const BString& directoryPath, bool recursive = true);
    
    /**
     * @brief Stop monitoring a directory
     * 
     * Stops node monitoring for the specified directory.
     * 
     * @param directoryPath Path to directory to stop monitoring
     * @return B_OK on success, error code on failure
     */
    status_t StopMonitoring(const BString& directoryPath);
    
    /**
     * @brief Stop all monitoring
     * 
     * Stops monitoring all directories and clears the monitoring list.
     * 
     * @return B_OK on success, error code on failure
     */
    status_t StopAllMonitoring();
    
    // Change Queue Management
    
    /**
     * @brief Get pending attribute changes
     * 
     * Retrieves all queued attribute changes for processing. Changes are
     * removed from the queue after retrieval.
     * 
     * @param changes Output list to store AttributeChange objects
     * @return Number of changes retrieved
     */
    int32 GetPendingChanges(BList& changes);
    
    /**
     * @brief Check if there are pending attribute changes
     * 
     * @return true if changes are queued, false otherwise
     */
    bool HasPendingChanges() const;
    
    /**
     * @brief Clear all pending changes
     * 
     * Removes all queued attribute changes without processing them.
     * Use with caution as this may cause data loss.
     */
    void ClearPendingChanges();
    
    // Serialization and OneDrive Integration
    
    /**
     * @brief Serialize attributes to JSON format
     * 
     * Converts a BMessage containing attributes to JSON format suitable
     * for storage in OneDrive metadata.
     * 
     * @param attributes BMessage containing attributes to serialize
     * @param jsonOutput Output string to store JSON data
     * @return B_OK on success, error code on failure
     * @retval B_OK Serialization successful
     * @retval B_NO_MEMORY Insufficient memory for JSON generation
     * @retval B_BAD_VALUE Invalid attribute data
     */
    status_t SerializeToJSON(const BMessage& attributes, BString& jsonOutput);
    
    /**
     * @brief Deserialize attributes from JSON format
     * 
     * Converts JSON attribute data from OneDrive metadata back to a BMessage.
     * 
     * @param jsonInput JSON string containing attribute data
     * @param attributes Output BMessage to store parsed attributes
     * @return B_OK on success, error code on failure
     * @retval B_OK Deserialization successful
     * @retval B_BAD_VALUE Invalid JSON format
     * @retval B_NO_MEMORY Insufficient memory for parsing
     */
    status_t DeserializeFromJSON(const BString& jsonInput, BMessage& attributes);
    
    /**
     * @brief Synchronize file attributes with OneDrive
     * 
     * Uploads local file attributes to OneDrive metadata storage and
     * downloads any remote changes.
     * 
     * @param filePath Path to file to synchronize
     * @param forceUpload Force upload even if no local changes detected
     * @return B_OK on success, error code on failure
     * @retval B_OK Synchronization successful
     * @retval B_NETWORK_ERROR Network communication failed
     * @retval B_PERMISSION_DENIED OneDrive access denied
     */
    status_t SynchronizeAttributes(const BString& filePath, bool forceUpload = false);
    
    // Conflict Resolution
    
    /**
     * @brief Detect attribute conflicts between local and remote
     * 
     * Compares local file attributes with OneDrive metadata to identify
     * conflicts that require resolution.
     * 
     * @param filePath Path to file to check for conflicts
     * @param hasConflicts Output parameter indicating if conflicts exist
     * @param conflictDetails Output BMessage with conflict information
     * @return B_OK on success, error code on failure
     */
    status_t DetectConflicts(const BString& filePath, bool& hasConflicts, BMessage& conflictDetails);
    
    /**
     * @brief Resolve attribute conflicts using specified strategy
     * 
     * Resolves conflicts between local and remote attributes using the
     * specified resolution strategy.
     * 
     * @param filePath Path to file with conflicts
     * @param conflictDetails BMessage containing conflict information
     * @param strategy Resolution strategy ("local_wins", "remote_wins", "merge")
     * @return B_OK on success, error code on failure
     */
    status_t ResolveConflicts(const BString& filePath, const BMessage& conflictDetails, const BString& strategy);

private:
    /// @name Core Components
    /// @{
    OneDriveAPI*            fAPIClient;         ///< OneDrive API client for metadata operations
    BLocker                 fLock;              ///< Thread synchronization lock
    /// @}
    
    /// @name Monitoring State
    /// @{
    BList                   fMonitoredPaths;    ///< List of monitored directory paths
    std::queue<AttributeChange*> fChangeQueue;  ///< Queue of pending attribute changes
    bool                    fMonitoringActive;  ///< Whether monitoring is currently active
    thread_id               fProcessorThread;   ///< Change processing thread ID
    sem_id                  fProcessorSemaphore; ///< Semaphore for thread synchronization
    bool                    fShuttingDown;      ///< Shutdown flag for threads
    /// @}
    
    /// @name Internal Methods
    /// @{
    
    /**
     * @brief Node monitor message handler
     * 
     * Processes node monitor messages and queues attribute changes.
     * 
     * @param message Node monitor message
     */
    static status_t _NodeMonitorHandler(BMessage* message);
    
    /**
     * @brief Change processing thread entry point
     * 
     * Processes queued attribute changes in background thread.
     * 
     * @param data Pointer to AttributeManager instance
     * @return Thread exit code
     */
    static int32 _ChangeProcessorThread(void* data);
    
    /**
     * @brief Process a single attribute change
     * 
     * Handles a single queued attribute change, including OneDrive synchronization.
     * 
     * @param change Pointer to AttributeChange to process
     */
    void _ProcessAttributeChange(AttributeChange* change);
    
    /**
     * @brief Convert BFS attribute type to string
     * 
     * @param type BFS attribute type
     * @return String representation of type
     */
    BString _AttributeTypeToString(BFSAttributeType type);
    
    /**
     * @brief Convert string to BFS attribute type
     * 
     * @param typeString String representation of type
     * @return BFS attribute type
     */
    BFSAttributeType _StringToAttributeType(const BString& typeString);
    
    /**
     * @brief Add a monitored path to the internal list
     * 
     * @param path Path to add to monitoring list
     * @param recursive Whether monitoring is recursive
     */
    void _AddMonitoredPath(const BString& path, bool recursive);
    
    /**
     * @brief Remove a monitored path from the internal list
     * 
     * @param path Path to remove from monitoring list
     */
    void _RemoveMonitoredPath(const BString& path);
    
    /**
     * @brief Queue an attribute change for processing
     * 
     * @param change Attribute change to queue (manager takes ownership)
     */
    void _QueueAttributeChange(AttributeChange* change);
    
    /**
     * @brief Get remote attributes from OneDrive metadata
     * 
     * @param filePath Local file path
     * @param remoteAttributes BMessage to store remote attributes
     * @return B_OK on success, error code on failure
     */
    status_t _GetRemoteAttributes(const BString& filePath, BMessage& remoteAttributes);
    
    /**
     * @brief Compare local and remote attributes for conflicts
     * 
     * @param localAttributes Local BFS attributes
     * @param remoteAttributes Remote attributes from OneDrive
     * @param conflicts List to store detected conflicts
     * @return B_OK on success, error code on failure
     */
    status_t _CompareAttributes(const BMessage& localAttributes, const BMessage& remoteAttributes, BList& conflicts);
    
    /**
     * @brief Convert conflict type to string representation
     * 
     * @param type Conflict type enum
     * @return String representation of conflict type
     */
    BString _ConflictTypeToString(AttributeConflictType type);
    
    /**
     * @brief Add attribute value to BMessage for conflict reporting
     * 
     * @param attribute BFS attribute to add
     * @param message Message to add value to
     * @param fieldName Field name for the value
     */
    void _AddAttributeValueToMessage(const BFSAttribute& attribute, BMessage& message, const char* fieldName);
    
    /**
     * @brief Clear conflict list and free memory
     * 
     * @param conflicts List of AttributeConflict objects to clear
     */
    void _ClearConflictList(BList& conflicts);
    
    /**
     * @brief Resolve conflicts using local wins strategy
     * 
     * @param localAttributes Local attributes
     * @param remoteAttributes Remote attributes
     * @param conflictDetails Conflict information
     * @param resolvedAttributes Output resolved attributes
     * @return B_OK on success, error code on failure
     */
    status_t _ResolveLocalWins(const BMessage& localAttributes, const BMessage& remoteAttributes,
                              const BMessage& conflictDetails, BMessage& resolvedAttributes);
    
    /**
     * @brief Resolve conflicts using remote wins strategy
     * 
     * @param localAttributes Local attributes
     * @param remoteAttributes Remote attributes
     * @param conflictDetails Conflict information
     * @param resolvedAttributes Output resolved attributes
     * @return B_OK on success, error code on failure
     */
    status_t _ResolveRemoteWins(const BMessage& localAttributes, const BMessage& remoteAttributes,
                               const BMessage& conflictDetails, BMessage& resolvedAttributes);
    
    /**
     * @brief Resolve conflicts using intelligent merge strategy
     * 
     * @param localAttributes Local attributes
     * @param remoteAttributes Remote attributes
     * @param conflictDetails Conflict information
     * @param resolvedAttributes Output resolved attributes
     * @return B_OK on success, error code on failure
     */
    status_t _ResolveMerge(const BMessage& localAttributes, const BMessage& remoteAttributes,
                          const BMessage& conflictDetails, BMessage& resolvedAttributes);
    
    /**
     * @brief Resolve conflicts using timestamp-based strategy
     * 
     * @param localAttributes Local attributes
     * @param remoteAttributes Remote attributes
     * @param conflictDetails Conflict information
     * @param resolvedAttributes Output resolved attributes
     * @return B_OK on success, error code on failure
     */
    status_t _ResolveTimestampWins(const BMessage& localAttributes, const BMessage& remoteAttributes,
                                  const BMessage& conflictDetails, BMessage& resolvedAttributes);
    
    /**
     * @brief Convert type code to string representation
     * 
     * @param type Haiku type code
     * @return String representation of type
     */
    BString _TypeCodeToString(type_code type);
    
    /**
     * @brief Convert string to type code
     * 
     * @param typeString String representation of type
     * @return Haiku type code
     */
    type_code _StringToTypeCode(const BString& typeString);
    
    /**
     * @brief Serialize attribute data to JSON string
     * 
     * @param jsonOutput BString to append JSON data to
     * @param type Attribute type code
     * @param data Pointer to attribute data
     * @param size Size of data in bytes
     */
    void _SerializeAttributeDataToJSON(BString& jsonOutput, type_code type, 
                                      const void* data, size_t size);
    
    /**
     * @brief Deserialize attribute data from JSON message
     * 
     * @param attrMessage BMessage containing attribute data
     * @param name Attribute name
     * @param type Attribute type
     * @param size Attribute size
     * @param attributes Output BMessage to store attribute
     */
    void _DeserializeAttributeDataFromJSON(const BMessage& attrMessage, 
                                          const BString& name, type_code type, 
                                          int64 size, BMessage& attributes);
    
    /**
     * @brief Simple Base64 encoding
     * 
     * @param data Binary data to encode
     * @param size Size of data
     * @param base64Output Encoded Base64 string
     */
    void _EncodeBase64Simple(const void* data, size_t size, BString& base64Output);
    
    /**
     * @brief Simple Base64 decoding
     * 
     * @param base64Input Base64 encoded string
     * @param data Output decoded data (caller owns)
     * @param size Output size of decoded data
     * @return B_OK on success, error code on failure
     */
    status_t _DecodeBase64Simple(const BString& base64Input, void** data, size_t* size);
    
    /// @}
};

#endif // ATTRIBUTE_MANAGER_H