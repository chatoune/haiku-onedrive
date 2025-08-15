/**
 * @file ErrorLogger.cpp
 * @brief Implementation of centralized error logging
 * @author Haiku OneDrive Team
 * @version 1.0.0
 * @date 2025-08-15
 */

#include "ErrorLogger.h"

#include <Autolock.h>
#include <FindDirectory.h>
#include <Path.h>

#include <stdio.h>
#include <stdarg.h>
#include <time.h>

namespace OneDrive {

// Singleton instance
ErrorLogger* ErrorLogger::sInstance = NULL;

/**
 * @brief Get the singleton instance
 */
ErrorLogger&
ErrorLogger::Instance()
{
    if (!sInstance) {
        sInstance = new ErrorLogger();
    }
    return *sInstance;
}

/**
 * @brief Private constructor
 */
ErrorLogger::ErrorLogger()
    : fInitialized(false),
      fLogToFile(true),
      fLogToConsole(true),
      fMinimumLevel(kLogInfo),
      fLogFile(NULL),
      fMaxHistorySize(1000)
{
}

/**
 * @brief Destructor
 */
ErrorLogger::~ErrorLogger()
{
    Shutdown();
}

/**
 * @brief Initialize the error logger
 */
status_t
ErrorLogger::Initialize(bool logToFile, bool logToConsole,
                       const char* logFilePath)
{
    if (fInitialized) {
        return B_OK;
    }
    
    fLogToFile = logToFile;
    fLogToConsole = logToConsole;
    
    if (fLogToFile) {
        if (logFilePath) {
            fLogFilePath = logFilePath;
        } else {
            // Default log path
            BPath path;
            if (find_directory(B_USER_LOG_DIRECTORY, &path) == B_OK) {
                path.Append("OneDrive.log");
                fLogFilePath = path.Path();
            } else {
                fLogFilePath = "/tmp/OneDrive.log";
            }
        }
        
        fLogFile = fopen(fLogFilePath.String(), "a");
        if (!fLogFile) {
            fLogToFile = false;
            return B_ERROR;
        }
    }
    
    fInitialized = true;
    
    Log(kLogInfo, "ErrorLogger", "Logger initialized (file: %s)",
        fLogToFile ? fLogFilePath.String() : "disabled");
    
    return B_OK;
}

/**
 * @brief Shutdown the error logger
 */
void
ErrorLogger::Shutdown()
{
    if (!fInitialized) {
        return;
    }
    
    Log(kLogInfo, "ErrorLogger", "Logger shutting down");
    
    if (fLogFile) {
        fclose(fLogFile);
        fLogFile = NULL;
    }
    
    ClearHistory();
    fInitialized = false;
}

/**
 * @brief Log a message
 */
void
ErrorLogger::Log(LogLevel level, const char* component, const char* format, ...)
{
    if (!fInitialized || level < fMinimumLevel) {
        return;
    }
    
    // Create log entry
    LogEntry* entry = new LogEntry();
    entry->timestamp = time(NULL);
    entry->level = level;
    entry->component = component;
    entry->errorCode = B_OK;
    
    // Format message
    va_list args;
    va_start(args, format);
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    entry->message = buffer;
    
    // Write log entry
    _WriteLogEntry(*entry);
    
    // Store in history
    BAutolock lock(fLock);
    fRecentEntries.AddItem(entry);
    
    // Trim history if needed
    while (fRecentEntries.CountItems() > fMaxHistorySize) {
        LogEntry* old = static_cast<LogEntry*>(fRecentEntries.RemoveItem((int32)0));
        delete old;
    }
}

/**
 * @brief Log an error with status code
 */
void
ErrorLogger::LogError(const char* component, status_t error, 
                     const char* format, ...)
{
    if (!fInitialized || kLogError < fMinimumLevel) {
        return;
    }
    
    // Create log entry
    LogEntry* entry = new LogEntry();
    entry->timestamp = time(NULL);
    entry->level = kLogError;
    entry->component = component;
    entry->errorCode = error;
    
    // Format message
    va_list args;
    va_start(args, format);
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    entry->message = buffer;
    entry->message += " [";
    entry->message += FormatError(error);
    entry->message += "]";
    
    // Write log entry
    _WriteLogEntry(*entry);
    
    // Store in history
    BAutolock lock(fLock);
    fRecentEntries.AddItem(entry);
}

/**
 * @brief Log a debug message
 */
void
ErrorLogger::LogDebug(const char* component, const char* format, ...)
{
#ifdef DEBUG
    if (!fInitialized || kLogDebug < fMinimumLevel) {
        return;
    }
    
    // Format message
    va_list args;
    va_start(args, format);
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    Log(kLogDebug, component, "%s", buffer);
#endif
}

/**
 * @brief Get recent log entries
 */
int32
ErrorLogger::GetRecentEntries(BList& entries, int32 maxCount)
{
    BAutolock lock(fLock);
    
    int32 count = 0;
    int32 startIndex = fRecentEntries.CountItems() - maxCount;
    if (startIndex < 0) startIndex = 0;
    
    for (int32 i = startIndex; i < fRecentEntries.CountItems(); i++) {
        LogEntry* entry = static_cast<LogEntry*>(fRecentEntries.ItemAt(i));
        if (entry) {
            entries.AddItem(new LogEntry(*entry));
            count++;
        }
    }
    
    return count;
}

/**
 * @brief Clear log history
 */
void
ErrorLogger::ClearHistory()
{
    BAutolock lock(fLock);
    
    for (int32 i = 0; i < fRecentEntries.CountItems(); i++) {
        LogEntry* entry = static_cast<LogEntry*>(fRecentEntries.ItemAt(i));
        delete entry;
    }
    fRecentEntries.MakeEmpty();
}

/**
 * @brief Flush any pending log writes
 */
void
ErrorLogger::Flush()
{
    if (fLogFile) {
        fflush(fLogFile);
    }
}

/**
 * @brief Convert log level to string
 */
const char*
ErrorLogger::LevelToString(LogLevel level)
{
    switch (level) {
        case kLogDebug:    return "DEBUG";
        case kLogInfo:     return "INFO";
        case kLogWarning:  return "WARN";
        case kLogError:    return "ERROR";
        case kLogCritical: return "CRIT";
        default:           return "UNKNOWN";
    }
}

/**
 * @brief Format error code to human-readable string
 */
BString
ErrorLogger::FormatError(status_t error)
{
    return strerror(error);
}

/**
 * @brief Write log entry to destinations
 */
void
ErrorLogger::_WriteLogEntry(const LogEntry& entry)
{
    BString formatted = _FormatLogEntry(entry);
    
    if (fLogToConsole) {
        _WriteToConsole(entry.level, formatted);
    }
    
    if (fLogToFile && fLogFile) {
        _WriteToFile(formatted);
    }
}

/**
 * @brief Format log entry as string
 */
BString
ErrorLogger::_FormatLogEntry(const LogEntry& entry)
{
    // Format timestamp
    char timeStr[32];
    struct tm* tm_info = localtime(&entry.timestamp);
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", tm_info);
    
    // Build formatted string
    BString formatted;
    formatted << "[" << timeStr << "] ";
    formatted << "[" << LevelToString(entry.level) << "] ";
    formatted << "[" << entry.component << "] ";
    formatted << entry.message;
    
    return formatted;
}

/**
 * @brief Write to log file
 */
void
ErrorLogger::_WriteToFile(const BString& message)
{
    if (fLogFile) {
        fprintf(fLogFile, "%s\n", message.String());
        fflush(fLogFile);
    }
}

/**
 * @brief Write to console
 */
void
ErrorLogger::_WriteToConsole(LogLevel level, const BString& message)
{
    FILE* output = (level >= kLogError) ? stderr : stdout;
    fprintf(output, "%s\n", message.String());
}

} // namespace OneDrive