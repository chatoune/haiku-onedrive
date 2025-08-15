/**
 * @file ErrorLogger.h
 * @brief Centralized error logging and handling for the Haiku OneDrive project
 * @author Haiku OneDrive Team
 * @version 1.0.0
 * @date 2025-08-15
 * 
 * This class provides consistent error logging and handling across all modules,
 * with support for different log levels and output destinations.
 */

#ifndef ERROR_LOGGER_H
#define ERROR_LOGGER_H

#include <String.h>
#include <Locker.h>
#include <List.h>
#include <OS.h>

#include <stdio.h>

namespace OneDrive {

/**
 * @brief Log levels for message categorization
 */
enum LogLevel {
    kLogDebug = 0,    ///< Debug information
    kLogInfo,         ///< Informational messages
    kLogWarning,      ///< Warning messages
    kLogError,        ///< Error messages
    kLogCritical      ///< Critical errors
};

/**
 * @brief Log entry structure
 */
struct LogEntry {
    time_t timestamp;     ///< When the log was created
    LogLevel level;       ///< Severity level
    BString component;    ///< Component that logged
    BString message;      ///< Log message
    status_t errorCode;   ///< Associated error code (if any)
};

/**
 * @brief Centralized error logging and handling
 * 
 * This singleton class provides consistent error logging across the application,
 * with support for different output destinations and log levels.
 * 
 * @since 1.0.0
 */
class ErrorLogger {
public:
    /**
     * @brief Get the singleton instance
     * 
     * @return Reference to the singleton instance
     */
    static ErrorLogger& Instance();
    
    /**
     * @brief Initialize the error logger
     * 
     * @param logToFile Whether to log to file
     * @param logToConsole Whether to log to console
     * @param logFilePath Path to log file (optional)
     * @return B_OK on success
     */
    status_t Initialize(bool logToFile = true, bool logToConsole = true,
                       const char* logFilePath = NULL);
    
    /**
     * @brief Shutdown the error logger
     */
    void Shutdown();
    
    /**
     * @brief Log a message
     * 
     * @param level Log level
     * @param component Component name
     * @param format Printf-style format string
     * @param ... Variable arguments
     */
    void Log(LogLevel level, const char* component, const char* format, ...);
    
    /**
     * @brief Log an error with status code
     * 
     * @param component Component name
     * @param error Error status code
     * @param format Printf-style format string
     * @param ... Variable arguments
     */
    void LogError(const char* component, status_t error, const char* format, ...);
    
    /**
     * @brief Log a debug message (only in debug builds)
     * 
     * @param component Component name
     * @param format Printf-style format string
     * @param ... Variable arguments
     */
    void LogDebug(const char* component, const char* format, ...);
    
    /**
     * @brief Set minimum log level
     * 
     * @param level Minimum level to log
     */
    void SetMinimumLevel(LogLevel level) { fMinimumLevel = level; }
    
    /**
     * @brief Get minimum log level
     * 
     * @return Current minimum log level
     */
    LogLevel GetMinimumLevel() const { return fMinimumLevel; }
    
    /**
     * @brief Enable/disable file logging
     * 
     * @param enable Whether to enable file logging
     */
    void SetFileLogging(bool enable) { fLogToFile = enable; }
    
    /**
     * @brief Enable/disable console logging
     * 
     * @param enable Whether to enable console logging
     */
    void SetConsoleLogging(bool enable) { fLogToConsole = enable; }
    
    /**
     * @brief Get recent log entries
     * 
     * @param entries Output list of log entries
     * @param maxCount Maximum number of entries to retrieve
     * @return Number of entries retrieved
     */
    int32 GetRecentEntries(BList& entries, int32 maxCount = 100);
    
    /**
     * @brief Clear log history
     */
    void ClearHistory();
    
    /**
     * @brief Flush any pending log writes
     */
    void Flush();
    
    /**
     * @brief Convert log level to string
     * 
     * @param level Log level
     * @return String representation
     */
    static const char* LevelToString(LogLevel level);
    
    /**
     * @brief Format error code to human-readable string
     * 
     * @param error Error code
     * @return Error description
     */
    static BString FormatError(status_t error);

private:
    /**
     * @brief Private constructor (singleton)
     */
    ErrorLogger();
    
    /**
     * @brief Destructor
     */
    ~ErrorLogger();
    
    /**
     * @brief Write log entry to destinations
     * 
     * @param entry Log entry to write
     */
    void _WriteLogEntry(const LogEntry& entry);
    
    /**
     * @brief Format log entry as string
     * 
     * @param entry Log entry
     * @return Formatted string
     */
    BString _FormatLogEntry(const LogEntry& entry);
    
    /**
     * @brief Write to log file
     * 
     * @param message Formatted message
     */
    void _WriteToFile(const BString& message);
    
    /**
     * @brief Write to console
     * 
     * @param level Log level
     * @param message Formatted message
     */
    void _WriteToConsole(LogLevel level, const BString& message);

private:
    static ErrorLogger* sInstance;    ///< Singleton instance
    
    bool fInitialized;               ///< Initialization flag
    bool fLogToFile;                 ///< Log to file flag
    bool fLogToConsole;              ///< Log to console flag
    LogLevel fMinimumLevel;          ///< Minimum log level
    
    BString fLogFilePath;            ///< Path to log file
    FILE* fLogFile;                  ///< Log file handle
    
    BList fRecentEntries;            ///< Recent log entries
    int32 fMaxHistorySize;           ///< Maximum history size
    
    mutable BLocker fLock;           ///< Thread safety lock
    
    // Prevent copying
    ErrorLogger(const ErrorLogger&);
    ErrorLogger& operator=(const ErrorLogger&);
};

/**
 * @brief Convenience macros for logging
 */
#define LOG_ERROR(component, format, ...) \
    ErrorLogger::Instance().Log(kLogError, component, format, ##__VA_ARGS__)

#define LOG_WARNING(component, format, ...) \
    ErrorLogger::Instance().Log(kLogWarning, component, format, ##__VA_ARGS__)

#define LOG_INFO(component, format, ...) \
    ErrorLogger::Instance().Log(kLogInfo, component, format, ##__VA_ARGS__)

#ifdef DEBUG
#define LOG_DEBUG(component, format, ...) \
    ErrorLogger::Instance().LogDebug(component, format, ##__VA_ARGS__)
#else
#define LOG_DEBUG(component, format, ...) ((void)0)
#endif

} // namespace OneDrive

#endif // ERROR_LOGGER_H