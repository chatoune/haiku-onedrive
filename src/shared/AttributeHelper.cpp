/**
 * @file AttributeHelper.cpp
 * @brief Implementation of helper utilities for BFS attribute operations
 * @author Haiku OneDrive Team
 * @version 1.0.0
 * @date 2025-08-15
 */

#include "AttributeHelper.h"
#include "FileSystemConstants.h"
#include "ErrorLogger.h"

#include <fs_attr.h>
#include <string.h>

namespace OneDrive {

/**
 * @brief Read string attribute
 */
status_t
AttributeHelper::ReadStringAttribute(BNode& node, const char* attrName, 
                                    BString& value)
{
    attr_info info;
    status_t result = node.GetAttrInfo(attrName, &info);
    if (result != B_OK) {
        return result;
    }
    
    if (info.type != B_STRING_TYPE) {
        return B_BAD_TYPE;
    }
    
    char* buffer = new char[info.size + 1];
    ssize_t bytesRead = node.ReadAttr(attrName, B_STRING_TYPE, 0, 
                                     buffer, info.size);
    
    if (bytesRead < 0) {
        delete[] buffer;
        return bytesRead;
    }
    
    buffer[bytesRead] = '\0';
    value = buffer;
    delete[] buffer;
    
    return B_OK;
}

/**
 * @brief Write string attribute
 */
status_t
AttributeHelper::WriteStringAttribute(BNode& node, const char* attrName,
                                     const BString& value)
{
    ssize_t written = node.WriteAttr(attrName, B_STRING_TYPE, 0,
                                    value.String(), value.Length() + 1);
    
    return (written == value.Length() + 1) ? B_OK : B_ERROR;
}

/**
 * @brief Read int32 attribute
 */
status_t
AttributeHelper::ReadInt32Attribute(BNode& node, const char* attrName,
                                   int32& value)
{
    ssize_t bytesRead = node.ReadAttr(attrName, B_INT32_TYPE, 0,
                                     &value, sizeof(int32));
    
    return (bytesRead == sizeof(int32)) ? B_OK : B_ENTRY_NOT_FOUND;
}

/**
 * @brief Write int32 attribute
 */
status_t
AttributeHelper::WriteInt32Attribute(BNode& node, const char* attrName,
                                    int32 value)
{
    ssize_t written = node.WriteAttr(attrName, B_INT32_TYPE, 0,
                                    &value, sizeof(int32));
    
    return (written == sizeof(int32)) ? B_OK : B_ERROR;
}

/**
 * @brief Read bool attribute
 */
status_t
AttributeHelper::ReadBoolAttribute(BNode& node, const char* attrName,
                                  bool& value)
{
    ssize_t bytesRead = node.ReadAttr(attrName, B_BOOL_TYPE, 0,
                                     &value, sizeof(bool));
    
    return (bytesRead == sizeof(bool)) ? B_OK : B_ENTRY_NOT_FOUND;
}

/**
 * @brief Write bool attribute
 */
status_t
AttributeHelper::WriteBoolAttribute(BNode& node, const char* attrName,
                                   bool value)
{
    ssize_t written = node.WriteAttr(attrName, B_BOOL_TYPE, 0,
                                    &value, sizeof(bool));
    
    return (written == sizeof(bool)) ? B_OK : B_ERROR;
}

/**
 * @brief Read time_t attribute
 */
status_t
AttributeHelper::ReadTimeAttribute(BNode& node, const char* attrName,
                                  time_t& value)
{
    ssize_t bytesRead = node.ReadAttr(attrName, B_TIME_TYPE, 0,
                                     &value, sizeof(time_t));
    
    return (bytesRead == sizeof(time_t)) ? B_OK : B_ENTRY_NOT_FOUND;
}

/**
 * @brief Write time_t attribute
 */
status_t
AttributeHelper::WriteTimeAttribute(BNode& node, const char* attrName,
                                   time_t value)
{
    ssize_t written = node.WriteAttr(attrName, B_TIME_TYPE, 0,
                                    &value, sizeof(time_t));
    
    return (written == sizeof(time_t)) ? B_OK : B_ERROR;
}

/**
 * @brief Copy all attributes from one node to another
 */
status_t
AttributeHelper::CopyAllAttributes(BNode& source, BNode& destination,
                                  bool overwrite)
{
    char attrName[B_ATTR_NAME_LENGTH];
    
    source.RewindAttrs();
    
    while (source.GetNextAttrName(attrName) == B_OK) {
        // Skip if attribute exists and we're not overwriting
        if (!overwrite && HasAttribute(destination, attrName)) {
            continue;
        }
        
        attr_info info;
        status_t result = source.GetAttrInfo(attrName, &info);
        if (result != B_OK) {
            LOG_WARNING("AttributeHelper", 
                       "Failed to get info for attribute %s", attrName);
            continue;
        }
        
        // Allocate buffer for attribute data
        uint8* buffer = new uint8[info.size];
        
        ssize_t bytesRead = source.ReadAttr(attrName, info.type, 0,
                                           buffer, info.size);
        if (bytesRead != info.size) {
            delete[] buffer;
            LOG_WARNING("AttributeHelper", 
                       "Failed to read attribute %s", attrName);
            continue;
        }
        
        ssize_t bytesWritten = destination.WriteAttr(attrName, info.type, 0,
                                                     buffer, info.size);
        delete[] buffer;
        
        if (bytesWritten != info.size) {
            LOG_WARNING("AttributeHelper", 
                       "Failed to write attribute %s", attrName);
        }
    }
    
    return B_OK;
}

/**
 * @brief Remove attribute
 */
status_t
AttributeHelper::RemoveAttribute(BNode& node, const char* attrName)
{
    return node.RemoveAttr(attrName);
}

/**
 * @brief Check if attribute exists
 */
bool
AttributeHelper::HasAttribute(BNode& node, const char* attrName)
{
    attr_info info;
    return (node.GetAttrInfo(attrName, &info) == B_OK);
}

/**
 * @brief Get attribute info
 */
status_t
AttributeHelper::GetAttributeInfo(BNode& node, const char* attrName,
                                 attr_info& info)
{
    return node.GetAttrInfo(attrName, &info);
}

} // namespace OneDrive