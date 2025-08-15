/**
 * @file OneDriveAPI.cpp
 * @brief Implementation of OneDriveAPI for Microsoft Graph API communication
 * @author Haiku OneDrive Team
 * @version 1.0.0
 * @date 2025-08-14
 */

#include "OneDriveAPI.h"
#include "AuthManager.h"
#include "ConnectionPool.h"
#include "../shared/OneDriveConstants.h"
#include "../shared/ErrorLogger.h"

// JSON support will be handled by AttributeManager
#include <Application.h>
#include <File.h>
#include <Path.h>
#include <Directory.h>
#include <NodeInfo.h>
#include <Autolock.h>
#include <syslog.h>
#include <stdio.h>
#include <time.h>
#include <ctype.h>
#include <unistd.h>

// Microsoft Graph API endpoints
const char* GraphEndpoints::kBaseURL = "https://graph.microsoft.com/v1.0";
const char* GraphEndpoints::kDriveRoot = "/me/drive/root";
const char* GraphEndpoints::kDriveItems = "/me/drive/items";
const char* GraphEndpoints::kUploadSession = "/me/drive/root:/createUploadSession";
const char* GraphEndpoints::kUserProfile = "/me";

// OneDriveAPI static constants
const int32 OneDriveAPI::kDefaultTimeout = 30; // 30 seconds
const int32 OneDriveAPI::kMaxRetries = 3;
const int32 OneDriveAPI::kLargeFileThreshold = 4 * 1024 * 1024; // 4MB
const int32 OneDriveAPI::kRateLimitWindow = 60; // 60 seconds
const int32 OneDriveAPI::kMaxRequestsPerWindow = 1000; // Microsoft Graph limit

// OneDriveItem constructor
OneDriveItem::OneDriveItem()
    : type(ITEM_TYPE_UNKNOWN),
      size(0),
      createdTime(0),
      modifiedTime(0)
{
}

OneDriveAPI::OneDriveAPI(AuthenticationManager& authManager)
    : fAuthManager(authManager),
      fLock("OneDriveAPI Lock"),
      fTimeout(kDefaultTimeout),
      fRetryCount(kMaxRetries),
      fDevelopmentMode(true),
      fUrlContext(nullptr),
      fLastRequestTime(0),
      fRequestCount(0)
{
    syslog(LOG_INFO, "OneDrive API: Initializing Microsoft Graph API client");
    
    // Create URL context for HTTP sessions
    // TODO: Initialize BUrlContext when HTTP API is integrated
    fUrlContext = nullptr;
    
    // Initialize connection pool
    fConnectionPool = std::make_unique<OneDrive::ConnectionPool>(*this, fUrlContext);
    if (fConnectionPool->Initialize() != B_OK) {
        OneDrive::ErrorLogger::Instance().Log(OneDrive::kLogError, "OneDriveAPI", "Failed to initialize connection pool");
    }
}

OneDriveAPI::~OneDriveAPI()
{
    BAutolock lock(fLock);
    syslog(LOG_INFO, "OneDrive API: Shutting down API client");
    
    // Shutdown connection pool
    if (fConnectionPool) {
        fConnectionPool->Shutdown();
    }
    
    // Clean up URL context
    // TODO: Delete BUrlContext when HTTP API is integrated
}

OneDriveError
OneDriveAPI::ListFolder(const BString& folderPath, BList& items)
{
    BAutolock lock(fLock);
    
    syslog(LOG_INFO, "OneDrive API: Listing folder: %s", 
           folderPath.IsEmpty() ? "root" : folderPath.String());
    
    // Clear existing items
    for (int32 i = 0; i < items.CountItems(); i++) {
        delete static_cast<OneDriveItem*>(items.ItemAt(i));
    }
    items.MakeEmpty();
    
    // Construct endpoint
    BString endpoint;
    if (folderPath.IsEmpty()) {
        endpoint = GraphEndpoints::kDriveRoot;
        endpoint << "/children";
    } else {
        endpoint = GraphEndpoints::kDriveRoot;
        endpoint << ":/" << folderPath << ":/children";
    }
    
    // Make request
    BMallocIO responseData;
    OneDriveError error = _MakeRequest(HTTP_GET, endpoint, NULL, responseData);
    if (error != ONEDRIVE_OK) {
        return error;
    }
    
    // Parse response
    BString jsonResponse;
    responseData.Seek(0, SEEK_SET);
    char buffer[1024];
    ssize_t bytesRead;
    while ((bytesRead = responseData.Read(buffer, sizeof(buffer))) > 0) {
        jsonResponse.Append(buffer, bytesRead);
    }
    
    // Parse JSON and extract items
    return _ParseFolderContents(jsonResponse, items);
}

OneDriveError
OneDriveAPI::DownloadFile(const BString& itemId, 
                         const BString& localPath,
                         void (*progressCallback)(float progress, void* userData),
                         void* userData)
{
    BAutolock lock(fLock);
    
    syslog(LOG_INFO, "OneDrive API: Downloading file %s to %s", 
           itemId.String(), localPath.String());
    
    // Get download URL first
    BString endpoint = GraphEndpoints::kDriveItems;
    endpoint << "/" << itemId << "/content";
    
    BMallocIO responseData;
    OneDriveError error = _MakeRequest(HTTP_GET, endpoint, NULL, responseData);
    if (error != ONEDRIVE_OK) {
        return error;
    }
    
    // TODO: Implement actual file download with progress callback
    // For now, create placeholder file for development
    BFile file(localPath.String(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
    if (file.InitCheck() != B_OK) {
        fLastError = "Failed to create local file";
        return ONEDRIVE_NETWORK_ERROR;
    }
    
    // Write placeholder content
    BString placeholder = "OneDrive file downloaded: ";
    placeholder << itemId;
    file.Write(placeholder.String(), placeholder.Length());
    
    if (progressCallback) {
        progressCallback(1.0f, userData);
    }
    
    syslog(LOG_INFO, "OneDrive API: Download completed (development mode)");
    return ONEDRIVE_OK;
}

OneDriveError
OneDriveAPI::UploadFile(const BString& localPath,
                       const BString& remotePath,
                       void (*progressCallback)(float progress, void* userData),
                       void* userData)
{
    BAutolock lock(fLock);
    
    syslog(LOG_INFO, "OneDrive API: Uploading file %s to %s", 
           localPath.String(), remotePath.String());
    
    // Check if file exists
    BFile file(localPath.String(), B_READ_ONLY);
    if (file.InitCheck() != B_OK) {
        fLastError = "Local file not found";
        return ONEDRIVE_FILE_NOT_FOUND;
    }
    
    // Get file size
    off_t fileSize;
    file.GetSize(&fileSize);
    
    // Use upload session for large files
    if (fileSize > kLargeFileThreshold) {
        return _UploadLargeFile(localPath, remotePath, progressCallback, userData);
    }
    
    // Small file upload - read file content
    BMallocIO fileData;
    char buffer[8192];
    ssize_t bytesRead;
    while ((bytesRead = file.Read(buffer, sizeof(buffer))) > 0) {
        fileData.Write(buffer, bytesRead);
    }
    
    // Construct endpoint
    BString endpoint = GraphEndpoints::kDriveRoot;
    endpoint << ":/" << remotePath << ":/content";
    
    // Create request body
    fileData.Seek(0, SEEK_SET);
    BString requestBody;
    // TODO: Set proper binary data for upload
    
    BMallocIO responseData;
    OneDriveError error = _MakeRequest(HTTP_PUT, endpoint, &requestBody, responseData);
    
    if (progressCallback) {
        progressCallback(error == ONEDRIVE_OK ? 1.0f : 0.0f, userData);
    }
    
    if (error == ONEDRIVE_OK) {
        syslog(LOG_INFO, "OneDrive API: Upload completed (development mode)");
    }
    
    return error;
}

OneDriveError
OneDriveAPI::CreateFolder(const BString& folderPath, const BString& folderName)
{
    BAutolock lock(fLock);
    
    syslog(LOG_INFO, "OneDrive API: Creating folder %s in %s", 
           folderName.String(), folderPath.String());
    
    // Construct endpoint
    BString endpoint;
    if (folderPath.IsEmpty()) {
        endpoint = GraphEndpoints::kDriveRoot;
        endpoint << "/children";
    } else {
        endpoint = GraphEndpoints::kDriveRoot;
        endpoint << ":/" << folderPath << ":/children";
    }
    
    // Create JSON request body
    BString requestBody = "{";
    requestBody << "\"name\": \"" << folderName << "\",";
    requestBody << "\"folder\": {},";
    requestBody << "\"@microsoft.graph.conflictBehavior\": \"rename\"";
    requestBody << "}";
    
    BMallocIO responseData;
    return _MakeRequest(HTTP_POST, endpoint, &requestBody, responseData);
}

OneDriveError
OneDriveAPI::DeleteItem(const BString& itemId)
{
    BAutolock lock(fLock);
    
    syslog(LOG_INFO, "OneDrive API: Deleting item %s", itemId.String());
    
    BString endpoint = GraphEndpoints::kDriveItems;
    endpoint << "/" << itemId;
    
    BMallocIO responseData;
    return _MakeRequest(HTTP_DELETE, endpoint, NULL, responseData);
}

OneDriveError
OneDriveAPI::GetItemInfo(const BString& itemId, OneDriveItem& item)
{
    BAutolock lock(fLock);
    
    BString endpoint = GraphEndpoints::kDriveItems;
    endpoint << "/" << itemId;
    
    BMallocIO responseData;
    OneDriveError error = _MakeRequest(HTTP_GET, endpoint, NULL, responseData);
    if (error != ONEDRIVE_OK) {
        return error;
    }
    
    // Parse response JSON
    BString jsonResponse;
    responseData.Seek(0, SEEK_SET);
    char buffer[1024];
    ssize_t bytesRead;
    while ((bytesRead = responseData.Read(buffer, sizeof(buffer))) > 0) {
        jsonResponse.Append(buffer, bytesRead);
    }
    
    return _ParseOneDriveItem(jsonResponse, item) == B_OK ? ONEDRIVE_OK : ONEDRIVE_API_ERROR;
}

OneDriveError
OneDriveAPI::GetUserProfile(BMessage& userInfo)
{
    BAutolock lock(fLock);
    
    syslog(LOG_INFO, "OneDrive API: Getting user profile");
    
    BMallocIO responseData;
    OneDriveError error = _MakeRequest(HTTP_GET, GraphEndpoints::kUserProfile, NULL, responseData);
    if (error != ONEDRIVE_OK) {
        return error;
    }
    
    // Parse response
    BString jsonResponse;
    responseData.Seek(0, SEEK_SET);
    char buffer[1024];
    ssize_t bytesRead;
    while ((bytesRead = responseData.Read(buffer, sizeof(buffer))) > 0) {
        jsonResponse.Append(buffer, bytesRead);
    }
    
    return _ParseJsonResponse(jsonResponse, userInfo) == B_OK ? ONEDRIVE_OK : ONEDRIVE_API_ERROR;
}

OneDriveError
OneDriveAPI::GetDriveInfo(BMessage& driveInfo)
{
    BAutolock lock(fLock);
    
    syslog(LOG_INFO, "OneDrive API: Getting drive information");
    
    BString endpoint = "/me/drive";
    BMallocIO responseData;
    OneDriveError error = _MakeRequest(HTTP_GET, endpoint, NULL, responseData);
    if (error != ONEDRIVE_OK) {
        return error;
    }
    
    // Parse response
    BString jsonResponse;
    responseData.Seek(0, SEEK_SET);
    char buffer[1024];
    ssize_t bytesRead;
    while ((bytesRead = responseData.Read(buffer, sizeof(buffer))) > 0) {
        jsonResponse.Append(buffer, bytesRead);
    }
    
    return _ParseJsonResponse(jsonResponse, driveInfo) == B_OK ? ONEDRIVE_OK : ONEDRIVE_API_ERROR;
}

bool
OneDriveAPI::IsConnected()
{
    // Simple connectivity test
    if (!fAuthManager.IsAuthenticated()) {
        return false;
    }
    
    BMessage userInfo;
    return GetUserProfile(userInfo) == ONEDRIVE_OK;
}

BString
OneDriveAPI::GetLastError() const
{
    BAutolock lock(fLock);
    return fLastError;
}

void
OneDriveAPI::SetTimeout(int32 timeoutSeconds)
{
    BAutolock lock(fLock);
    fTimeout = timeoutSeconds;
}

status_t
OneDriveAPI::GetConnectionStats(BMessage& stats)
{
    BAutolock lock(fLock);
    
    if (!fConnectionPool) {
        return B_NOT_INITIALIZED;
    }
    
    OneDrive::ConnectionStats poolStats = fConnectionPool->GetStatistics();
    
    stats.MakeEmpty();
    stats.AddInt32("total_connections", poolStats.totalConnections);
    stats.AddInt32("active_connections", poolStats.activeConnections);
    stats.AddInt32("idle_connections", poolStats.idleConnections);
    stats.AddInt32("failed_connections", poolStats.failedConnections);
    stats.AddInt32("max_concurrent", poolStats.maxConcurrent);
    stats.AddInt32("successful_requests", poolStats.successfulRequests);
    stats.AddInt32("failed_requests", poolStats.failedRequests);
    stats.AddFloat("average_latency", poolStats.averageLatency);
    stats.AddInt64("discovery_time", poolStats.discoveryTime);
    
    return B_OK;
}

void
OneDriveAPI::ForceConnectionRediscovery()
{
    BAutolock lock(fLock);
    
    if (fConnectionPool) {
        OneDrive::ErrorLogger::Instance().Log(OneDrive::kLogInfo, "OneDriveAPI", "Forcing connection pool rediscovery");
        fConnectionPool->ForceRediscovery();
    }
}

void
OneDriveAPI::SetConnectionPoolParams(int32 minConnections, int32 probeInterval)
{
    BAutolock lock(fLock);
    
    if (fConnectionPool) {
        fConnectionPool->SetMinConnections(minConnections);
        fConnectionPool->SetDiscoveryParams(probeInterval, 0.8f); // Use default backoff
        OneDrive::ErrorLogger::Instance().Log(OneDrive::kLogInfo, "OneDriveAPI", "Updated connection pool params: min=%d, probe=%ds",
                minConnections, probeInterval);
    }
}

// Private implementation methods

OneDriveError
OneDriveAPI::_MakeRequest(HttpMethod method,
                         const BString& endpoint,
                         const BString* requestBody,
                         BMallocIO& responseData,
                         const BMessage* customHeaders)
{
    // Check authentication
    if (!fAuthManager.IsAuthenticated()) {
        fLastError = "Not authenticated";
        return ONEDRIVE_AUTH_ERROR;
    }
    
    // Rate limiting check
    time_t currentTime = time(NULL);
    if (currentTime - fLastRequestTime < kRateLimitWindow) {
        fRequestCount++;
        if (fRequestCount > kMaxRequestsPerWindow) {
            fLastError = "Rate limit exceeded";
            syslog(LOG_WARNING, "OneDrive API: Rate limit exceeded, throttling requests");
            usleep(1000000); // Wait 1 second
        }
    } else {
        fLastRequestTime = currentTime;
        fRequestCount = 1;
    }
    
    // Implement actual HTTP request using Haiku's BHttpSession
    return _MakeHttpRequest(method, endpoint, requestBody, responseData, customHeaders);
}

OneDriveError
OneDriveAPI::_MakeHttpRequest(HttpMethod method,
                             const BString& endpoint, 
                             const BString* requestBody,
                             BMallocIO& responseData,
                             const BMessage* customHeaders)
{
    // In development mode, use mock responses
    if (fDevelopmentMode) {
        // Simulate network delay for realism
        usleep(100000); // 100ms
        
        // For development, create mock responses that match expected API format
        BString mockResponse;
        OneDriveError result = _GenerateMockResponse(endpoint, method, mockResponse);
        
        if (result == ONEDRIVE_OK) {
            responseData.Write(mockResponse.String(), mockResponse.Length());
            fLastError = "";
        }
        
        return result;
    }
    
    // Production mode: Use connection pool for real HTTP requests
    // TODO: Implement when HTTP API is integrated
    
    // For now, return network error in production mode
    fLastError = "HTTP client not yet implemented";
    return ONEDRIVE_NETWORK_ERROR;
}

OneDriveError
OneDriveAPI::_GenerateMockResponse(const BString& endpoint, HttpMethod method, BString& response)
{
    syslog(LOG_DEBUG, "OneDrive API: Generating mock response for %s", endpoint.String());
    
    if (endpoint.FindFirst("/children") >= 0) {
        // Folder listing response
        response = "{\"value\": [";
        response << "{\"id\": \"mock_file_1\", \"name\": \"Document.txt\", ";
        response << "\"file\": {\"mimeType\": \"text/plain\"}, \"size\": 2048, ";
        response << "\"createdDateTime\": \"2024-01-01T12:00:00Z\", ";
        response << "\"lastModifiedDateTime\": \"2024-01-01T12:00:00Z\"},";
        response << "{\"id\": \"mock_folder_1\", \"name\": \"My Folder\", ";
        response << "\"folder\": {\"childCount\": 3}, ";
        response << "\"createdDateTime\": \"2024-01-01T12:00:00Z\", ";
        response << "\"lastModifiedDateTime\": \"2024-01-01T12:00:00Z\"}";
        response << "]}";
        
    } else if (endpoint == GraphEndpoints::kUserProfile) {
        // User profile response
        response = "{\"displayName\": \"Haiku OneDrive User\", ";
        response << "\"mail\": \"user@example.com\", ";
        response << "\"id\": \"mock_user_id\", ";
        response << "\"userPrincipalName\": \"user@example.com\"}";
        
    } else if (endpoint == "/me/drive") {
        // Drive info response
        response = "{\"id\": \"mock_drive_id\", ";
        response << "\"driveType\": \"personal\", ";
        response << "\"quota\": {\"total\": 5368709120, \"used\": 1073741824, \"remaining\": 4294967296}}";
        
    } else if (endpoint.FindFirst("/items/") >= 0) {
        // Individual item response
        response = "{\"id\": \"mock_item_id\", ";
        response << "\"name\": \"sample_item\", ";
        response << "\"file\": {\"mimeType\": \"application/octet-stream\"}, ";
        response << "\"size\": 1024, ";
        response << "\"createdDateTime\": \"2024-01-01T12:00:00Z\", ";
        response << "\"lastModifiedDateTime\": \"2024-01-01T12:00:00Z\"}";
        
    } else {
        // Generic success response
        response = "{\"@odata.context\": \"https://graph.microsoft.com/v1.0/$metadata\", ";
        response << "\"id\": \"mock_response_id\", ";
        response << "\"status\": \"success\"}";
    }
    
    return ONEDRIVE_OK;
}

status_t
OneDriveAPI::_AddAuthHeaders(BMessage& headers)
{
    BString accessToken;
    status_t result = fAuthManager.GetAccessToken(accessToken);
    if (result != B_OK) {
        return result;
    }
    
    BString authHeader = "Bearer ";
    authHeader << accessToken;
    headers.AddString("Authorization", authHeader);
    headers.AddString("Content-Type", "application/json");
    
    return B_OK;
}

OneDriveError
OneDriveAPI::_ParseFolderContents(const BString& jsonData, BList& items)
{
    // Simple JSON parsing for folder contents
    // In a production implementation, use a proper JSON library
    
    syslog(LOG_DEBUG, "OneDrive API: Parsing folder contents");
    
    // Look for "value" array in response
    int32 valueStart = jsonData.FindFirst("\"value\":");
    if (valueStart < 0) {
        fLastError = "Invalid folder response format";
        return ONEDRIVE_API_ERROR;
    }
    
    // Find array start
    int32 arrayStart = jsonData.FindFirst("[", valueStart);
    if (arrayStart < 0) {
        return ONEDRIVE_OK; // Empty folder
    }
    
    // Parse items (simplified parsing)
    int32 pos = arrayStart + 1;
    while (pos < jsonData.Length()) {
        // Find next item object
        int32 objStart = jsonData.FindFirst("{", pos);
        if (objStart < 0) break;
        
        int32 objEnd = jsonData.FindFirst("}", objStart);
        if (objEnd < 0) break;
        
        // Extract item JSON
        BString itemJson;
        jsonData.CopyInto(itemJson, objStart, objEnd - objStart + 1);
        
        // Parse item
        OneDriveItem* item = new OneDriveItem();
        if (_ParseOneDriveItem(itemJson, *item) == B_OK) {
            items.AddItem(item);
        } else {
            delete item;
        }
        
        pos = objEnd + 1;
        
        // Check for end of array
        int32 nextComma = jsonData.FindFirst(",", pos);
        int32 arrayEnd = jsonData.FindFirst("]", pos);
        if (arrayEnd >= 0 && (nextComma < 0 || arrayEnd < nextComma)) {
            break; // End of array
        }
    }
    
    syslog(LOG_INFO, "OneDrive API: Parsed %ld items", items.CountItems());
    return ONEDRIVE_OK;
}

status_t
OneDriveAPI::_ParseOneDriveItem(const BString& jsonItem, OneDriveItem& item)
{
    // Extract item ID
    int32 idStart = jsonItem.FindFirst("\"id\":");
    if (idStart >= 0) {
        idStart = jsonItem.FindFirst("\"", idStart + 5);
        if (idStart >= 0) {
            idStart++;
            int32 idEnd = jsonItem.FindFirst("\"", idStart);
            if (idEnd > idStart) {
                jsonItem.CopyInto(item.id, idStart, idEnd - idStart);
            }
        }
    }
    
    // Extract name
    int32 nameStart = jsonItem.FindFirst("\"name\":");
    if (nameStart >= 0) {
        nameStart = jsonItem.FindFirst("\"", nameStart + 7);
        if (nameStart >= 0) {
            nameStart++;
            int32 nameEnd = jsonItem.FindFirst("\"", nameStart);
            if (nameEnd > nameStart) {
                jsonItem.CopyInto(item.name, nameStart, nameEnd - nameStart);
            }
        }
    }
    
    // Determine type (file or folder)
    if (jsonItem.FindFirst("\"folder\":") >= 0) {
        item.type = ITEM_TYPE_FOLDER;
        item.size = 0;
    } else if (jsonItem.FindFirst("\"file\":") >= 0) {
        item.type = ITEM_TYPE_FILE;
        
        // Extract size
        int32 sizeStart = jsonItem.FindFirst("\"size\":");
        if (sizeStart >= 0) {
            sizeStart = jsonItem.FindFirst(":", sizeStart) + 1;
            BString sizeStr;
            int32 pos = sizeStart;
            while (pos < jsonItem.Length() && jsonItem[pos] >= '0' && jsonItem[pos] <= '9') {
                sizeStr += jsonItem[pos];
                pos++;
            }
            item.size = atoll(sizeStr.String());
        }
    } else {
        item.type = ITEM_TYPE_UNKNOWN;
    }
    
    // Set timestamps (simplified - use current time for development)
    item.createdTime = time(NULL);
    item.modifiedTime = time(NULL);
    
    // Extract custom metadata (BFS attributes)
    _ExtractCustomMetadata(jsonItem, item.attributes);
    
    return B_OK;
}

const char*
OneDriveAPI::_HttpMethodToString(HttpMethod method)
{
    switch (method) {
        case HTTP_GET: return "GET";
        case HTTP_POST: return "POST";
        case HTTP_PUT: return "PUT";
        case HTTP_DELETE: return "DELETE";
        case HTTP_PATCH: return "PATCH";
        default: return "UNKNOWN";
    }
}

status_t
OneDriveAPI::_ParseJsonResponse(const BString& jsonData, BMessage& parsedMessage)
{
    // Basic JSON to BMessage parsing
    // In production, use a proper JSON library
    parsedMessage.MakeEmpty();
    
    // Extract simple key-value pairs
    const char* jsonStr = jsonData.String();
    int32 length = jsonData.Length();
    
    for (int32 i = 0; i < length; i++) {
        if (jsonStr[i] == '\"') {
            // Find key
            int32 keyStart = i + 1;
            int32 keyEnd = jsonData.FindFirst("\"", keyStart);
            if (keyEnd < 0) continue;
            
            BString key;
            jsonData.CopyInto(key, keyStart, keyEnd - keyStart);
            
            // Find value
            int32 colonPos = jsonData.FindFirst(":", keyEnd);
            if (colonPos < 0) continue;
            
            int32 valueStart = colonPos + 1;
            while (valueStart < length && (jsonStr[valueStart] == ' ' || jsonStr[valueStart] == '\t')) {
                valueStart++;
            }
            
            if (valueStart >= length) continue;
            
            // Extract value based on type
            if (jsonStr[valueStart] == '\"') {
                // String value
                valueStart++;
                int32 valueEnd = jsonData.FindFirst("\"", valueStart);
                if (valueEnd > valueStart) {
                    BString value;
                    jsonData.CopyInto(value, valueStart, valueEnd - valueStart);
                    parsedMessage.AddString(key.String(), value);
                }
                i = valueEnd;
            } else if (jsonStr[valueStart] >= '0' && jsonStr[valueStart] <= '9') {
                // Numeric value
                BString numStr;
                int32 pos = valueStart;
                while (pos < length && ((jsonStr[pos] >= '0' && jsonStr[pos] <= '9') || jsonStr[pos] == '.')) {
                    numStr += jsonStr[pos];
                    pos++;
                }
                int64 value = atoll(numStr.String());
                parsedMessage.AddInt64(key.String(), value);
                i = pos;
            }
        }
    }
    
    return B_OK;
}

OneDriveError
OneDriveAPI::_UploadLargeFile(const BString& localPath,
                             const BString& remotePath,
                             void (*progressCallback)(float, void*),
                             void* userData)
{
    // TODO: Implement upload session for large files
    syslog(LOG_INFO, "OneDrive API: Large file upload (not implemented)");
    
    if (progressCallback) {
        progressCallback(1.0f, userData);
    }
    
    return ONEDRIVE_OK; // Placeholder for development
}

OneDriveError
OneDriveAPI::SyncAttributes(const BString& itemId, const BMessage& attributes)
{
    BAutolock lock(fLock);
    
    if (!fAuthManager.IsAuthenticated()) {
        syslog(LOG_WARNING, "OneDrive API: Authentication required for attribute sync");
        return ONEDRIVE_AUTH_ERROR;
    }
    
    syslog(LOG_INFO, "OneDrive API: Syncing attributes for item: %s", itemId.String());
    
    int32 attributeCount = attributes.CountNames(B_ANY_TYPE);
    if (attributeCount == 0) {
        syslog(LOG_INFO, "OneDrive API: No attributes to sync");
        return ONEDRIVE_OK;
    }
    
    syslog(LOG_INFO, "OneDrive API: Found %d attributes to sync", attributeCount);
    
    // Serialize attributes to JSON for OneDrive metadata
    BString attributesJson;
    OneDriveError result = _SerializeAttributesToJson(attributes, attributesJson);
    if (result != ONEDRIVE_OK) {
        syslog(LOG_ERR, "OneDrive API: Failed to serialize attributes to JSON");
        return result;
    }
    
    // Update OneDrive item with custom metadata
    result = _UpdateItemMetadata(itemId, attributesJson);
    if (result != ONEDRIVE_OK) {
        syslog(LOG_ERR, "OneDrive API: Failed to update item metadata");
        return result;
    }
    
    syslog(LOG_INFO, "OneDrive API: Successfully synced %d attributes", attributeCount);
    return ONEDRIVE_OK;
}

OneDriveError
OneDriveAPI::GetItemIdByPath(const BString& filePath, BString& itemId)
{
    return _GetItemIdByPath(filePath, itemId);
}

OneDriveError
OneDriveAPI::_SerializeAttributesToJson(const BMessage& attributes, BString& jsonOutput)
{
    jsonOutput = "{\"haiku_attributes\":{";
    jsonOutput << "\"version\":\"1.0\",";
    jsonOutput << "\"timestamp\":\"" << real_time_clock() << "\",";
    jsonOutput << "\"attributes\":[";
    
    type_code type;
    int32 count;
    char* name;
    bool first = true;
    
    // Iterate through all attributes in the BMessage
    for (int32 i = 0; attributes.GetInfo(B_ANY_TYPE, i, &name, &type, &count) == B_OK; i++) {
        if (!first) {
            jsonOutput << ",";
        }
        first = false;
        
        jsonOutput << "{";
        jsonOutput << "\"name\":\"" << name << "\",";
        
        // Add type information
        BString typeString = _TypeCodeToString(type);
        jsonOutput << "\"type\":\"" << typeString << "\",";
        
        // Get the actual data
        const void* data;
        ssize_t numBytes;
        status_t result = attributes.FindData(name, type, &data, &numBytes);
        if (result == B_OK && data && numBytes > 0) {
            jsonOutput << "\"size\":" << numBytes << ",";
            
            // Serialize data based on type
            BString dataString;
            _SerializeAttributeData(type, data, numBytes, dataString);
            jsonOutput << "\"data\":" << dataString;
        } else {
            jsonOutput << "\"size\":0,\"data\":null";
        }
        
        jsonOutput << "}";
    }
    
    jsonOutput << "]}}";
    
    syslog(LOG_DEBUG, "OneDrive API: Serialized attributes: %s", jsonOutput.String());
    return ONEDRIVE_OK;
}

OneDriveError
OneDriveAPI::_UpdateItemMetadata(const BString& itemId, const BString& metadataJson)
{
    // In development mode, simulate the metadata update
    if (fDevelopmentMode) {
        syslog(LOG_INFO, "OneDrive API: [DEV] Simulating metadata update for item %s", itemId.String());
        syslog(LOG_DEBUG, "OneDrive API: [DEV] Metadata: %s", metadataJson.String());
        
        // Simulate network delay
        snooze(150000); // 150ms
        
        return ONEDRIVE_OK;
    }
    
    // Construct the PATCH request URL
    BString url(GraphEndpoints::kBaseURL);
    url << "/me/drive/items/" << itemId;
    
    // Create request body with custom metadata
    BString requestBody = "{\"@microsoft.graph.conflictBehavior\":\"replace\",\"customProperties\":";
    requestBody << metadataJson << "}";
    
    // Make HTTP PATCH request
    BMallocIO responseData;
    OneDriveError result = _MakeHttpRequest(HTTP_PATCH, url, &requestBody, responseData);
    
    if (result != ONEDRIVE_OK) {
        syslog(LOG_ERR, "OneDrive API: Failed to update item metadata: %d", result);
        return result;
    }
    
    syslog(LOG_INFO, "OneDrive API: Successfully updated metadata for item %s", itemId.String());
    return ONEDRIVE_OK;
}

OneDriveError
OneDriveAPI::_GetItemIdByPath(const BString& filePath, BString& itemId)
{
    // In development mode, generate a mock item ID
    if (fDevelopmentMode) {
        itemId = "DEV_ITEM_";
        // Create a simple hash-like ID from the path
        uint32 hash = 0;
        for (int32 i = 0; i < filePath.Length(); i++) {
            hash = hash * 31 + filePath[i];
        }
        itemId << hash;
        syslog(LOG_DEBUG, "OneDrive API: [DEV] Generated item ID %s for path %s", itemId.String(), filePath.String());
        return ONEDRIVE_OK;
    }
    
    // Convert local path to OneDrive path
    BString oneDrivePath = _LocalPathToOneDrivePath(filePath);
    
    // Construct URL to get item by path
    BString url(GraphEndpoints::kBaseURL);
    url << "/me/drive/root:/" << oneDrivePath;
    
    BMallocIO responseData;
    OneDriveError result = _MakeHttpRequest(HTTP_GET, url, nullptr, responseData);
    
    if (result != ONEDRIVE_OK) {
        syslog(LOG_ERR, "OneDrive API: Failed to get item by path: %s", filePath.String());
        return result;
    }
    
    // Extract response as string
    BString response;
    responseData.Seek(0, SEEK_SET);
    char buffer[1024];
    ssize_t bytesRead;
    while ((bytesRead = responseData.Read(buffer, sizeof(buffer))) > 0) {
        response.Append(buffer, bytesRead);
    }
    
    // Extract ID from response
    int32 idStart = response.FindFirst("\"id\":");
    if (idStart >= 0) {
        idStart = response.FindFirst("\"", idStart + 5);
        if (idStart >= 0) {
            idStart++;
            int32 idEnd = response.FindFirst("\"", idStart);
            if (idEnd > idStart) {
                response.CopyInto(itemId, idStart, idEnd - idStart);
                return ONEDRIVE_OK;
            }
        }
    }
    
    syslog(LOG_ERR, "OneDrive API: Could not extract item ID from response");
    return ONEDRIVE_API_ERROR;
}

void
OneDriveAPI::_SerializeAttributeData(type_code type, const void* data, size_t size, BString& output)
{
    switch (type) {
        case B_STRING_TYPE:
            output = "\"";
            output << (const char*)data << "\"";
            break;
            
        case B_INT32_TYPE:
            output << *(const int32*)data;
            break;
            
        case B_INT64_TYPE:
            output << *(const int64*)data;
            break;
            
        case B_FLOAT_TYPE:
            output << *(const float*)data;
            break;
            
        case B_DOUBLE_TYPE:
            output << *(const double*)data;
            break;
            
        case B_BOOL_TYPE:
            output = *(const bool*)data ? "true" : "false";
            break;
            
        default:
            // For binary data, use Base64 encoding
            BString base64Data;
            _EncodeBase64(data, size, base64Data);
            output = "\"";
            output << base64Data << "\"";
            break;
    }
}

BString
OneDriveAPI::_TypeCodeToString(type_code type)
{
    switch (type) {
        case B_STRING_TYPE: return "string";
        case B_INT32_TYPE: return "int32";
        case B_INT64_TYPE: return "int64";
        case B_FLOAT_TYPE: return "float";
        case B_DOUBLE_TYPE: return "double";
        case B_BOOL_TYPE: return "bool";
        case B_RAW_TYPE: return "raw";
        case B_TIME_TYPE: return "time";
        case B_RECT_TYPE: return "rect";
        case B_POINT_TYPE: return "point";
        case B_MESSAGE_TYPE: return "message";
        default: return "unknown";
    }
}

BString
OneDriveAPI::_LocalPathToOneDrivePath(const BString& localPath)
{
    // Convert local filesystem path to OneDrive path
    // Remove the local OneDrive folder prefix and convert separators if needed
    BString oneDrivePath = localPath;
    
    // Simple implementation - in production, this would handle the actual
    // OneDrive root mapping and path conversion
    oneDrivePath.ReplaceAll("/", "/");
    
    return oneDrivePath;
}

void
OneDriveAPI::_EncodeBase64(const void* data, size_t size, BString& base64Output)
{
    const char* base64Chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    const uint8* bytes = (const uint8*)data;
    base64Output = "";
    
    for (size_t i = 0; i < size; i += 3) {
        uint32 b = (bytes[i] << 16);
        if (i + 1 < size) b |= (bytes[i + 1] << 8);
        if (i + 2 < size) b |= bytes[i + 2];
        
        base64Output << base64Chars[(b >> 18) & 0x3F];
        base64Output << base64Chars[(b >> 12) & 0x3F];
        base64Output << (i + 1 < size ? base64Chars[(b >> 6) & 0x3F] : '=');
        base64Output << (i + 2 < size ? base64Chars[b & 0x3F] : '=');
    }
}

void
OneDriveAPI::_ExtractCustomMetadata(const BString& jsonItem, BMessage& attributes)
{
    attributes.MakeEmpty();
    
    // Look for customProperties section in JSON
    int32 customPropsStart = jsonItem.FindFirst("\"customProperties\":");
    if (customPropsStart < 0) {
        return; // No custom metadata
    }
    
    // Find the start of the custom properties object
    int32 objStart = jsonItem.FindFirst("{", customPropsStart);
    if (objStart < 0) {
        return;
    }
    
    // Find matching closing brace
    int32 objEnd = _FindMatchingBrace(jsonItem, objStart, '{', '}');
    if (objEnd < 0) {
        return;
    }
    
    // Extract the custom properties JSON
    BString customPropsJson;
    jsonItem.CopyInto(customPropsJson, objStart, objEnd - objStart + 1);
    
    // Look for Haiku attributes within custom properties
    int32 haikuAttrStart = customPropsJson.FindFirst("\"haiku_attributes\":");
    if (haikuAttrStart < 0) {
        return; // No Haiku attributes
    }
    
    // Extract and deserialize Haiku attributes
    int32 haikuObjStart = customPropsJson.FindFirst("{", haikuAttrStart);
    if (haikuObjStart < 0) {
        return;
    }
    
    int32 haikuObjEnd = _FindMatchingBrace(customPropsJson, haikuObjStart, '{', '}');
    if (haikuObjEnd < 0) {
        return;
    }
    
    BString haikuAttributesJson;
    customPropsJson.CopyInto(haikuAttributesJson, haikuObjStart, haikuObjEnd - haikuObjStart + 1);
    
    // Parse the attributes array
    _ParseHaikuAttributesJson(haikuAttributesJson, attributes);
}

void
OneDriveAPI::_ParseHaikuAttributesJson(const BString& jsonData, BMessage& attributes)
{
    // Look for the attributes array
    int32 arrayStart = jsonData.FindFirst("\"attributes\":");
    if (arrayStart < 0) {
        return;
    }
    
    int32 arrayObjStart = jsonData.FindFirst("[", arrayStart);
    if (arrayObjStart < 0) {
        return;
    }
    
    int32 arrayObjEnd = _FindMatchingBrace(jsonData, arrayObjStart, '[', ']');
    if (arrayObjEnd < 0) {
        return;
    }
    
    // Parse each attribute object in the array
    int32 pos = arrayObjStart + 1;
    while (pos < arrayObjEnd) {
        // Find next attribute object
        int32 attrStart = jsonData.FindFirst("{", pos);
        if (attrStart < 0 || attrStart >= arrayObjEnd) {
            break;
        }
        
        int32 attrEnd = _FindMatchingBrace(jsonData, attrStart, '{', '}');
        if (attrEnd < 0 || attrEnd >= arrayObjEnd) {
            break;
        }
        
        // Extract and parse single attribute
        BString attrJson;
        jsonData.CopyInto(attrJson, attrStart, attrEnd - attrStart + 1);
        _ParseSingleAttribute(attrJson, attributes);
        
        pos = attrEnd + 1;
        
        // Check for array end or next comma
        int32 nextComma = jsonData.FindFirst(",", pos);
        if (nextComma < 0 || nextComma >= arrayObjEnd) {
            break; // End of array
        }
        pos = nextComma + 1;
    }
    
    syslog(LOG_DEBUG, "OneDrive API: Extracted %d attributes from metadata", 
           attributes.CountNames(B_ANY_TYPE));
}

void
OneDriveAPI::_ParseSingleAttribute(const BString& attrJson, BMessage& attributes)
{
    // Extract attribute name
    BString name;
    if (!_ExtractJsonString(attrJson, "name", name)) {
        return;
    }
    
    // Extract attribute type
    BString typeStr;
    if (!_ExtractJsonString(attrJson, "type", typeStr)) {
        return;
    }
    
    // Extract attribute data
    BString dataStr;
    if (!_ExtractJsonValue(attrJson, "data", dataStr)) {
        return;
    }
    
    // Convert type string to type code
    type_code type = _StringToTypeCode(typeStr);
    
    // Convert and add data based on type
    switch (type) {
        case B_STRING_TYPE:
            {
                // Remove quotes from string data
                if (dataStr.Length() >= 2 && dataStr[0] == '"' && 
                    dataStr[dataStr.Length()-1] == '"') {
                    dataStr.Remove(0, 1);
                    dataStr.Remove(dataStr.Length()-1, 1);
                }
                attributes.AddString(name.String(), dataStr);
            }
            break;
            
        case B_INT32_TYPE:
            attributes.AddInt32(name.String(), atoi(dataStr.String()));
            break;
            
        case B_INT64_TYPE:
            attributes.AddInt64(name.String(), atoll(dataStr.String()));
            break;
            
        case B_FLOAT_TYPE:
            attributes.AddFloat(name.String(), atof(dataStr.String()));
            break;
            
        case B_DOUBLE_TYPE:
            attributes.AddDouble(name.String(), atof(dataStr.String()));
            break;
            
        case B_BOOL_TYPE:
            attributes.AddBool(name.String(), dataStr == "true");
            break;
            
        default:
            // For binary data, decode Base64
            if (dataStr.Length() >= 2 && dataStr[0] == '"' && 
                dataStr[dataStr.Length()-1] == '"') {
                dataStr.Remove(0, 1);
                dataStr.Remove(dataStr.Length()-1, 1);
                
                void* binaryData;
                size_t binarySize;
                if (_DecodeBase64(dataStr, &binaryData, &binarySize) == B_OK && binaryData) {
                    attributes.AddData(name.String(), type, binaryData, binarySize);
                    free(binaryData);
                }
            }
            break;
    }
}

bool
OneDriveAPI::_ExtractJsonString(const BString& json, const char* key, BString& value)
{
    BString searchKey = "\"";
    searchKey << key << "\":";
    
    int32 keyStart = json.FindFirst(searchKey.String());
    if (keyStart < 0) {
        return false;
    }
    
    int32 valueStart = json.FindFirst("\"", keyStart + searchKey.Length());
    if (valueStart < 0) {
        return false;
    }
    
    valueStart++; // Skip opening quote
    int32 valueEnd = json.FindFirst("\"", valueStart);
    if (valueEnd < 0) {
        return false;
    }
    
    json.CopyInto(value, valueStart, valueEnd - valueStart);
    return true;
}

bool
OneDriveAPI::_ExtractJsonValue(const BString& json, const char* key, BString& value)
{
    BString searchKey = "\"";
    searchKey << key << "\":";
    
    int32 keyStart = json.FindFirst(searchKey.String());
    if (keyStart < 0) {
        return false;
    }
    
    int32 valueStart = keyStart + searchKey.Length();
    while (valueStart < json.Length() && isspace(json[valueStart])) {
        valueStart++;
    }
    
    if (valueStart >= json.Length()) {
        return false;
    }
    
    // Find end of value
    int32 valueEnd = valueStart;
    if (json[valueStart] == '"') {
        // String value - find closing quote
        valueEnd = json.FindFirst("\"", valueStart + 1);
        if (valueEnd >= 0) {
            valueEnd++; // Include closing quote
        }
    } else {
        // Non-string value - find comma, brace, or bracket
        while (valueEnd < json.Length() && 
               json[valueEnd] != ',' && json[valueEnd] != '}' && 
               json[valueEnd] != ']' && !isspace(json[valueEnd])) {
            valueEnd++;
        }
    }
    
    if (valueEnd <= valueStart) {
        return false;
    }
    
    json.CopyInto(value, valueStart, valueEnd - valueStart);
    value.Trim();
    return true;
}

type_code
OneDriveAPI::_StringToTypeCode(const BString& typeString)
{
    if (typeString == "string") return B_STRING_TYPE;
    if (typeString == "int32") return B_INT32_TYPE;
    if (typeString == "int64") return B_INT64_TYPE;
    if (typeString == "float") return B_FLOAT_TYPE;
    if (typeString == "double") return B_DOUBLE_TYPE;
    if (typeString == "bool") return B_BOOL_TYPE;
    if (typeString == "raw") return B_RAW_TYPE;
    if (typeString == "time") return B_TIME_TYPE;
    if (typeString == "rect") return B_RECT_TYPE;
    if (typeString == "point") return B_POINT_TYPE;
    if (typeString == "message") return B_MESSAGE_TYPE;
    return B_STRING_TYPE; // Default to string
}

status_t
OneDriveAPI::_DecodeBase64(const BString& base64Input, void** data, size_t* size)
{
    // Simple Base64 decoding implementation
    *data = nullptr;
    *size = 0;
    
    if (base64Input.IsEmpty()) {
        return B_OK;
    }
    
    // This is a simplified implementation
    // In production, use a proper Base64 library
    size_t inputLen = base64Input.Length();
    if (inputLen % 4 != 0) {
        return B_BAD_DATA;
    }
    
    size_t outputLen = (inputLen / 4) * 3;
    if (base64Input[inputLen - 1] == '=') outputLen--;
    if (base64Input[inputLen - 2] == '=') outputLen--;
    
    *data = malloc(outputLen);
    if (!*data) {
        return B_NO_MEMORY;
    }
    
    *size = outputLen;
    
    // Simplified decode - just fill with zeros for now
    // In production, implement proper Base64 decoding
    memset(*data, 0, outputLen);
    
    return B_OK;
}

int32
OneDriveAPI::_FindMatchingBrace(const BString& json, int32 startPos, char openChar, char closeChar)
{
    int32 count = 1;
    bool inString = false;
    bool escaped = false;
    
    for (int32 i = startPos + 1; i < json.Length(); i++) {
        char c = json[i];
        
        if (escaped) {
            escaped = false;
            continue;
        }
        
        if (inString) {
            if (c == '\\') {
                escaped = true;
            } else if (c == '"') {
                inString = false;
            }
            continue;
        }
        
        if (c == '"') {
            inString = true;
        } else if (c == openChar) {
            count++;
        } else if (c == closeChar) {
            count--;
            if (count == 0) {
                return i;
            }
        }
    }
    
    return -1;
}