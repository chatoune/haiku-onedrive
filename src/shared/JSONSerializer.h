/**
 * @file JSONSerializer.h
 * @brief JSON serialization utilities for BFS attributes
 * @author Haiku OneDrive Team
 * @version 1.0.0
 * @date 2025-08-14
 * 
 * This file contains the JSONSerializer class which provides comprehensive
 * JSON serialization and deserialization for Haiku BFS extended attributes,
 * enabling seamless storage of attribute data in OneDrive metadata.
 */

#ifndef JSON_SERIALIZER_H
#define JSON_SERIALIZER_H

#include <String.h>
#include <Message.h>
#include <TypeConstants.h>
#include <List.h>

/**
 * @brief JSON serialization error codes
 */
enum JSONError {
    JSON_OK = 0,                ///< No error
    JSON_PARSE_ERROR,           ///< JSON parsing failed
    JSON_TYPE_ERROR,            ///< Unsupported data type
    JSON_MEMORY_ERROR,          ///< Memory allocation failed
    JSON_INVALID_DATA,          ///< Invalid input data
    JSON_ENCODING_ERROR         ///< Character encoding error
};

/**
 * @brief JSON node types for parsing
 */
enum JSONNodeType {
    JSON_TYPE_NULL = 0,
    JSON_TYPE_BOOL,
    JSON_TYPE_NUMBER,
    JSON_TYPE_STRING,
    JSON_TYPE_OBJECT,
    JSON_TYPE_ARRAY
};

/**
 * @brief JSON value wrapper for type-safe handling
 */
class JSONValue {
public:
    JSONValue();
    JSONValue(const BString& value);
    JSONValue(int32 value);
    JSONValue(int64 value);
    JSONValue(float value);
    JSONValue(double value);
    JSONValue(bool value);
    JSONValue(const void* data, size_t size); // For binary data
    ~JSONValue();
    
    JSONNodeType GetType() const { return fType; }
    BString ToString() const;
    int32 ToInt32() const;
    int64 ToInt64() const;
    float ToFloat() const;
    double ToDouble() const;
    bool ToBool() const;
    const void* ToBinary(size_t& size) const;
    
    bool IsValid() const { return fType != JSON_TYPE_NULL; }
    
private:
    JSONNodeType fType;
    BString fStringValue;
    union {
        int64 intValue;
        double floatValue;
        bool boolValue;
    } fValue;
    void* fBinaryData;
    size_t fBinarySize;
    
    void _Clear();
    void _CopyBinary(const void* data, size_t size);
};

/**
 * @brief JSON serialization and deserialization utilities
 * 
 * The JSONSerializer class provides comprehensive JSON serialization and
 * deserialization capabilities for Haiku BFS extended attributes. It handles
 * all standard BFS attribute types and provides proper character escaping,
 * binary data encoding (Base64), and robust error handling.
 * 
 * Key features:
 * - Full BFS attribute type support (string, int32/64, float/double, bool, raw)
 * - Base64 encoding for binary data
 * - Proper JSON character escaping
 * - UTF-8 encoding support
 * - Validation of JSON structure
 * - Memory-efficient parsing and generation
 * - Error reporting with detailed messages
 * 
 * The serialization format stores each attribute with type information:
 * ```json
 * {
 *   "attribute_name": {
 *     "type": "string|int32|int64|float|double|bool|raw",
 *     "data": <value_or_base64_string>
 *   }
 * }
 * ```
 * 
 * @see BMessage
 * @see AttributeManager
 * @since 1.0.0
 */
class JSONSerializer {
public:
    /**
     * @brief Constructor
     */
    JSONSerializer();
    
    /**
     * @brief Destructor
     */
    ~JSONSerializer();
    
    // Serialization Methods
    
    /**
     * @brief Serialize BMessage to JSON string
     * 
     * Converts a BMessage containing BFS attributes to a JSON string
     * suitable for storage in OneDrive metadata.
     * 
     * @param message BMessage containing attributes to serialize
     * @param jsonOutput Output string to store JSON data
     * @return JSON_OK on success, error code on failure
     * @retval JSON_OK Serialization successful
     * @retval JSON_MEMORY_ERROR Insufficient memory
     * @retval JSON_TYPE_ERROR Unsupported attribute type
     * @retval JSON_ENCODING_ERROR Character encoding error
     */
    JSONError SerializeMessage(const BMessage& message, BString& jsonOutput);
    
    /**
     * @brief Serialize single value to JSON
     * 
     * Converts a single typed value to its JSON representation.
     * 
     * @param name Field name for the JSON object
     * @param type BFS attribute type
     * @param data Pointer to data to serialize
     * @param size Size of data in bytes
     * @param jsonOutput Output string to append JSON data
     * @return JSON_OK on success, error code on failure
     */
    JSONError SerializeValue(const BString& name, type_code type, 
                            const void* data, size_t size, BString& jsonOutput);
    
    // Deserialization Methods
    
    /**
     * @brief Deserialize JSON string to BMessage
     * 
     * Parses a JSON string containing attribute data and converts it
     * back to a BMessage with proper type information.
     * 
     * @param jsonInput JSON string to parse
     * @param message Output BMessage to store parsed attributes
     * @return JSON_OK on success, error code on failure
     * @retval JSON_OK Deserialization successful
     * @retval JSON_PARSE_ERROR Invalid JSON format
     * @retval JSON_TYPE_ERROR Unknown attribute type
     * @retval JSON_MEMORY_ERROR Insufficient memory
     */
    JSONError DeserializeMessage(const BString& jsonInput, BMessage& message);
    
    /**
     * @brief Parse single JSON value
     * 
     * Extracts and converts a single value from JSON data.
     * 
     * @param jsonValue JSON string containing a single value
     * @param value Output JSONValue object
     * @return JSON_OK on success, error code on failure
     */
    JSONError ParseValue(const BString& jsonValue, JSONValue& value);
    
    // Utility Methods
    
    /**
     * @brief Escape string for JSON format
     * 
     * Properly escapes special characters in strings for JSON compliance.
     * Handles quotes, backslashes, newlines, tabs, and Unicode characters.
     * 
     * @param input String to escape
     * @param output Escaped string suitable for JSON
     * @return JSON_OK on success, error code on failure
     */
    JSONError EscapeString(const BString& input, BString& output);
    
    /**
     * @brief Unescape JSON string
     * 
     * Converts escaped JSON string back to original format.
     * 
     * @param input Escaped JSON string
     * @param output Unescaped string
     * @return JSON_OK on success, error code on failure
     */
    JSONError UnescapeString(const BString& input, BString& output);
    
    /**
     * @brief Encode binary data to Base64
     * 
     * Converts binary data to Base64 string representation for JSON storage.
     * 
     * @param data Binary data to encode
     * @param size Size of data in bytes
     * @param base64Output Base64 encoded string
     * @return JSON_OK on success, error code on failure
     */
    JSONError EncodeBase64(const void* data, size_t size, BString& base64Output);
    
    /**
     * @brief Decode Base64 string to binary data
     * 
     * Converts Base64 encoded string back to binary data.
     * Caller is responsible for freeing returned memory.
     * 
     * @param base64Input Base64 encoded string
     * @param data Output pointer to decoded data (caller owns)
     * @param size Output size of decoded data
     * @return JSON_OK on success, error code on failure
     */
    JSONError DecodeBase64(const BString& base64Input, void** data, size_t* size);
    
    /**
     * @brief Validate JSON string format
     * 
     * Checks if a string contains valid JSON without full parsing.
     * 
     * @param jsonInput JSON string to validate
     * @param errorMessage Output parameter for error details
     * @return true if valid JSON, false otherwise
     */
    bool ValidateJSON(const BString& jsonInput, BString& errorMessage);
    
    /**
     * @brief Get pretty-formatted JSON string
     * 
     * Formats JSON with proper indentation for human readability.
     * 
     * @param jsonInput Compact JSON string
     * @param prettyOutput Formatted JSON with indentation
     * @param indent Indentation string (default: "  ")
     * @return JSON_OK on success, error code on failure
     */
    JSONError FormatJSON(const BString& jsonInput, BString& prettyOutput, 
                        const BString& indent = "  ");
    
    // Type Conversion Utilities
    
    /**
     * @brief Convert BFS attribute type to string
     * 
     * @param type BFS attribute type constant
     * @return String representation of the type
     */
    static BString TypeToString(type_code type);
    
    /**
     * @brief Convert string to BFS attribute type
     * 
     * @param typeString String representation of type
     * @return BFS attribute type constant
     */
    static type_code StringToType(const BString& typeString);
    
    /**
     * @brief Get error description for JSON error code
     * 
     * @param error JSON error code
     * @return Human-readable error description
     */
    static BString GetErrorDescription(JSONError error);

private:
    /// @name Internal State
    /// @{
    BString fErrorMessage;      ///< Last error message for debugging
    /// @}
    
    /// @name Internal Methods
    /// @{
    
    /**
     * @brief Serialize string value with proper escaping
     * 
     * @param value String value to serialize
     * @param output JSON representation
     */
    void _SerializeString(const BString& value, BString& output);
    
    /**
     * @brief Serialize numeric value
     * 
     * @param type Numeric type (int32, int64, float, double)
     * @param data Pointer to numeric data
     * @param output JSON representation
     */
    void _SerializeNumber(type_code type, const void* data, BString& output);
    
    /**
     * @brief Serialize boolean value
     * 
     * @param value Boolean value
     * @param output JSON representation
     */
    void _SerializeBool(bool value, BString& output);
    
    /**
     * @brief Serialize binary data as Base64
     * 
     * @param data Binary data
     * @param size Data size
     * @param output JSON representation with Base64 encoding
     */
    JSONError _SerializeBinary(const void* data, size_t size, BString& output);
    
    /**
     * @brief Find matching brace/bracket in JSON string
     * 
     * @param json JSON string
     * @param startPos Position of opening brace/bracket
     * @param openChar Opening character ('{' or '[')
     * @param closeChar Closing character ('}' or ']')
     * @return Position of matching closing character, -1 if not found
     */
    int32 _FindMatchingBrace(const BString& json, int32 startPos, 
                            char openChar, char closeChar);
    
    /**
     * @brief Skip whitespace in JSON string
     * 
     * @param json JSON string
     * @param pos Current position, updated to skip whitespace
     */
    void _SkipWhitespace(const BString& json, int32& pos);
    
    /**
     * @brief Parse quoted string from JSON
     * 
     * @param json JSON string
     * @param pos Current position, updated after parsing
     * @param value Output string value
     * @return JSON_OK on success, error code on failure
     */
    JSONError _ParseString(const BString& json, int32& pos, BString& value);
    
    /**
     * @brief Parse numeric value from JSON
     * 
     * @param json JSON string
     * @param pos Current position, updated after parsing
     * @param value Output JSONValue with parsed number
     * @return JSON_OK on success, error code on failure
     */
    JSONError _ParseNumber(const BString& json, int32& pos, JSONValue& value);
    
    /**
     * @brief Set internal error message
     * 
     * @param message Error message to set
     */
    void _SetError(const BString& message);
    
    /// @}
    
    /// @name Base64 Implementation Details
    /// @{
    static const char* kBase64Chars;        ///< Base64 character set
    static const uint8 kBase64DecodeTable[]; ///< Base64 decode lookup table
    /// @}
};

#endif // JSON_SERIALIZER_H