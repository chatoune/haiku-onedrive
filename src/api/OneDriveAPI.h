/**
 * @file OneDriveAPI.h
 * @brief Microsoft Graph API client for OneDrive integration
 * @author Haiku OneDrive Team
 * @version 1.0.0
 * @date 2025-08-14
 * 
 * This file contains the OneDriveAPI class which provides HTTP client functionality
 * for communicating with Microsoft Graph API endpoints. It handles file operations,
 * metadata retrieval, and seamless integration with the AuthenticationManager.
 */

#ifndef ONEDRIVE_API_H
#define ONEDRIVE_API_H

#include <String.h>
#include <Message.h>
#include <Url.h>
#include <Locker.h>
#include <List.h>
#include <DataIO.h>

#include <memory>

// Forward declarations
class AuthenticationManager;
namespace OneDrive {
    class ConnectionPool;
}

/**
 * @brief Microsoft Graph API endpoints
 */
struct GraphEndpoints {
    static const char* kBaseURL;           ///< Graph API base URL
    static const char* kDriveRoot;         ///< Drive root endpoint
    static const char* kDriveItems;        ///< Drive items endpoint
    static const char* kUploadSession;     ///< Upload session endpoint
    static const char* kUserProfile;      ///< User profile endpoint
};

/**
 * @brief HTTP request methods
 */
enum HttpMethod {
    HTTP_GET = 0,
    HTTP_POST,
    HTTP_PUT,
    HTTP_DELETE,
    HTTP_PATCH
};

/**
 * @brief OneDrive item types
 */
enum OneDriveItemType {
    ITEM_TYPE_FILE = 0,
    ITEM_TYPE_FOLDER,
    ITEM_TYPE_UNKNOWN
};

/**
 * @brief OneDrive API error codes
 */
enum OneDriveError {
    ONEDRIVE_OK = 0,
    ONEDRIVE_AUTH_ERROR,
    ONEDRIVE_NETWORK_ERROR,
    ONEDRIVE_API_ERROR,
    ONEDRIVE_QUOTA_EXCEEDED,
    ONEDRIVE_FILE_NOT_FOUND,
    ONEDRIVE_INVALID_REQUEST,
    ONEDRIVE_RATE_LIMITED,
    ONEDRIVE_SERVER_ERROR
};

/**
 * @brief OneDrive file/folder item information
 */
struct OneDriveItem {
    BString id;                    ///< Unique OneDrive item ID
    BString name;                  ///< Item name
    BString path;                  ///< Full path from root
    OneDriveItemType type;         ///< File or folder
    off_t size;                    ///< File size in bytes (0 for folders)
    time_t createdTime;            ///< Creation timestamp
    time_t modifiedTime;           ///< Last modification timestamp
    BString eTag;                  ///< Entity tag for change detection
    BString downloadUrl;           ///< Direct download URL (temporary)
    BMessage attributes;           ///< Custom BFS attributes (Haiku-specific)
    
    OneDriveItem();
};

/**
 * @brief Microsoft Graph API client for OneDrive integration
 * 
 * The OneDriveAPI class provides a comprehensive HTTP client for communicating
 * with Microsoft Graph API endpoints. It handles authentication integration,
 * file operations, metadata management, and error handling.
 * 
 * Key features:
 * - Microsoft Graph API communication with proper authentication
 * - File upload/download with progress callbacks
 * - Folder listing and navigation
 * - Metadata synchronization including BFS attributes
 * - Error handling with retry logic
 * - Rate limiting compliance
 * - Thread-safe operation
 * 
 * Integration:
 * - Works seamlessly with AuthenticationManager for OAuth2 tokens
 * - Supports Haiku's BFS extended attributes via custom metadata
 * - Uses native Haiku networking APIs (BHttpSession, BUrlRequest)
 * - Follows Haiku coding conventions and error handling patterns
 * 
 * @see AuthenticationManager
 * @see BHttpSession
 * @since 1.0.0
 */
class OneDriveAPI {
public:
    /**
     * @brief Constructor with authentication manager
     * 
     * @param authManager Reference to authentication manager for token access
     */
    OneDriveAPI(AuthenticationManager& authManager);
    
    /**
     * @brief Destructor
     */
    virtual ~OneDriveAPI();
    
    // File Operations
    
    /**
     * @brief List items in OneDrive folder
     * 
     * Retrieves the contents of a OneDrive folder, including files and subfolders.
     * 
     * @param folderPath Path to folder (empty string for root)
     * @param items List to store retrieved items
     * @return OneDriveError code
     * @retval ONEDRIVE_OK Success
     * @retval ONEDRIVE_AUTH_ERROR Authentication failed
     * @retval ONEDRIVE_NETWORK_ERROR Network communication error
     * @retval ONEDRIVE_FILE_NOT_FOUND Folder not found
     */
    OneDriveError ListFolder(const BString& folderPath, BList& items);
    
    /**
     * @brief Download file from OneDrive
     * 
     * Downloads a file from OneDrive to local storage with optional progress callback.
     * 
     * @param itemId OneDrive item ID
     * @param localPath Local file path to save to
     * @param progressCallback Optional progress callback function
     * @param userData User data for progress callback
     * @return OneDriveError code
     */
    OneDriveError DownloadFile(const BString& itemId, 
                              const BString& localPath,
                              void (*progressCallback)(float progress, void* userData) = NULL,
                              void* userData = NULL);
    
    /**
     * @brief Upload file to OneDrive
     * 
     * Uploads a local file to OneDrive with optional progress callback.
     * Automatically handles large file uploads using upload sessions.
     * 
     * @param localPath Local file path to upload
     * @param remotePath Remote path in OneDrive (including filename)
     * @param progressCallback Optional progress callback function
     * @param userData User data for progress callback
     * @return OneDriveError code
     */
    OneDriveError UploadFile(const BString& localPath,
                            const BString& remotePath,
                            void (*progressCallback)(float progress, void* userData) = NULL,
                            void* userData = NULL);
    
    /**
     * @brief Create folder in OneDrive
     * 
     * @param folderPath Path where to create the folder
     * @param folderName Name of the new folder
     * @return OneDriveError code
     */
    OneDriveError CreateFolder(const BString& folderPath, const BString& folderName);
    
    /**
     * @brief Delete item from OneDrive
     * 
     * @param itemId OneDrive item ID to delete
     * @return OneDriveError code
     */
    OneDriveError DeleteItem(const BString& itemId);
    
    // Metadata Operations
    
    /**
     * @brief Get item metadata
     * 
     * Retrieves detailed metadata for a OneDrive item including BFS attributes.
     * 
     * @param itemId OneDrive item ID
     * @param item Reference to store item information
     * @return OneDriveError code
     */
    OneDriveError GetItemInfo(const BString& itemId, OneDriveItem& item);
    
    /**
     * @brief Update item metadata
     * 
     * Updates OneDrive item metadata including custom BFS attributes.
     * 
     * @param itemId OneDrive item ID
     * @param metadata BMessage containing metadata to update
     * @return OneDriveError code
     */
    OneDriveError UpdateItemMetadata(const BString& itemId, const BMessage& metadata);
    
    /**
     * @brief Sync BFS attributes to OneDrive
     * 
     * Synchronizes Haiku BFS extended attributes to OneDrive custom metadata.
     * 
     * @param itemId OneDrive item ID
     * @param attributes BMessage containing BFS attributes
     * @return OneDriveError code
     */
    OneDriveError SyncAttributes(const BString& itemId, const BMessage& attributes);
    
    /**
     * @brief Get OneDrive item ID from file path
     * 
     * Retrieves the OneDrive item ID for a given local file path.
     * 
     * @param filePath Local file path
     * @param itemId Output string to store item ID
     * @return OneDriveError code
     */
    OneDriveError GetItemIdByPath(const BString& filePath, BString& itemId);
    
    // User and Drive Information
    
    /**
     * @brief Get user profile information
     * 
     * @param userInfo BMessage to store user profile data
     * @return OneDriveError code
     */
    OneDriveError GetUserProfile(BMessage& userInfo);
    
    /**
     * @brief Get drive information and quota
     * 
     * @param driveInfo BMessage to store drive information
     * @return OneDriveError code
     */
    OneDriveError GetDriveInfo(BMessage& driveInfo);
    
    // Utility Methods
    
    /**
     * @brief Check API connectivity
     * 
     * @return true if API is accessible, false otherwise
     */
    bool IsConnected();
    
    /**
     * @brief Get last error message
     * 
     * @return Human-readable error message from last operation
     */
    BString GetLastError() const;
    
    /**
     * @brief Set request timeout
     * 
     * @param timeoutSeconds Timeout in seconds for HTTP requests
     */
    void SetTimeout(int32 timeoutSeconds);
    
    /**
     * @brief Get connection pool statistics
     * 
     * @param stats BMessage to store connection pool statistics
     * @return B_OK on success
     */
    status_t GetConnectionStats(BMessage& stats);
    
    /**
     * @brief Force rediscovery of connection limits
     * 
     * Useful when network conditions change or after errors.
     */
    void ForceConnectionRediscovery();
    
    /**
     * @brief Set connection pool parameters
     * 
     * @param minConnections Minimum connections to maintain
     * @param probeInterval How often to re-probe limits (seconds)
     */
    void SetConnectionPoolParams(int32 minConnections, int32 probeInterval);

private:
    /// @name HTTP Client Implementation
    /// @{
    
    /**
     * @brief Make HTTP request to Graph API
     * 
     * @param method HTTP method
     * @param endpoint API endpoint (relative to base URL)
     * @param requestBody Request body data (NULL for GET)
     * @param responseData Response data storage
     * @param customHeaders Custom HTTP headers
     * @return OneDriveError code
     */
    OneDriveError _MakeRequest(HttpMethod method,
                              const BString& endpoint,
                              const BString* requestBody,
                              BMallocIO& responseData,
                              const BMessage* customHeaders = NULL);
    
    /**
     * @brief Make actual HTTP request using networking APIs
     * 
     * @param method HTTP method
     * @param endpoint API endpoint (relative to base URL)
     * @param requestBody Request body data (NULL for GET)
     * @param responseData Response data storage
     * @param customHeaders Custom HTTP headers
     * @return OneDriveError code
     */
    OneDriveError _MakeHttpRequest(HttpMethod method,
                                  const BString& endpoint,
                                  const BString* requestBody,
                                  BMallocIO& responseData,
                                  const BMessage* customHeaders = NULL);
    
    /**
     * @brief Generate mock API response for development
     * 
     * @param endpoint API endpoint
     * @param method HTTP method
     * @param response Reference to store mock response
     * @return OneDriveError code
     */
    OneDriveError _GenerateMockResponse(const BString& endpoint,
                                       HttpMethod method,
                                       BString& response);
    
    /**
     * @brief Add authentication headers to request
     * 
     * @param headers BMessage to add headers to
     * @return B_OK on success, error code on failure
     */
    status_t _AddAuthHeaders(BMessage& headers);
    
    /**
     * @brief Parse JSON response from API
     * 
     * @param jsonData Raw JSON response data
     * @param parsedMessage BMessage to store parsed data
     * @return B_OK on success, error code on failure
     */
    status_t _ParseJsonResponse(const BString& jsonData, BMessage& parsedMessage);
    
    /**
     * @brief Handle API error response
     * 
     * @param responseData Response data containing error
     * @param httpStatus HTTP status code
     * @return Appropriate OneDriveError code
     */
    OneDriveError _HandleApiError(const BString& responseData, int32 httpStatus);
    
    /**
     * @brief Check if request should be retried
     * 
     * @param error OneDriveError code
     * @param retryCount Current retry attempt number
     * @return true if should retry, false otherwise
     */
    bool _ShouldRetry(OneDriveError error, int32 retryCount);
    
    /**
     * @brief Upload file using upload session for large files
     * 
     * @param localPath Local file path
     * @param remotePath Remote path in OneDrive
     * @param progressCallback Progress callback
     * @param userData User data for callback
     * @return OneDriveError code
     */
    OneDriveError _UploadLargeFile(const BString& localPath,
                                  const BString& remotePath,
                                  void (*progressCallback)(float, void*),
                                  void* userData);
    
    /// @}
    
    /// @name JSON Parsing Helpers
    /// @{
    
    /**
     * @brief Parse OneDrive item from JSON
     * 
     * @param jsonItem JSON object containing item data
     * @param item OneDriveItem to populate
     * @return B_OK on success, error code on failure
     */
    status_t _ParseOneDriveItem(const BString& jsonItem, OneDriveItem& item);
    
    /**
     * @brief Convert BFS attributes to JSON metadata
     * 
     * @param attributes BMessage containing BFS attributes
     * @param jsonMetadata String to store JSON representation
     * @return B_OK on success, error code on failure
     */
    status_t _AttributesToJson(const BMessage& attributes, BString& jsonMetadata);
    
    /**
     * @brief Convert JSON metadata to BFS attributes
     * 
     * @param jsonMetadata JSON string containing metadata
     * @param attributes BMessage to store BFS attributes
     * @return B_OK on success, error code on failure
     */
    status_t _JsonToAttributes(const BString& jsonMetadata, BMessage& attributes);
    
    /**
     * @brief Parse folder contents from JSON response
     * 
     * @param jsonData JSON response from folder listing API
     * @param items BList to populate with OneDriveItem objects
     * @return OneDriveError code
     */
    OneDriveError _ParseFolderContents(const BString& jsonData, BList& items);
    
    /**
     * @brief Convert HTTP method enum to string
     * 
     * @param method HTTP method enum value
     * @return String representation of HTTP method
     */
    const char* _HttpMethodToString(HttpMethod method);
    
    /**
     * @brief Serialize BFS attributes to JSON format
     * 
     * @param attributes BMessage containing BFS attributes
     * @param jsonOutput String to store JSON representation
     * @return OneDriveError code
     */
    OneDriveError _SerializeAttributesToJson(const BMessage& attributes, BString& jsonOutput);
    
    /**
     * @brief Update OneDrive item metadata
     * 
     * @param itemId OneDrive item ID
     * @param metadataJson JSON string containing metadata
     * @return OneDriveError code
     */
    OneDriveError _UpdateItemMetadata(const BString& itemId, const BString& metadataJson);
    
    /**
     * @brief Get OneDrive item ID by file path
     * 
     * @param filePath Local file path
     * @param itemId String to store OneDrive item ID
     * @return OneDriveError code
     */
    OneDriveError _GetItemIdByPath(const BString& filePath, BString& itemId);
    
    /**
     * @brief Serialize attribute data based on type
     * 
     * @param type BFS attribute type code
     * @param data Pointer to attribute data
     * @param size Size of data in bytes
     * @param output String to store serialized data
     */
    void _SerializeAttributeData(type_code type, const void* data, size_t size, BString& output);
    
    /**
     * @brief Convert type code to string representation
     * 
     * @param type BFS type code
     * @return String representation of type
     */
    BString _TypeCodeToString(type_code type);
    
    /**
     * @brief Convert local file path to OneDrive path
     * 
     * @param localPath Local filesystem path
     * @return OneDrive path representation
     */
    BString _LocalPathToOneDrivePath(const BString& localPath);
    
    /**
     * @brief Encode binary data as Base64
     * 
     * @param data Pointer to binary data
     * @param size Size of data in bytes
     * @param base64Output String to store Base64 encoded data
     */
    void _EncodeBase64(const void* data, size_t size, BString& base64Output);
    
    /**
     * @brief Extract custom metadata from OneDrive item JSON
     * 
     * @param jsonItem JSON string containing OneDrive item
     * @param attributes BMessage to store extracted attributes
     */
    void _ExtractCustomMetadata(const BString& jsonItem, BMessage& attributes);
    
    /**
     * @brief Parse Haiku attributes from JSON metadata
     * 
     * @param jsonData JSON string containing Haiku attributes
     * @param attributes BMessage to store parsed attributes
     */
    void _ParseHaikuAttributesJson(const BString& jsonData, BMessage& attributes);
    
    /**
     * @brief Parse a single attribute from JSON
     * 
     * @param attrJson JSON string for single attribute
     * @param attributes BMessage to add parsed attribute to
     */
    void _ParseSingleAttribute(const BString& attrJson, BMessage& attributes);
    
    /**
     * @brief Extract string value from JSON by key
     * 
     * @param json JSON string to search
     * @param key Key to find
     * @param value Output string value
     * @return true if found, false otherwise
     */
    bool _ExtractJsonString(const BString& json, const char* key, BString& value);
    
    /**
     * @brief Extract any value from JSON by key
     * 
     * @param json JSON string to search
     * @param key Key to find
     * @param value Output value as string
     * @return true if found, false otherwise
     */
    bool _ExtractJsonValue(const BString& json, const char* key, BString& value);
    
    /**
     * @brief Convert type string to Haiku type code
     * 
     * @param typeString String representation of type
     * @return Haiku type code
     */
    type_code _StringToTypeCode(const BString& typeString);
    
    /**
     * @brief Decode Base64 string to binary data
     * 
     * @param base64Input Base64 encoded string
     * @param data Output pointer to decoded data
     * @param size Output size of decoded data
     * @return B_OK on success, error code on failure
     */
    status_t _DecodeBase64(const BString& base64Input, void** data, size_t* size);
    
    /**
     * @brief Find matching closing brace/bracket in JSON
     * 
     * @param json JSON string to search
     * @param startPos Position of opening character
     * @param openChar Opening character ('{' or '[')
     * @param closeChar Closing character ('}' or ']')
     * @return Position of matching closing character, or -1 if not found
     */
    int32 _FindMatchingBrace(const BString& json, int32 startPos, char openChar, char closeChar);
    
    /// @}
    
    /// @name Member Variables
    /// @{
    
    AuthenticationManager&  fAuthManager;      ///< Reference to authentication manager
    mutable BLocker         fLock;             ///< Thread safety lock
    
    BString                 fLastError;        ///< Last error message
    int32                   fTimeout;          ///< Request timeout in seconds
    int32                   fRetryCount;       ///< Maximum retry attempts
    bool                    fDevelopmentMode;  ///< Development mode flag
    
    // Connection pool
    std::unique_ptr<OneDrive::ConnectionPool> fConnectionPool; ///< Adaptive connection pool
    void*                   fUrlContext;       ///< URL context for sessions (BUrlContext*)
    
    // Rate limiting
    time_t                  fLastRequestTime;  ///< Time of last API request
    int32                   fRequestCount;     ///< Requests made in current period
    
    /// @}
    
    /// @name Static Constants
    /// @{
    
    static const int32 kDefaultTimeout;       ///< Default request timeout
    static const int32 kMaxRetries;           ///< Maximum retry attempts
    static const int32 kLargeFileThreshold;   ///< Threshold for upload sessions
    static const int32 kRateLimitWindow;      ///< Rate limit time window
    static const int32 kMaxRequestsPerWindow; ///< Max requests per time window
    
    /// @}
};

#endif // ONEDRIVE_API_H