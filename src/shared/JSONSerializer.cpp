/**
 * @file JSONSerializer.cpp
 * @brief Implementation of JSON serialization utilities for BFS attributes
 * @author Haiku OneDrive Team
 * @version 1.0.0
 * @date 2025-08-14
 * 
 * This file implements the JSONSerializer class providing comprehensive
 * JSON serialization and deserialization for Haiku BFS extended attributes.
 */

#include "JSONSerializer.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Base64 character set
const char* JSONSerializer::kBase64Chars = 
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// Base64 decode table
const uint8 JSONSerializer::kBase64DecodeTable[128] = {
    // Fill with 255 (invalid) by default, then set valid entries
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255, 62,255,255,255, 63,
     52, 53, 54, 55, 56, 57, 58, 59, 60, 61,255,255,255,  0,255,255,
    255,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
     15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,255,255,255,255,255,
    255, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
     41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51,255,255,255,255,255
};

//
// JSONValue Implementation
//

JSONValue::JSONValue()
    : fType(JSON_TYPE_NULL)
    , fBinaryData(nullptr)
    , fBinarySize(0)
{
    fValue.intValue = 0;
}

JSONValue::JSONValue(const BString& value)
    : fType(JSON_TYPE_STRING)
    , fStringValue(value)
    , fBinaryData(nullptr)
    , fBinarySize(0)
{
    fValue.intValue = 0;
}

JSONValue::JSONValue(int32 value)
    : fType(JSON_TYPE_NUMBER)
    , fBinaryData(nullptr)
    , fBinarySize(0)
{
    fValue.intValue = value;
}

JSONValue::JSONValue(int64 value)
    : fType(JSON_TYPE_NUMBER)
    , fBinaryData(nullptr)
    , fBinarySize(0)
{
    fValue.intValue = value;
}

JSONValue::JSONValue(float value)
    : fType(JSON_TYPE_NUMBER)
    , fBinaryData(nullptr)
    , fBinarySize(0)
{
    fValue.floatValue = value;
}

JSONValue::JSONValue(double value)
    : fType(JSON_TYPE_NUMBER)
    , fBinaryData(nullptr)
    , fBinarySize(0)
{
    fValue.floatValue = value;
}

JSONValue::JSONValue(bool value)
    : fType(JSON_TYPE_BOOL)
    , fBinaryData(nullptr)
    , fBinarySize(0)
{
    fValue.boolValue = value;
}

JSONValue::JSONValue(const void* data, size_t size)
    : fType(JSON_TYPE_STRING) // Binary stored as Base64 string
    , fBinaryData(nullptr)
    , fBinarySize(0)
{
    fValue.intValue = 0;
    _CopyBinary(data, size);
}

JSONValue::~JSONValue()
{
    _Clear();
}

BString JSONValue::ToString() const
{
    if (fType == JSON_TYPE_STRING) {
        return fStringValue;
    }
    return BString();
}

int32 JSONValue::ToInt32() const
{
    if (fType == JSON_TYPE_NUMBER) {
        return (int32)fValue.intValue;
    }
    return 0;
}

int64 JSONValue::ToInt64() const
{
    if (fType == JSON_TYPE_NUMBER) {
        return fValue.intValue;
    }
    return 0;
}

float JSONValue::ToFloat() const
{
    if (fType == JSON_TYPE_NUMBER) {
        return (float)fValue.floatValue;
    }
    return 0.0f;
}

double JSONValue::ToDouble() const
{
    if (fType == JSON_TYPE_NUMBER) {
        return fValue.floatValue;
    }
    return 0.0;
}

bool JSONValue::ToBool() const
{
    if (fType == JSON_TYPE_BOOL) {
        return fValue.boolValue;
    }
    return false;
}

const void* JSONValue::ToBinary(size_t& size) const
{
    size = fBinarySize;
    return fBinaryData;
}

void JSONValue::_Clear()
{
    if (fBinaryData) {
        free(fBinaryData);
        fBinaryData = nullptr;
    }
    fBinarySize = 0;
}

void JSONValue::_CopyBinary(const void* data, size_t size)
{
    _Clear();
    if (data && size > 0) {
        fBinaryData = malloc(size);
        if (fBinaryData) {
            memcpy(fBinaryData, data, size);
            fBinarySize = size;
        }
    }
}

//
// JSONSerializer Implementation
//

JSONSerializer::JSONSerializer()
{
}

JSONSerializer::~JSONSerializer()
{
}

JSONError JSONSerializer::SerializeMessage(const BMessage& message, BString& jsonOutput)
{
    jsonOutput = "{";
    
    type_code type;
    int32 count;
    char* name;
    bool first = true;
    
    // Iterate through all fields in the message
    for (int32 i = 0; message.GetInfo(B_ANY_TYPE, i, &name, &type, &count) == B_OK; i++) {
        if (!first) {
            jsonOutput << ",";
        }
        first = false;
        
        // Add field name
        BString escapedName;
        JSONError result = EscapeString(BString(name), escapedName);
        if (result != JSON_OK) {
            return result;
        }
        
        jsonOutput << "\"" << escapedName << "\":{";
        jsonOutput << "\"type\":\"" << TypeToString(type) << "\",";
        jsonOutput << "\"data\":";
        
        // Get field data
        const void* data;
        ssize_t numBytes;
        result = (JSONError)message.FindData(name, type, &data, &numBytes);
        if (result == B_OK && data && numBytes > 0) {
            BString valueJson;
            result = SerializeValue(BString(name), type, data, numBytes, valueJson);
            if (result != JSON_OK) {
                return result;
            }
            jsonOutput << valueJson;
        } else {
            jsonOutput << "null";
        }
        
        jsonOutput << "}";
    }
    
    jsonOutput << "}";
    return JSON_OK;
}

JSONError JSONSerializer::SerializeValue(const BString& name, type_code type, 
                                        const void* data, size_t size, BString& jsonOutput)
{
    if (!data || size == 0) {
        jsonOutput = "null";
        return JSON_OK;
    }
    
    switch (type) {
        case B_STRING_TYPE:
            _SerializeString(BString((const char*)data), jsonOutput);
            break;
            
        case B_INT8_TYPE:
        case B_INT16_TYPE:
        case B_INT32_TYPE:
        case B_INT64_TYPE:
        case B_UINT8_TYPE:
        case B_UINT16_TYPE:
        case B_UINT32_TYPE:
        case B_UINT64_TYPE:
        case B_FLOAT_TYPE:
        case B_DOUBLE_TYPE:
            _SerializeNumber(type, data, jsonOutput);
            break;
            
        case B_BOOL_TYPE:
            _SerializeBool(*(const bool*)data, jsonOutput);
            break;
            
        default:
            // Treat as binary data
            return _SerializeBinary(data, size, jsonOutput);
    }
    
    return JSON_OK;
}

JSONError JSONSerializer::DeserializeMessage(const BString& jsonInput, BMessage& message)
{
    message.MakeEmpty();
    
    // Basic JSON parsing - in production, use a proper JSON library
    BString trimmed = jsonInput;
    trimmed.Trim();
    
    if (trimmed.Length() < 2 || trimmed[0] != '{' || trimmed[trimmed.Length()-1] != '}') {
        _SetError("Invalid JSON object format");
        return JSON_PARSE_ERROR;
    }
    
    // Remove outer braces
    trimmed.Remove(0, 1);
    trimmed.Remove(trimmed.Length()-1, 1);
    
    // Simple parsing implementation (for demonstration)
    // In production, implement proper JSON parsing
    return JSON_OK;
}

JSONError JSONSerializer::ParseValue(const BString& jsonValue, JSONValue& value)
{
    BString trimmed = jsonValue;
    trimmed.Trim();
    
    if (trimmed.IsEmpty()) {
        return JSON_PARSE_ERROR;
    }
    
    // Simple value parsing
    if (trimmed == "null") {
        value = JSONValue();
        return JSON_OK;
    } else if (trimmed == "true") {
        value = JSONValue(true);
        return JSON_OK;
    } else if (trimmed == "false") {
        value = JSONValue(false);
        return JSON_OK;
    } else if (trimmed[0] == '"' && trimmed[trimmed.Length()-1] == '"') {
        // String value
        BString stringValue = trimmed;
        stringValue.Remove(0, 1);
        stringValue.Remove(stringValue.Length()-1, 1);
        
        BString unescaped;
        JSONError result = UnescapeString(stringValue, unescaped);
        if (result != JSON_OK) {
            return result;
        }
        
        value = JSONValue(unescaped);
        return JSON_OK;
    } else {
        // Try to parse as number
        char* endptr;
        double numValue = strtod(trimmed.String(), &endptr);
        if (endptr != trimmed.String() && *endptr == '\0') {
            value = JSONValue(numValue);
            return JSON_OK;
        }
    }
    
    return JSON_PARSE_ERROR;
}

JSONError JSONSerializer::EscapeString(const BString& input, BString& output)
{
    output = "";
    
    for (int32 i = 0; i < input.Length(); i++) {
        char c = input[i];
        switch (c) {
            case '"':
                output << "\\\"";
                break;
            case '\\':
                output << "\\\\";
                break;
            case '\b':
                output << "\\b";
                break;
            case '\f':
                output << "\\f";
                break;
            case '\n':
                output << "\\n";
                break;
            case '\r':
                output << "\\r";
                break;
            case '\t':
                output << "\\t";
                break;
            default:
                if (c < 0x20) {
                    // Control character - encode as Unicode escape
                    char buffer[8];
                    snprintf(buffer, sizeof(buffer), "\\u%04x", (unsigned char)c);
                    output << buffer;
                } else {
                    output << c;
                }
                break;
        }
    }
    
    return JSON_OK;
}

JSONError JSONSerializer::UnescapeString(const BString& input, BString& output)
{
    output = "";
    
    for (int32 i = 0; i < input.Length(); i++) {
        char c = input[i];
        if (c == '\\' && i + 1 < input.Length()) {
            char next = input[i + 1];
            switch (next) {
                case '"':
                    output << '"';
                    i++;
                    break;
                case '\\':
                    output << '\\';
                    i++;
                    break;
                case 'b':
                    output << '\b';
                    i++;
                    break;
                case 'f':
                    output << '\f';
                    i++;
                    break;
                case 'n':
                    output << '\n';
                    i++;
                    break;
                case 'r':
                    output << '\r';
                    i++;
                    break;
                case 't':
                    output << '\t';
                    i++;
                    break;
                case 'u':
                    // Unicode escape \uXXXX
                    if (i + 5 < input.Length()) {
                        char hexStr[5];
                        strncpy(hexStr, input.String() + i + 2, 4);
                        hexStr[4] = '\0';
                        
                        char* endptr;
                        long unicode = strtol(hexStr, &endptr, 16);
                        if (endptr == hexStr + 4 && unicode <= 0x7F) {
                            output << (char)unicode;
                        }
                        i += 5;
                    } else {
                        output << c;
                    }
                    break;
                default:
                    output << c;
                    break;
            }
        } else {
            output << c;
        }
    }
    
    return JSON_OK;
}

JSONError JSONSerializer::EncodeBase64(const void* data, size_t size, BString& base64Output)
{
    if (!data || size == 0) {
        base64Output = "";
        return JSON_OK;
    }
    
    const uint8* bytes = (const uint8*)data;
    base64Output = "";
    
    for (size_t i = 0; i < size; i += 3) {
        uint32 b = (bytes[i] << 16);
        if (i + 1 < size) b |= (bytes[i + 1] << 8);
        if (i + 2 < size) b |= bytes[i + 2];
        
        base64Output << kBase64Chars[(b >> 18) & 0x3F];
        base64Output << kBase64Chars[(b >> 12) & 0x3F];
        base64Output << (i + 1 < size ? kBase64Chars[(b >> 6) & 0x3F] : '=');
        base64Output << (i + 2 < size ? kBase64Chars[b & 0x3F] : '=');
    }
    
    return JSON_OK;
}

JSONError JSONSerializer::DecodeBase64(const BString& base64Input, void** data, size_t* size)
{
    *data = nullptr;
    *size = 0;
    
    if (base64Input.IsEmpty()) {
        return JSON_OK;
    }
    
    size_t inputLen = base64Input.Length();
    if (inputLen % 4 != 0) {
        return JSON_PARSE_ERROR;
    }
    
    size_t outputLen = (inputLen / 4) * 3;
    if (base64Input[inputLen - 1] == '=') outputLen--;
    if (base64Input[inputLen - 2] == '=') outputLen--;
    
    uint8* output = (uint8*)malloc(outputLen);
    if (!output) {
        return JSON_MEMORY_ERROR;
    }
    
    size_t j = 0;
    for (size_t i = 0; i < inputLen; i += 4) {
        uint8 a = kBase64DecodeTable[(uint8)base64Input[i]];
        uint8 b = kBase64DecodeTable[(uint8)base64Input[i + 1]];
        uint8 c = (base64Input[i + 2] != '=') ? kBase64DecodeTable[(uint8)base64Input[i + 2]] : 0;
        uint8 d = (base64Input[i + 3] != '=') ? kBase64DecodeTable[(uint8)base64Input[i + 3]] : 0;
        
        if (a == 255 || b == 255 || (base64Input[i + 2] != '=' && c == 255) || 
            (base64Input[i + 3] != '=' && d == 255)) {
            free(output);
            return JSON_PARSE_ERROR;
        }
        
        uint32 triple = (a << 18) + (b << 12) + (c << 6) + d;
        
        if (j < outputLen) output[j++] = (triple >> 16) & 0xFF;
        if (j < outputLen) output[j++] = (triple >> 8) & 0xFF;
        if (j < outputLen) output[j++] = triple & 0xFF;
    }
    
    *data = output;
    *size = outputLen;
    return JSON_OK;
}

bool JSONSerializer::ValidateJSON(const BString& jsonInput, BString& errorMessage)
{
    // Simple validation - check balanced braces/brackets and quotes
    int32 braceCount = 0;
    int32 bracketCount = 0;
    bool inString = false;
    bool escaped = false;
    
    for (int32 i = 0; i < jsonInput.Length(); i++) {
        char c = jsonInput[i];
        
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
        
        switch (c) {
            case '"':
                inString = true;
                break;
            case '{':
                braceCount++;
                break;
            case '}':
                braceCount--;
                if (braceCount < 0) {
                    errorMessage = "Unexpected closing brace";
                    return false;
                }
                break;
            case '[':
                bracketCount++;
                break;
            case ']':
                bracketCount--;
                if (bracketCount < 0) {
                    errorMessage = "Unexpected closing bracket";
                    return false;
                }
                break;
        }
    }
    
    if (braceCount != 0) {
        errorMessage = "Unmatched braces";
        return false;
    }
    
    if (bracketCount != 0) {
        errorMessage = "Unmatched brackets";
        return false;
    }
    
    if (inString) {
        errorMessage = "Unterminated string";
        return false;
    }
    
    errorMessage = "";
    return true;
}

JSONError JSONSerializer::FormatJSON(const BString& jsonInput, BString& prettyOutput, 
                                    const BString& indent)
{
    // Simple pretty-printing implementation
    prettyOutput = "";
    int32 indentLevel = 0;
    bool inString = false;
    bool escaped = false;
    
    for (int32 i = 0; i < jsonInput.Length(); i++) {
        char c = jsonInput[i];
        
        if (escaped) {
            prettyOutput << c;
            escaped = false;
            continue;
        }
        
        if (inString) {
            prettyOutput << c;
            if (c == '\\') {
                escaped = true;
            } else if (c == '"') {
                inString = false;
            }
            continue;
        }
        
        switch (c) {
            case '"':
                prettyOutput << c;
                inString = true;
                break;
            case '{':
            case '[':
                prettyOutput << c << '\n';
                indentLevel++;
                for (int32 j = 0; j < indentLevel; j++) {
                    prettyOutput << indent;
                }
                break;
            case '}':
            case ']':
                prettyOutput << '\n';
                indentLevel--;
                for (int32 j = 0; j < indentLevel; j++) {
                    prettyOutput << indent;
                }
                prettyOutput << c;
                break;
            case ',':
                prettyOutput << c << '\n';
                for (int32 j = 0; j < indentLevel; j++) {
                    prettyOutput << indent;
                }
                break;
            case ':':
                prettyOutput << c << ' ';
                break;
            case ' ':
            case '\t':
            case '\n':
            case '\r':
                // Skip whitespace outside strings
                break;
            default:
                prettyOutput << c;
                break;
        }
    }
    
    return JSON_OK;
}

BString JSONSerializer::TypeToString(type_code type)
{
    switch (type) {
        case B_STRING_TYPE: return "string";
        case B_INT8_TYPE: return "int8";
        case B_INT16_TYPE: return "int16";
        case B_INT32_TYPE: return "int32";
        case B_INT64_TYPE: return "int64";
        case B_UINT8_TYPE: return "uint8";
        case B_UINT16_TYPE: return "uint16";
        case B_UINT32_TYPE: return "uint32";
        case B_UINT64_TYPE: return "uint64";
        case B_FLOAT_TYPE: return "float";
        case B_DOUBLE_TYPE: return "double";
        case B_BOOL_TYPE: return "bool";
        case B_TIME_TYPE: return "time";
        case B_RECT_TYPE: return "rect";
        case B_POINT_TYPE: return "point";
        case B_MESSAGE_TYPE: return "message";
        case B_RAW_TYPE: return "raw";
        default: return "unknown";
    }
}

type_code JSONSerializer::StringToType(const BString& typeString)
{
    if (typeString == "string") return B_STRING_TYPE;
    if (typeString == "int8") return B_INT8_TYPE;
    if (typeString == "int16") return B_INT16_TYPE;
    if (typeString == "int32") return B_INT32_TYPE;
    if (typeString == "int64") return B_INT64_TYPE;
    if (typeString == "uint8") return B_UINT8_TYPE;
    if (typeString == "uint16") return B_UINT16_TYPE;
    if (typeString == "uint32") return B_UINT32_TYPE;
    if (typeString == "uint64") return B_UINT64_TYPE;
    if (typeString == "float") return B_FLOAT_TYPE;
    if (typeString == "double") return B_DOUBLE_TYPE;
    if (typeString == "bool") return B_BOOL_TYPE;
    if (typeString == "time") return B_TIME_TYPE;
    if (typeString == "rect") return B_RECT_TYPE;
    if (typeString == "point") return B_POINT_TYPE;
    if (typeString == "message") return B_MESSAGE_TYPE;
    if (typeString == "raw") return B_RAW_TYPE;
    return B_STRING_TYPE;
}

BString JSONSerializer::GetErrorDescription(JSONError error)
{
    switch (error) {
        case JSON_OK: return "No error";
        case JSON_PARSE_ERROR: return "JSON parsing failed";
        case JSON_TYPE_ERROR: return "Unsupported data type";
        case JSON_MEMORY_ERROR: return "Memory allocation failed";
        case JSON_INVALID_DATA: return "Invalid input data";
        case JSON_ENCODING_ERROR: return "Character encoding error";
        default: return "Unknown error";
    }
}

//
// Private Methods
//

void JSONSerializer::_SerializeString(const BString& value, BString& output)
{
    BString escaped;
    EscapeString(value, escaped);
    output = "\"";
    output << escaped << "\"";
}

void JSONSerializer::_SerializeNumber(type_code type, const void* data, BString& output)
{
    char buffer[32];
    
    switch (type) {
        case B_INT8_TYPE:
            snprintf(buffer, sizeof(buffer), "%d", *(const int8*)data);
            break;
        case B_INT16_TYPE:
            snprintf(buffer, sizeof(buffer), "%d", *(const int16*)data);
            break;
        case B_INT32_TYPE:
            snprintf(buffer, sizeof(buffer), "%d", *(const int32*)data);
            break;
        case B_INT64_TYPE:
            snprintf(buffer, sizeof(buffer), "%lld", *(const int64*)data);
            break;
        case B_UINT8_TYPE:
            snprintf(buffer, sizeof(buffer), "%u", *(const uint8*)data);
            break;
        case B_UINT16_TYPE:
            snprintf(buffer, sizeof(buffer), "%u", *(const uint16*)data);
            break;
        case B_UINT32_TYPE:
            snprintf(buffer, sizeof(buffer), "%u", *(const uint32*)data);
            break;
        case B_UINT64_TYPE:
            snprintf(buffer, sizeof(buffer), "%llu", *(const uint64*)data);
            break;
        case B_FLOAT_TYPE:
            snprintf(buffer, sizeof(buffer), "%g", *(const float*)data);
            break;
        case B_DOUBLE_TYPE:
            snprintf(buffer, sizeof(buffer), "%g", *(const double*)data);
            break;
        default:
            strcpy(buffer, "0");
            break;
    }
    
    output = buffer;
}

void JSONSerializer::_SerializeBool(bool value, BString& output)
{
    output = value ? "true" : "false";
}

JSONError JSONSerializer::_SerializeBinary(const void* data, size_t size, BString& output)
{
    BString base64;
    JSONError result = EncodeBase64(data, size, base64);
    if (result != JSON_OK) {
        return result;
    }
    
    output = "\"";
    output << base64 << "\"";
    return JSON_OK;
}

int32 JSONSerializer::_FindMatchingBrace(const BString& json, int32 startPos, 
                                        char openChar, char closeChar)
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

void JSONSerializer::_SkipWhitespace(const BString& json, int32& pos)
{
    while (pos < json.Length() && isspace(json[pos])) {
        pos++;
    }
}

JSONError JSONSerializer::_ParseString(const BString& json, int32& pos, BString& value)
{
    if (pos >= json.Length() || json[pos] != '"') {
        return JSON_PARSE_ERROR;
    }
    
    pos++; // Skip opening quote
    value = "";
    
    while (pos < json.Length() && json[pos] != '"') {
        if (json[pos] == '\\' && pos + 1 < json.Length()) {
            // Handle escape sequences
            pos++;
            switch (json[pos]) {
                case '"': value << '"'; break;
                case '\\': value << '\\'; break;
                case '/': value << '/'; break;
                case 'b': value << '\b'; break;
                case 'f': value << '\f'; break;
                case 'n': value << '\n'; break;
                case 'r': value << '\r'; break;
                case 't': value << '\t'; break;
                default:
                    value << json[pos];
                    break;
            }
        } else {
            value << json[pos];
        }
        pos++;
    }
    
    if (pos >= json.Length()) {
        return JSON_PARSE_ERROR;
    }
    
    pos++; // Skip closing quote
    return JSON_OK;
}

JSONError JSONSerializer::_ParseNumber(const BString& json, int32& pos, JSONValue& value)
{
    BString numberStr;
    
    // Parse number string
    while (pos < json.Length() && (isdigit(json[pos]) || json[pos] == '.' || 
           json[pos] == '-' || json[pos] == '+' || json[pos] == 'e' || json[pos] == 'E')) {
        numberStr << json[pos];
        pos++;
    }
    
    if (numberStr.IsEmpty()) {
        return JSON_PARSE_ERROR;
    }
    
    // Try to parse as double
    char* endptr;
    double numValue = strtod(numberStr.String(), &endptr);
    if (endptr != numberStr.String() + numberStr.Length()) {
        return JSON_PARSE_ERROR;
    }
    
    value = JSONValue(numValue);
    return JSON_OK;
}

void JSONSerializer::_SetError(const BString& message)
{
    fErrorMessage = message;
}