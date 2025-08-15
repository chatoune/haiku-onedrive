/**
 * @file AttributeHelper.h
 * @brief Helper utilities for BFS attribute operations
 * @author Haiku OneDrive Team
 * @version 1.0.0
 * @date 2025-08-15
 * 
 * This class provides reusable utilities for reading and writing
 * BFS extended attributes, reducing code duplication across the project.
 */

#ifndef ATTRIBUTE_HELPER_H
#define ATTRIBUTE_HELPER_H

#include <Entry.h>
#include <Node.h>
#include <String.h>
#include <TypeConstants.h>

namespace OneDrive {

/**
 * @brief Helper class for BFS attribute operations
 * 
 * Provides convenient methods for reading and writing various types
 * of extended attributes with proper error handling.
 * 
 * @since 1.0.0
 */
class AttributeHelper {
public:
    /**
     * @brief Read string attribute
     * 
     * @param node Node to read from
     * @param attrName Attribute name
     * @param value Output string value
     * @return B_OK on success, error code otherwise
     */
    static status_t ReadStringAttribute(BNode& node, const char* attrName, 
                                       BString& value);
    
    /**
     * @brief Write string attribute
     * 
     * @param node Node to write to
     * @param attrName Attribute name
     * @param value String value to write
     * @return B_OK on success, error code otherwise
     */
    static status_t WriteStringAttribute(BNode& node, const char* attrName,
                                        const BString& value);
    
    /**
     * @brief Read int32 attribute
     * 
     * @param node Node to read from
     * @param attrName Attribute name
     * @param value Output int32 value
     * @return B_OK on success, error code otherwise
     */
    static status_t ReadInt32Attribute(BNode& node, const char* attrName,
                                      int32& value);
    
    /**
     * @brief Write int32 attribute
     * 
     * @param node Node to write to
     * @param attrName Attribute name
     * @param value Int32 value to write
     * @return B_OK on success, error code otherwise
     */
    static status_t WriteInt32Attribute(BNode& node, const char* attrName,
                                       int32 value);
    
    /**
     * @brief Read bool attribute
     * 
     * @param node Node to read from
     * @param attrName Attribute name
     * @param value Output bool value
     * @return B_OK on success, error code otherwise
     */
    static status_t ReadBoolAttribute(BNode& node, const char* attrName,
                                     bool& value);
    
    /**
     * @brief Write bool attribute
     * 
     * @param node Node to write to
     * @param attrName Attribute name
     * @param value Bool value to write
     * @return B_OK on success, error code otherwise
     */
    static status_t WriteBoolAttribute(BNode& node, const char* attrName,
                                      bool value);
    
    /**
     * @brief Read time_t attribute
     * 
     * @param node Node to read from
     * @param attrName Attribute name
     * @param value Output time_t value
     * @return B_OK on success, error code otherwise
     */
    static status_t ReadTimeAttribute(BNode& node, const char* attrName,
                                     time_t& value);
    
    /**
     * @brief Write time_t attribute
     * 
     * @param node Node to write to
     * @param attrName Attribute name
     * @param value Time_t value to write
     * @return B_OK on success, error code otherwise
     */
    static status_t WriteTimeAttribute(BNode& node, const char* attrName,
                                      time_t value);
    
    /**
     * @brief Copy all attributes from one node to another
     * 
     * @param source Source node
     * @param destination Destination node
     * @param overwrite Whether to overwrite existing attributes
     * @return B_OK on success, error code otherwise
     */
    static status_t CopyAllAttributes(BNode& source, BNode& destination,
                                     bool overwrite = true);
    
    /**
     * @brief Remove attribute
     * 
     * @param node Node to remove attribute from
     * @param attrName Attribute name
     * @return B_OK on success, error code otherwise
     */
    static status_t RemoveAttribute(BNode& node, const char* attrName);
    
    /**
     * @brief Check if attribute exists
     * 
     * @param node Node to check
     * @param attrName Attribute name
     * @return true if attribute exists
     */
    static bool HasAttribute(BNode& node, const char* attrName);
    
    /**
     * @brief Get attribute info
     * 
     * @param node Node to query
     * @param attrName Attribute name
     * @param info Output attribute info
     * @return B_OK on success, error code otherwise
     */
    static status_t GetAttributeInfo(BNode& node, const char* attrName,
                                    attr_info& info);

private:
    // Private constructor to prevent instantiation
    AttributeHelper();
};

} // namespace OneDrive

#endif // ATTRIBUTE_HELPER_H