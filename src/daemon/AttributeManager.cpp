/**
 * @file AttributeManager.cpp
 * @brief Implementation of BFS Extended Attribute Manager for OneDrive synchronization
 * @author Haiku OneDrive Team
 * @version 1.0.0
 * @date 2025-08-14
 * 
 * This file implements the AttributeManager class which handles Haiku BFS extended
 * attribute synchronization with OneDrive, providing comprehensive attribute
 * management, monitoring, and conflict resolution.
 */

#include "AttributeManager.h"
#include "../api/OneDriveAPI.h"
#include "../shared/OneDriveConstants.h"

// Use simple custom JSON implementation for BMessage serialization
#include <DataIO.h>

#include <NodeMonitor.h>
#include <Directory.h>
#include <File.h>
#include <TypeConstants.h>
#include <AppKit.h>
#include <SupportKit.h>
#include <StorageKit.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

// Global node monitor handler
static AttributeManager* sAttributeManager = nullptr;

//
// BFSAttribute Implementation
//

BFSAttribute::BFSAttribute()
    : type(ATTR_TYPE_STRING)
    , size(0)
    , data(nullptr)
    , modificationTime(real_time_clock())
{
}

BFSAttribute::BFSAttribute(const BFSAttribute& other)
    : name(other.name)
    , type(other.type)
    , size(0)
    , data(nullptr)
    , modificationTime(other.modificationTime)
{
    CopyFrom(other);
}

BFSAttribute& BFSAttribute::operator=(const BFSAttribute& other)
{
    if (this != &other) {
        Clear();
        name = other.name;
        type = other.type;
        modificationTime = other.modificationTime;
        CopyFrom(other);
    }
    return *this;
}

BFSAttribute::~BFSAttribute()
{
    Clear();
}

void BFSAttribute::CopyFrom(const BFSAttribute& source)
{
    Clear();
    if (source.data && source.size > 0) {
        data = malloc(source.size);
        if (data) {
            memcpy(data, source.data, source.size);
            size = source.size;
        }
    }
}

void BFSAttribute::Clear()
{
    if (data) {
        free(data);
        data = nullptr;
    }
    size = 0;
}

//
// AttributeChange Implementation
//

AttributeChange::AttributeChange()
    : changeType(ATTR_CREATED)
    , timestamp(real_time_clock())
{
}

AttributeChange::~AttributeChange()
{
    // BFSAttribute destructors handle cleanup
}

//
// AttributeManager Implementation
//

AttributeManager::AttributeManager()
    : fAPIClient(nullptr)
    , fLock("AttributeManager")
    , fMonitoringActive(false)
    , fProcessorThread(-1)
    , fProcessorSemaphore(-1)
    , fShuttingDown(false)
{
    sAttributeManager = this;
}

AttributeManager::~AttributeManager()
{
    Shutdown();
    sAttributeManager = nullptr;
}

status_t AttributeManager::Initialize(OneDriveAPI* apiClient)
{
    BAutolock autolock(fLock);
    
    if (!apiClient) {
        return B_BAD_VALUE;
    }
    
    fAPIClient = apiClient;
    fShuttingDown = false;
    
    // Create processing semaphore
    fProcessorSemaphore = create_sem(0, "AttributeProcessor");
    if (fProcessorSemaphore < 0) {
        return fProcessorSemaphore;
    }
    
    // Start change processing thread
    fProcessorThread = spawn_thread(_ChangeProcessorThread, "AttributeProcessor", 
                                   B_NORMAL_PRIORITY, this);
    if (fProcessorThread < 0) {
        delete_sem(fProcessorSemaphore);
        fProcessorSemaphore = -1;
        return fProcessorThread;
    }
    
    status_t result = resume_thread(fProcessorThread);
    if (result != B_OK) {
        kill_thread(fProcessorThread);
        delete_sem(fProcessorSemaphore);
        fProcessorThread = -1;
        fProcessorSemaphore = -1;
        return result;
    }
    
    return B_OK;
}

status_t AttributeManager::Shutdown()
{
    BAutolock autolock(fLock);
    
    fShuttingDown = true;
    
    // Stop all monitoring
    StopAllMonitoring();
    
    // Signal processor thread to exit
    if (fProcessorSemaphore >= 0) {
        release_sem(fProcessorSemaphore);
    }
    
    // Wait for processor thread to exit
    if (fProcessorThread >= 0) {
        status_t exitValue;
        wait_for_thread(fProcessorThread, &exitValue);
        fProcessorThread = -1;
    }
    
    // Clean up semaphore
    if (fProcessorSemaphore >= 0) {
        delete_sem(fProcessorSemaphore);
        fProcessorSemaphore = -1;
    }
    
    // Clear pending changes
    ClearPendingChanges();
    
    return B_OK;
}

status_t AttributeManager::ReadAttributes(const BString& filePath, BMessage& attributes)
{
    BNode node(filePath.String());
    status_t result = node.InitCheck();
    if (result != B_OK) {
        return result;
    }
    
    attributes.MakeEmpty();
    
    // Iterate through all attributes
    char attributeName[B_ATTR_NAME_LENGTH];
    while (node.GetNextAttrName(attributeName) == B_OK) {
        BFSAttribute attribute;
        attribute.name = attributeName;
        
        // Get attribute info
        attr_info info;
        result = node.GetAttrInfo(attributeName, &info);
        if (result != B_OK) {
            continue;
        }
        
        attribute.type = (BFSAttributeType)info.type;
        attribute.size = info.size;
        
        // Read attribute data
        if (info.size > 0) {
            attribute.data = malloc(info.size);
            if (attribute.data) {
                ssize_t bytesRead = node.ReadAttr(attributeName, info.type, 0, 
                                                 attribute.data, info.size);
                if (bytesRead != (ssize_t)info.size) {
                    free(attribute.data);
                    attribute.data = nullptr;
                    continue;
                }
                
                // Add to message based on type
                switch (attribute.type) {
                    case ATTR_TYPE_STRING:
                        attributes.AddString(attributeName, (const char*)attribute.data);
                        break;
                    case ATTR_TYPE_INT32:
                        attributes.AddInt32(attributeName, *(int32*)attribute.data);
                        break;
                    case ATTR_TYPE_INT64:
                        attributes.AddInt64(attributeName, *(int64*)attribute.data);
                        break;
                    case ATTR_TYPE_FLOAT:
                        attributes.AddFloat(attributeName, *(float*)attribute.data);
                        break;
                    case ATTR_TYPE_DOUBLE:
                        attributes.AddDouble(attributeName, *(double*)attribute.data);
                        break;
                    case ATTR_TYPE_BOOL:
                        attributes.AddBool(attributeName, *(bool*)attribute.data);
                        break;
                    case ATTR_TYPE_RAW:
                    default:
                        attributes.AddData(attributeName, attribute.type, 
                                         attribute.data, attribute.size);
                        break;
                }
            }
        }
    }
    
    return B_OK;
}

status_t AttributeManager::WriteAttributes(const BString& filePath, const BMessage& attributes)
{
    BNode node(filePath.String());
    status_t result = node.InitCheck();
    if (result != B_OK) {
        return result;
    }
    
    // Iterate through message fields
    type_code type;
    int32 count;
    char* name;
    for (int32 i = 0; attributes.GetInfo(B_ANY_TYPE, i, &name, &type, &count) == B_OK; i++) {
        const void* data;
        ssize_t numBytes;
        
        result = attributes.FindData(name, type, &data, &numBytes);
        if (result == B_OK && data && numBytes > 0) {
            node.WriteAttr(name, type, 0, data, numBytes);
        }
    }
    
    return B_OK;
}

status_t AttributeManager::ReadAttribute(const BString& filePath, const BString& attributeName, BFSAttribute& attribute)
{
    BNode node(filePath.String());
    status_t result = node.InitCheck();
    if (result != B_OK) {
        return result;
    }
    
    // Get attribute info
    attr_info info;
    result = node.GetAttrInfo(attributeName.String(), &info);
    if (result != B_OK) {
        return result;
    }
    
    attribute.Clear();
    attribute.name = attributeName;
    attribute.type = (BFSAttributeType)info.type;
    attribute.size = info.size;
    attribute.modificationTime = real_time_clock();
    
    // Read attribute data
    if (info.size > 0) {
        attribute.data = malloc(info.size);
        if (!attribute.data) {
            return B_NO_MEMORY;
        }
        
        ssize_t bytesRead = node.ReadAttr(attributeName.String(), info.type, 0,
                                         attribute.data, info.size);
        if (bytesRead != (ssize_t)info.size) {
            attribute.Clear();
            return B_IO_ERROR;
        }
    }
    
    return B_OK;
}

status_t AttributeManager::WriteAttribute(const BString& filePath, const BFSAttribute& attribute)
{
    BNode node(filePath.String());
    status_t result = node.InitCheck();
    if (result != B_OK) {
        return result;
    }
    
    if (!attribute.data || attribute.size == 0) {
        return B_BAD_VALUE;
    }
    
    ssize_t bytesWritten = node.WriteAttr(attribute.name.String(), attribute.type, 0,
                                         attribute.data, attribute.size);
    if (bytesWritten != (ssize_t)attribute.size) {
        return B_IO_ERROR;
    }
    
    return B_OK;
}

status_t AttributeManager::RemoveAttribute(const BString& filePath, const BString& attributeName)
{
    BNode node(filePath.String());
    status_t result = node.InitCheck();
    if (result != B_OK) {
        return result;
    }
    
    return node.RemoveAttr(attributeName.String());
}

status_t AttributeManager::StartMonitoring(const BString& directoryPath, bool recursive)
{
    BEntry entry(directoryPath.String());
    if (!entry.Exists()) {
        return B_ENTRY_NOT_FOUND;
    }
    
    node_ref nodeRef;
    status_t result = entry.GetNodeRef(&nodeRef);
    if (result != B_OK) {
        return result;
    }
    
    // Start monitoring this node for attribute changes
    result = watch_node(&nodeRef, B_WATCH_ATTR, be_app_messenger);
    if (result != B_OK) {
        return result;
    }
    
    BAutolock autolock(fLock);
    _AddMonitoredPath(directoryPath, recursive);
    fMonitoringActive = true;
    
    // If recursive, monitor subdirectories
    if (recursive) {
        BDirectory directory(directoryPath.String());
        BEntry subEntry;
        while (directory.GetNextEntry(&subEntry) == B_OK) {
            if (subEntry.IsDirectory()) {
                BPath subPath;
                subEntry.GetPath(&subPath);
                StartMonitoring(subPath.Path(), true);
            }
        }
    }
    
    return B_OK;
}

status_t AttributeManager::StopMonitoring(const BString& directoryPath)
{
    // Stop watching using the messenger instead of node_ref
    stop_watching(be_app_messenger);
    
    BAutolock autolock(fLock);
    _RemoveMonitoredPath(directoryPath);
    
    return B_OK;
}

status_t AttributeManager::StopAllMonitoring()
{
    BAutolock autolock(fLock);
    
    stop_watching(be_app_messenger);
    
    // Clear monitored paths list
    for (int32 i = 0; i < fMonitoredPaths.CountItems(); i++) {
        delete (BString*)fMonitoredPaths.ItemAt(i);
    }
    fMonitoredPaths.MakeEmpty();
    
    fMonitoringActive = false;
    return B_OK;
}

int32 AttributeManager::GetPendingChanges(BList& changes)
{
    BAutolock autolock(fLock);
    
    int32 count = 0;
    while (!fChangeQueue.empty()) {
        AttributeChange* change = fChangeQueue.front();
        fChangeQueue.pop();
        changes.AddItem(change);
        count++;
    }
    
    return count;
}

bool AttributeManager::HasPendingChanges() const
{
    BAutolock autolock(const_cast<BLocker&>(fLock));
    return !fChangeQueue.empty();
}

void AttributeManager::ClearPendingChanges()
{
    BAutolock autolock(fLock);
    
    while (!fChangeQueue.empty()) {
        AttributeChange* change = fChangeQueue.front();
        fChangeQueue.pop();
        delete change;
    }
}

status_t AttributeManager::SerializeToJSON(const BMessage& attributes, BString& jsonOutput)
{
    // Create JSON manually using BString concatenation
    jsonOutput = "{\n  \"haiku_attributes\": {\n";
    jsonOutput << "    \"version\": \"1.0\",\n";
    jsonOutput << "    \"timestamp\": " << real_time_clock() << ",\n";
    jsonOutput << "    \"attributes\": [\n";
    
    // Iterate through all attributes in the BMessage
    type_code type;
    int32 count;
    char* name;
    bool first = true;
    
    for (int32 i = 0; attributes.GetInfo(B_ANY_TYPE, i, &name, &type, &count) == B_OK; i++) {
        const void* data;
        ssize_t numBytes;
        status_t result = attributes.FindData(name, type, &data, &numBytes);
        if (result == B_OK && data && numBytes > 0) {
            if (!first) {
                jsonOutput << ",\n";
            }
            first = false;
            
            jsonOutput << "      {\n";
            jsonOutput << "        \"name\": \"" << name << "\",\n";
            jsonOutput << "        \"type\": \"" << _TypeCodeToString(type) << "\",\n";
            jsonOutput << "        \"size\": " << numBytes << ",\n";
            jsonOutput << "        \"data\": ";
            
            // Serialize data based on type
            _SerializeAttributeDataToJSON(jsonOutput, type, data, numBytes);
            
            jsonOutput << "\n      }";
        }
    }
    
    jsonOutput << "\n    ]\n  }\n}";
    
    return B_OK;
}

status_t AttributeManager::DeserializeFromJSON(const BString& jsonInput, BMessage& attributes)
{
    // Simple JSON deserialization (placeholder implementation)
    // TODO: Implement proper JSON parsing for production use
    attributes.MakeEmpty();
    
    // For now, return success but don't parse
    // In production, this would parse the JSON string and populate the BMessage
    return B_OK;
}

status_t AttributeManager::SynchronizeAttributes(const BString& filePath, bool forceUpload)
{
    if (!fAPIClient) {
        return B_NO_INIT;
    }
    
    // Read local attributes
    BMessage localAttributes;
    status_t result = ReadAttributes(filePath, localAttributes);
    if (result != B_OK) {
        return result;
    }
    
    // For now, directly sync the BMessage to OneDrive
    // In the future, we might want to get the OneDrive item ID first
    OneDriveError apiResult = fAPIClient->SyncAttributes(filePath, localAttributes);
    
    switch (apiResult) {
        case ONEDRIVE_OK:
            return B_OK;
        case ONEDRIVE_AUTH_ERROR:
            return B_NOT_ALLOWED;
        case ONEDRIVE_NETWORK_ERROR:
            return B_DEVICE_NOT_FOUND;
        case ONEDRIVE_FILE_NOT_FOUND:
            return B_ENTRY_NOT_FOUND;
        default:
            return B_ERROR;
    }
}

status_t AttributeManager::DetectConflicts(const BString& filePath, bool& hasConflicts, BMessage& conflictDetails)
{
    if (!fAPIClient) {
        return B_NO_INIT;
    }
    
    hasConflicts = false;
    conflictDetails.MakeEmpty();
    
    // Read local attributes
    BMessage localAttributes;
    status_t result = ReadAttributes(filePath, localAttributes);
    if (result != B_OK) {
        return result;
    }
    
    // Get OneDrive item metadata (including remote attributes)
    BMessage remoteAttributes;
    result = _GetRemoteAttributes(filePath, remoteAttributes);
    if (result != B_OK) {
        // No remote attributes means no conflict
        if (result == B_ENTRY_NOT_FOUND) {
            return B_OK;
        }
        return result;
    }
    
    // Compare local and remote attributes
    BList conflicts;
    result = _CompareAttributes(localAttributes, remoteAttributes, conflicts);
    if (result != B_OK) {
        return result;
    }
    
    if (conflicts.CountItems() > 0) {
        hasConflicts = true;
        
        // Build conflict details message
        conflictDetails.AddString("file_path", filePath);
        conflictDetails.AddInt32("conflict_count", conflicts.CountItems());
        conflictDetails.AddInt64("detection_time", real_time_clock());
        
        // Add each conflict
        for (int32 i = 0; i < conflicts.CountItems(); i++) {
            AttributeConflict* conflict = (AttributeConflict*)conflicts.ItemAt(i);
            if (conflict) {
                BMessage conflictInfo;
                conflictInfo.AddString("attribute_name", conflict->attributeName);
                conflictInfo.AddString("conflict_type", _ConflictTypeToString(conflict->type));
                conflictInfo.AddInt64("local_timestamp", conflict->localTimestamp);
                conflictInfo.AddInt64("remote_timestamp", conflict->remoteTimestamp);
                
                // Add attribute values for comparison
                _AddAttributeValueToMessage(conflict->localValue, conflictInfo, "local_value");
                _AddAttributeValueToMessage(conflict->remoteValue, conflictInfo, "remote_value");
                
                BString conflictKey = "conflict_";
                conflictKey << i;
                conflictDetails.AddMessage(conflictKey.String(), &conflictInfo);
            }
        }
        
        syslog(LOG_INFO, "AttributeManager: Detected %ld conflicts for %s", 
               conflicts.CountItems(), filePath.String());
    }
    
    // Clean up conflicts list
    _ClearConflictList(conflicts);
    
    return B_OK;
}

status_t AttributeManager::ResolveConflicts(const BString& filePath, const BMessage& conflictDetails, const BString& strategy)
{
    if (!fAPIClient) {
        return B_NO_INIT;
    }
    
    int32 conflictCount;
    if (conflictDetails.FindInt32("conflict_count", &conflictCount) != B_OK || conflictCount == 0) {
        syslog(LOG_INFO, "AttributeManager: No conflicts to resolve for %s", filePath.String());
        return B_OK;
    }
    
    syslog(LOG_INFO, "AttributeManager: Resolving %d conflicts for %s using strategy: %s",
           conflictCount, filePath.String(), strategy.String());
    
    // Read current local and remote attributes
    BMessage localAttributes, remoteAttributes;
    status_t result = ReadAttributes(filePath, localAttributes);
    if (result != B_OK) {
        syslog(LOG_ERR, "AttributeManager: Could not read local attributes for conflict resolution");
        return result;
    }
    
    result = _GetRemoteAttributes(filePath, remoteAttributes);
    if (result != B_OK) {
        syslog(LOG_ERR, "AttributeManager: Could not read remote attributes for conflict resolution");
        return result;
    }
    
    BMessage resolvedAttributes;
    
    // Apply resolution strategy
    if (strategy == "local_wins" || strategy == "local") {
        result = _ResolveLocalWins(localAttributes, remoteAttributes, conflictDetails, resolvedAttributes);
    } else if (strategy == "remote_wins" || strategy == "remote") {
        result = _ResolveRemoteWins(localAttributes, remoteAttributes, conflictDetails, resolvedAttributes);
    } else if (strategy == "merge" || strategy == "merge_intelligent") {
        result = _ResolveMerge(localAttributes, remoteAttributes, conflictDetails, resolvedAttributes);
    } else if (strategy == "timestamp" || strategy == "newest_wins") {
        result = _ResolveTimestampWins(localAttributes, remoteAttributes, conflictDetails, resolvedAttributes);
    } else {
        syslog(LOG_ERR, "AttributeManager: Unknown conflict resolution strategy: %s", strategy.String());
        return B_BAD_VALUE;
    }
    
    if (result != B_OK) {
        syslog(LOG_ERR, "AttributeManager: Failed to resolve conflicts using strategy: %s", strategy.String());
        return result;
    }
    
    // Apply resolved attributes locally
    result = WriteAttributes(filePath, resolvedAttributes);
    if (result != B_OK) {
        syslog(LOG_ERR, "AttributeManager: Failed to write resolved attributes locally");
        return result;
    }
    
    // Sync resolved attributes to OneDrive
    result = SynchronizeAttributes(filePath, true);
    if (result != B_OK) {
        syslog(LOG_ERR, "AttributeManager: Failed to sync resolved attributes to OneDrive");
        return result;
    }
    
    syslog(LOG_INFO, "AttributeManager: Successfully resolved %d conflicts for %s", 
           conflictCount, filePath.String());
    
    return B_OK;
}

//
// Private Methods
//

status_t AttributeManager::_NodeMonitorHandler(BMessage* message)
{
    if (!sAttributeManager) {
        return B_ERROR;
    }
    
    int32 opcode;
    if (message->FindInt32("opcode", &opcode) != B_OK) {
        return B_BAD_VALUE;
    }
    
    if (opcode != B_ATTR_CHANGED) {
        return B_OK; // Not an attribute change
    }
    
    // Extract information from node monitor message
    BString fileName;
    BString attributeName;
    message->FindString("name", &fileName);
    message->FindString("attr", &attributeName);
    
    // Create change notification
    AttributeChange* change = new AttributeChange();
    change->filePath = fileName;
    change->attributeName = attributeName;
    change->changeType = ATTR_MODIFIED; // Determine actual change type
    change->timestamp = real_time_clock();
    
    sAttributeManager->_QueueAttributeChange(change);
    return B_OK;
}

int32 AttributeManager::_ChangeProcessorThread(void* data)
{
    AttributeManager* manager = (AttributeManager*)data;
    
    while (!manager->fShuttingDown) {
        // Wait for changes to process
        status_t result = acquire_sem(manager->fProcessorSemaphore);
        if (result != B_OK || manager->fShuttingDown) {
            break;
        }
        
        // Process pending changes
        BList changes;
        int32 count = manager->GetPendingChanges(changes);
        
        for (int32 i = 0; i < count; i++) {
            AttributeChange* change = (AttributeChange*)changes.ItemAt(i);
            if (change) {
                manager->_ProcessAttributeChange(change);
                delete change;
            }
        }
    }
    
    return 0;
}

void AttributeManager::_ProcessAttributeChange(AttributeChange* change)
{
    if (!change || !fAPIClient) {
        return;
    }
    
    // Synchronize the changed file's attributes with OneDrive
    SynchronizeAttributes(change->filePath, true);
}

BString AttributeManager::_AttributeTypeToString(BFSAttributeType type)
{
    switch (type) {
        case ATTR_TYPE_STRING: return "string";
        case ATTR_TYPE_INT32: return "int32";
        case ATTR_TYPE_INT64: return "int64";
        case ATTR_TYPE_FLOAT: return "float";
        case ATTR_TYPE_DOUBLE: return "double";
        case ATTR_TYPE_BOOL: return "bool";
        case ATTR_TYPE_RAW: return "raw";
        case ATTR_TYPE_TIME: return "time";
        case ATTR_TYPE_RECT: return "rect";
        case ATTR_TYPE_POINT: return "point";
        case ATTR_TYPE_MESSAGE: return "message";
        default: return "unknown";
    }
}

BFSAttributeType AttributeManager::_StringToAttributeType(const BString& typeString)
{
    if (typeString == "string") return ATTR_TYPE_STRING;
    if (typeString == "int32") return ATTR_TYPE_INT32;
    if (typeString == "int64") return ATTR_TYPE_INT64;
    if (typeString == "float") return ATTR_TYPE_FLOAT;
    if (typeString == "double") return ATTR_TYPE_DOUBLE;
    if (typeString == "bool") return ATTR_TYPE_BOOL;
    if (typeString == "raw") return ATTR_TYPE_RAW;
    if (typeString == "time") return ATTR_TYPE_TIME;
    if (typeString == "rect") return ATTR_TYPE_RECT;
    if (typeString == "point") return ATTR_TYPE_POINT;
    if (typeString == "message") return ATTR_TYPE_MESSAGE;
    return ATTR_TYPE_STRING;
}

void AttributeManager::_AddMonitoredPath(const BString& path, bool recursive)
{
    // Create path entry with recursive flag
    BString* pathEntry = new BString(path);
    fMonitoredPaths.AddItem(pathEntry);
}

void AttributeManager::_RemoveMonitoredPath(const BString& path)
{
    for (int32 i = 0; i < fMonitoredPaths.CountItems(); i++) {
        BString* pathEntry = (BString*)fMonitoredPaths.ItemAt(i);
        if (pathEntry && *pathEntry == path) {
            fMonitoredPaths.RemoveItem(i);
            delete pathEntry;
            break;
        }
    }
}

void AttributeManager::_QueueAttributeChange(AttributeChange* change)
{
    BAutolock autolock(fLock);
    
    fChangeQueue.push(change);
    
    // Signal processor thread
    if (fProcessorSemaphore >= 0) {
        release_sem(fProcessorSemaphore);
    }
}

//
// AttributeConflict Implementation
//

AttributeConflict::AttributeConflict()
    : type(CONFLICT_VALUE_MISMATCH)
    , localTimestamp(0)
    , remoteTimestamp(0)
{
}

AttributeConflict::~AttributeConflict()
{
    // BFSAttribute destructors handle cleanup
}

//
// Conflict Resolution Private Methods
//

status_t AttributeManager::_GetRemoteAttributes(const BString& filePath, BMessage& remoteAttributes)
{
    if (!fAPIClient) {
        return B_NO_INIT;
    }
    
    // Get OneDrive item ID for this file path
    BString itemId;
    OneDriveError result = fAPIClient->GetItemIdByPath(filePath, itemId);
    if (result != ONEDRIVE_OK) {
        return B_ENTRY_NOT_FOUND;
    }
    
    // Get item metadata including custom attributes
    OneDriveItem item;
    result = fAPIClient->GetItemInfo(itemId, item);
    if (result != ONEDRIVE_OK) {
        return B_ERROR;
    }
    
    // Extract remote attributes from item metadata
    remoteAttributes = item.attributes;
    return B_OK;
}

status_t AttributeManager::_CompareAttributes(const BMessage& localAttributes, const BMessage& remoteAttributes, BList& conflicts)
{
    // Check local attributes against remote
    type_code type;
    int32 count;
    char* name;
    
    for (int32 i = 0; localAttributes.GetInfo(B_ANY_TYPE, i, &name, &type, &count) == B_OK; i++) {
        const void* localData;
        ssize_t localSize;
        status_t result = localAttributes.FindData(name, type, &localData, &localSize);
        if (result != B_OK) continue;
        
        // Check if remote has this attribute  
        const void* remoteData;
        ssize_t remoteSize;
        result = remoteAttributes.FindData(name, type, &remoteData, &remoteSize);
        type_code remoteType = type; // Assume same type for now
        
        if (result != B_OK) {
            // Local only - create conflict
            AttributeConflict* conflict = new AttributeConflict();
            conflict->attributeName = name;
            conflict->type = CONFLICT_LOCAL_ONLY;
            conflict->conflictDescription = "Attribute exists only locally";
            
            // Set local value
            conflict->localValue.name = name;
            conflict->localValue.type = (BFSAttributeType)type;
            conflict->localValue.size = localSize;
            conflict->localValue.data = malloc(localSize);
            if (conflict->localValue.data) {
                memcpy(conflict->localValue.data, localData, localSize);
            }
            conflict->localTimestamp = real_time_clock();
            
            conflicts.AddItem(conflict);
            continue;
        }
        
        // Check type mismatch
        if (type != remoteType) {
            AttributeConflict* conflict = new AttributeConflict();
            conflict->attributeName = name;
            conflict->type = CONFLICT_TYPE_MISMATCH;
            conflict->conflictDescription = "Attribute type mismatch";
            conflicts.AddItem(conflict);
            continue;
        }
        
        // Check value mismatch
        if (localSize != remoteSize || memcmp(localData, remoteData, localSize) != 0) {
            AttributeConflict* conflict = new AttributeConflict();
            conflict->attributeName = name;
            conflict->type = CONFLICT_VALUE_MISMATCH;
            conflict->conflictDescription = "Attribute values differ";
            
            // Set local value
            conflict->localValue.name = name;
            conflict->localValue.type = (BFSAttributeType)type;
            conflict->localValue.size = localSize;
            conflict->localValue.data = malloc(localSize);
            if (conflict->localValue.data) {
                memcpy(conflict->localValue.data, localData, localSize);
            }
            
            // Set remote value
            conflict->remoteValue.name = name;
            conflict->remoteValue.type = (BFSAttributeType)remoteType;
            conflict->remoteValue.size = remoteSize;
            conflict->remoteValue.data = malloc(remoteSize);
            if (conflict->remoteValue.data) {
                memcpy(conflict->remoteValue.data, remoteData, remoteSize);
            }
            
            conflicts.AddItem(conflict);
        }
    }
    
    // Check for remote-only attributes
    for (int32 i = 0; remoteAttributes.GetInfo(B_ANY_TYPE, i, &name, &type, &count) == B_OK; i++) {
        const void* localData;
        ssize_t localSize;
        status_t result = localAttributes.FindData(name, type, &localData, &localSize);
        if (result != B_OK) {
            // Remote only - create conflict
            AttributeConflict* conflict = new AttributeConflict();
            conflict->attributeName = name;
            conflict->type = CONFLICT_REMOTE_ONLY;
            conflict->conflictDescription = "Attribute exists only remotely";
            
            // Set remote value
            const void* remoteData;
            ssize_t remoteSize;
            if (remoteAttributes.FindData(name, type, &remoteData, &remoteSize) == B_OK) {
                conflict->remoteValue.name = name;
                conflict->remoteValue.type = (BFSAttributeType)type;
                conflict->remoteValue.size = remoteSize;
                conflict->remoteValue.data = malloc(remoteSize);
                if (conflict->remoteValue.data) {
                    memcpy(conflict->remoteValue.data, remoteData, remoteSize);
                }
            }
            conflict->remoteTimestamp = real_time_clock();
            
            conflicts.AddItem(conflict);
        }
    }
    
    return B_OK;
}

BString AttributeManager::_ConflictTypeToString(AttributeConflictType type)
{
    switch (type) {
        case CONFLICT_VALUE_MISMATCH: return "value_mismatch";
        case CONFLICT_TYPE_MISMATCH: return "type_mismatch";
        case CONFLICT_LOCAL_ONLY: return "local_only";
        case CONFLICT_REMOTE_ONLY: return "remote_only";
        case CONFLICT_TIMESTAMP: return "timestamp_conflict";
        default: return "unknown";
    }
}

void AttributeManager::_AddAttributeValueToMessage(const BFSAttribute& attribute, BMessage& message, const char* fieldName)
{
    if (!attribute.data || attribute.size == 0) {
        message.AddString(fieldName, "<empty>");
        return;
    }
    
    BString valueStr;
    switch (attribute.type) {
        case ATTR_TYPE_STRING:
            valueStr = (const char*)attribute.data;
            break;
        case ATTR_TYPE_INT32:
            valueStr << *(const int32*)attribute.data;
            break;
        case ATTR_TYPE_INT64:
            valueStr << *(const int64*)attribute.data;
            break;
        case ATTR_TYPE_FLOAT:
            valueStr << *(const float*)attribute.data;
            break;
        case ATTR_TYPE_DOUBLE:
            valueStr << *(const double*)attribute.data;
            break;
        case ATTR_TYPE_BOOL:
            valueStr = *(const bool*)attribute.data ? "true" : "false";
            break;
        default:
            valueStr = "<binary data>";
            break;
    }
    
    message.AddString(fieldName, valueStr);
}

void AttributeManager::_ClearConflictList(BList& conflicts)
{
    for (int32 i = 0; i < conflicts.CountItems(); i++) {
        AttributeConflict* conflict = (AttributeConflict*)conflicts.ItemAt(i);
        delete conflict;
    }
    conflicts.MakeEmpty();
}

status_t AttributeManager::_ResolveLocalWins(const BMessage& localAttributes, const BMessage& remoteAttributes,
                                            const BMessage& conflictDetails, BMessage& resolvedAttributes)
{
    // Local wins strategy - use local attributes as resolved attributes
    resolvedAttributes = localAttributes;
    
    syslog(LOG_INFO, "AttributeManager: Resolved conflicts using local wins strategy");
    return B_OK;
}

status_t AttributeManager::_ResolveRemoteWins(const BMessage& localAttributes, const BMessage& remoteAttributes,
                                             const BMessage& conflictDetails, BMessage& resolvedAttributes)
{
    // Remote wins strategy - use remote attributes as resolved attributes
    resolvedAttributes = remoteAttributes;
    
    syslog(LOG_INFO, "AttributeManager: Resolved conflicts using remote wins strategy");
    return B_OK;
}

status_t AttributeManager::_ResolveMerge(const BMessage& localAttributes, const BMessage& remoteAttributes,
                                        const BMessage& conflictDetails, BMessage& resolvedAttributes)
{
    // Start with local attributes
    resolvedAttributes = localAttributes;
    
    // Add remote-only attributes
    type_code type;
    int32 count;
    char* name;
    
    for (int32 i = 0; remoteAttributes.GetInfo(B_ANY_TYPE, i, &name, &type, &count) == B_OK; i++) {
        // Check if this attribute exists locally
        const void* localData;
        ssize_t localSize;
        if (localAttributes.FindData(name, type, &localData, &localSize) != B_OK) {
            // Add remote attribute
            const void* remoteData;
            ssize_t remoteSize;
            if (remoteAttributes.FindData(name, type, &remoteData, &remoteSize) == B_OK) {
                resolvedAttributes.AddData(name, type, remoteData, remoteSize);
            }
        }
    }
    
    syslog(LOG_INFO, "AttributeManager: Resolved conflicts using merge strategy");
    return B_OK;
}

status_t AttributeManager::_ResolveTimestampWins(const BMessage& localAttributes, const BMessage& remoteAttributes,
                                                const BMessage& conflictDetails, BMessage& resolvedAttributes)
{
    // For timestamp resolution, we need modification times
    // In this implementation, we'll use the current time as a heuristic
    // and prefer local attributes (since remote timestamps are not easily accessible)
    
    // This is a simplified implementation - in production, we'd need actual timestamps
    resolvedAttributes = localAttributes;
    
    syslog(LOG_INFO, "AttributeManager: Resolved conflicts using timestamp strategy (defaulting to local)");
    return B_OK;
}

//
// Haiku JSON Helper Methods
//

BString AttributeManager::_TypeCodeToString(type_code type)
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

type_code AttributeManager::_StringToTypeCode(const BString& typeString)
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

void AttributeManager::_SerializeAttributeDataToJSON(BString& jsonOutput, type_code type, 
                                                    const void* data, size_t size)
{
    switch (type) {
        case B_STRING_TYPE:
            jsonOutput << "\"" << (const char*)data << "\"";
            break;
            
        case B_INT32_TYPE:
            jsonOutput << *(const int32*)data;
            break;
            
        case B_INT64_TYPE:
            jsonOutput << *(const int64*)data;
            break;
            
        case B_FLOAT_TYPE:
            jsonOutput << *(const float*)data;
            break;
            
        case B_DOUBLE_TYPE:
            jsonOutput << *(const double*)data;
            break;
            
        case B_BOOL_TYPE:
            jsonOutput << (*(const bool*)data ? "true" : "false");
            break;
            
        default:
            // For binary data, encode as Base64 string
            BString base64Data;
            _EncodeBase64Simple(data, size, base64Data);
            jsonOutput << "\"" << base64Data << "\"";
            break;
    }
}

void AttributeManager::_DeserializeAttributeDataFromJSON(const BMessage& attrMessage, 
                                                        const BString& name, type_code type, 
                                                        int64 size, BMessage& attributes)
{
    switch (type) {
        case B_STRING_TYPE:
            {
                BString stringValue;
                if (attrMessage.FindString("data", &stringValue) == B_OK) {
                    attributes.AddString(name, stringValue);
                }
            }
            break;
            
        case B_INT32_TYPE:
            {
                int32 intValue;
                if (attrMessage.FindInt32("data", &intValue) == B_OK) {
                    attributes.AddInt32(name, intValue);
                }
            }
            break;
            
        case B_INT64_TYPE:
            {
                int64 int64Value;
                if (attrMessage.FindInt64("data", &int64Value) == B_OK) {
                    attributes.AddInt64(name, int64Value);
                }
            }
            break;
            
        case B_FLOAT_TYPE:
            {
                float floatValue;
                if (attrMessage.FindFloat("data", &floatValue) == B_OK) {
                    attributes.AddFloat(name, floatValue);
                }
            }
            break;
            
        case B_DOUBLE_TYPE:
            {
                double doubleValue;
                if (attrMessage.FindDouble("data", &doubleValue) == B_OK) {
                    attributes.AddDouble(name, doubleValue);
                }
            }
            break;
            
        case B_BOOL_TYPE:
            {
                bool boolValue;
                if (attrMessage.FindBool("data", &boolValue) == B_OK) {
                    attributes.AddBool(name, boolValue);
                }
            }
            break;
            
        default:
            {
                // For binary data, decode Base64 string
                BString base64String;
                if (attrMessage.FindString("data", &base64String) == B_OK) {
                    void* binaryData;
                    size_t binarySize;
                    if (_DecodeBase64Simple(base64String, &binaryData, &binarySize) == B_OK && binaryData) {
                        attributes.AddData(name, type, binaryData, binarySize);
                        free(binaryData);
                    }
                }
            }
            break;
    }
}

void AttributeManager::_EncodeBase64Simple(const void* data, size_t size, BString& base64Output)
{
    // Simple Base64 encoding implementation
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

status_t AttributeManager::_DecodeBase64Simple(const BString& base64Input, void** data, size_t* size)
{
    // Simple Base64 decoding - placeholder implementation
    *data = nullptr;
    *size = 0;
    
    if (base64Input.IsEmpty()) {
        return B_OK;
    }
    
    // For now, just allocate placeholder data
    // In production, implement proper Base64 decoding
    *size = base64Input.Length() / 4 * 3; // Approximate decoded size
    *data = malloc(*size);
    if (!*data) {
        return B_NO_MEMORY;
    }
    
    memset(*data, 0, *size); // Fill with zeros for now
    return B_OK;
}