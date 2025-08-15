/**
 * @file CacheManager.h
 * @brief Local cache management for OneDrive files
 * @author Haiku OneDrive Team
 * @version 1.0.0
 * @date 2025-08-15
 * 
 * This class manages the local cache of OneDrive files, including
 * metadata storage, file content caching, and eviction policies.
 */

#ifndef CACHE_MANAGER_H
#define CACHE_MANAGER_H

#include <Entry.h>
#include <List.h>
#include <Locker.h>
#include <Message.h>
#include <Path.h>
#include <String.h>
#include <StringList.h>

#include <map>
#include <sqlite3.h>

namespace OneDrive {

/**
 * @brief Cache entry information
 */
struct CacheEntry {
    BString fileId;             ///< OneDrive file ID
    BPath localPath;            ///< Local cache path
    BPath originalPath;         ///< Original file path
    off_t size;                 ///< File size
    time_t lastAccess;          ///< Last access time
    time_t lastModified;        ///< Last modification time
    time_t cacheTime;           ///< When cached
    bool isPinned;              ///< Pinned for offline access
    BString eTag;               ///< OneDrive ETag
    BString checksum;           ///< File checksum
    int32 accessCount;          ///< Access frequency
};

/**
 * @brief Cache eviction policies
 */
enum EvictionPolicy {
    kEvictionLRU = 0,           ///< Least Recently Used
    kEvictionLFU,               ///< Least Frequently Used
    kEvictionFIFO,              ///< First In First Out
    kEvictionSize               ///< Largest files first
};

/**
 * @brief Cache statistics
 */
struct CacheStats {
    off_t totalSize;            ///< Total cache size
    off_t maxSize;              ///< Maximum allowed size
    int32 fileCount;            ///< Number of cached files
    int32 pinnedCount;          ///< Number of pinned files
    off_t pinnedSize;           ///< Size of pinned files
    int32 hitCount;             ///< Cache hits
    int32 missCount;            ///< Cache misses
    float hitRate;              ///< Hit rate percentage
};

/**
 * @brief Manages local caching of OneDrive files
 * 
 * This class provides efficient local caching of OneDrive files with
 * configurable eviction policies, metadata storage, and performance
 * optimization through intelligent prefetching.
 * 
 * @see OneDriveSyncEngine
 * @since 1.0.0
 */
class CacheManager {
public:
    /**
     * @brief Constructor
     * 
     * @param cachePath Base path for cache storage
     */
    CacheManager(const BPath& cachePath);
    
    /**
     * @brief Destructor
     */
    virtual ~CacheManager();
    
    /**
     * @brief Initialize the cache manager
     * 
     * Creates cache directories and opens metadata database.
     * 
     * @return B_OK on success, error code otherwise
     */
    status_t Initialize();
    
    /**
     * @brief Shutdown the cache manager
     * 
     * Closes database and flushes pending operations.
     */
    void Shutdown();
    
    /**
     * @brief Cache a file
     * 
     * @param fileId OneDrive file ID
     * @param sourcePath Path to file to cache
     * @param originalPath Original file path for reference
     * @param isPinned Whether to pin for offline access
     * @return B_OK on success, error code otherwise
     */
    status_t CacheFile(const BString& fileId, const BPath& sourcePath,
                      const BPath& originalPath, bool isPinned = false);
    
    /**
     * @brief Get cached file path
     * 
     * @param fileId OneDrive file ID
     * @return Path to cached file, or empty path if not cached
     */
    BPath GetCachedFilePath(const BString& fileId);
    
    /**
     * @brief Check if file is cached
     * 
     * @param fileId OneDrive file ID
     * @return true if file is in cache
     */
    bool IsFileCached(const BString& fileId);
    
    /**
     * @brief Evict file from cache
     * 
     * @param fileId OneDrive file ID
     * @return B_OK on success, error code otherwise
     */
    status_t EvictFile(const BString& fileId);
    
    /**
     * @brief Update file access time
     * 
     * @param fileId OneDrive file ID
     */
    void TouchFile(const BString& fileId);
    
    /**
     * @brief Pin file for offline access
     * 
     * @param fileId OneDrive file ID
     * @return B_OK on success, error code otherwise
     */
    status_t PinFile(const BString& fileId);
    
    /**
     * @brief Unpin file from offline access
     * 
     * @param fileId OneDrive file ID
     * @return B_OK on success, error code otherwise
     */
    status_t UnpinFile(const BString& fileId);
    
    /**
     * @brief Pin entire folder for offline access
     * 
     * Recursively pins all files in the folder and marks
     * the folder itself as always offline.
     * 
     * @param folderPath Path to folder to pin
     * @return B_OK on success, error code otherwise
     */
    status_t PinFolder(const BPath& folderPath);
    
    /**
     * @brief Unpin entire folder from offline access
     * 
     * @param folderPath Path to folder to unpin
     * @return B_OK on success, error code otherwise
     */
    status_t UnpinFolder(const BPath& folderPath);
    
    /**
     * @brief Check if folder is pinned
     * 
     * @param folderPath Path to check
     * @return true if folder is pinned for offline access
     */
    bool IsFolderPinned(const BPath& folderPath) const;
    
    /**
     * @brief Get list of pinned folders
     * 
     * @param folders Output list of pinned folder paths
     * @return B_OK on success
     */
    status_t GetPinnedFolders(BStringList& folders) const;
    
    /**
     * @brief Set maximum cache size
     * 
     * @param maxSize Maximum size in bytes
     */
    void SetMaxCacheSize(off_t maxSize);
    
    /**
     * @brief Get maximum cache size
     * 
     * @return Maximum cache size in bytes
     */
    off_t GetMaxCacheSize() const { return fMaxCacheSize; }
    
    /**
     * @brief Set eviction policy
     * 
     * @param policy Eviction policy to use
     */
    void SetEvictionPolicy(EvictionPolicy policy);
    
    /**
     * @brief Get current eviction policy
     * 
     * @return Current eviction policy
     */
    EvictionPolicy GetEvictionPolicy() const { return fEvictionPolicy; }
    
    /**
     * @brief Clean up cache based on policy
     * 
     * Removes files according to eviction policy to free space.
     * 
     * @param targetSize Target size to free (0 = auto)
     * @return Bytes freed
     */
    off_t CleanupCache(off_t targetSize = 0);
    
    /**
     * @brief Get cache statistics
     * 
     * @return Current cache statistics
     */
    CacheStats GetCacheStats() const;
    
    /**
     * @brief Clear entire cache
     * 
     * @param keepPinned Whether to keep pinned files
     * @return B_OK on success
     */
    status_t ClearCache(bool keepPinned = true);
    
    /**
     * @brief Verify cache integrity
     * 
     * @param repair Whether to repair issues found
     * @return Number of issues found
     */
    int32 VerifyCache(bool repair = false);
    
    /**
     * @brief Get cache entry information
     * 
     * @param fileId OneDrive file ID
     * @param entry Output cache entry
     * @return B_OK if found, B_ENTRY_NOT_FOUND otherwise
     */
    status_t GetCacheEntry(const BString& fileId, CacheEntry& entry);
    
    /**
     * @brief Update cache entry metadata
     * 
     * @param fileId OneDrive file ID
     * @param eTag New ETag value
     * @param checksum New checksum
     * @return B_OK on success
     */
    status_t UpdateCacheEntry(const BString& fileId, const BString& eTag,
                             const BString& checksum);
    
    /**
     * @brief Prefetch files for performance
     * 
     * @param fileIds List of file IDs to prefetch
     * @param priority Priority for prefetch operation
     */
    void PrefetchFiles(const BList& fileIds, int32 priority = 5);
    
    /**
     * @brief Export cache metadata
     * 
     * @param outputPath Path for export file
     * @return B_OK on success
     */
    status_t ExportMetadata(const BPath& outputPath);
    
    /**
     * @brief Import cache metadata
     * 
     * @param inputPath Path to import file
     * @return B_OK on success
     */
    status_t ImportMetadata(const BPath& inputPath);

private:
    /**
     * @brief Initialize database
     * 
     * @return B_OK on success
     */
    status_t _InitializeDatabase();
    
    /**
     * @brief Create cache tables
     * 
     * @return B_OK on success
     */
    status_t _CreateTables();
    
    /**
     * @brief Add entry to database
     * 
     * @param entry Cache entry to add
     * @return B_OK on success
     */
    status_t _AddToDatabase(const CacheEntry& entry);
    
    /**
     * @brief Remove entry from database
     * 
     * @param fileId File ID to remove
     * @return B_OK on success
     */
    status_t _RemoveFromDatabase(const BString& fileId);
    
    /**
     * @brief Update entry in database
     * 
     * @param entry Cache entry to update
     * @return B_OK on success
     */
    status_t _UpdateDatabase(const CacheEntry& entry);
    
    /**
     * @brief Load cache entries from database
     * 
     * @return B_OK on success
     */
    status_t _LoadCacheEntries();
    
    /**
     * @brief Calculate cache size
     * 
     * @return Total size of cached files
     */
    off_t _CalculateCacheSize();
    
    /**
     * @brief Get eviction candidates
     * 
     * @param targetSize Size to free
     * @param candidates Output list of candidates
     * @return Number of candidates
     */
    int32 _GetEvictionCandidates(off_t targetSize, BList& candidates);
    
    /**
     * @brief Copy file to cache
     * 
     * @param source Source file path
     * @param destination Cache file path
     * @return B_OK on success
     */
    status_t _CopyToCache(const BPath& source, const BPath& destination);
    
    /**
     * @brief Delete cached file
     * 
     * @param cachePath Path to cached file
     * @return B_OK on success
     */
    status_t _DeleteCachedFile(const BPath& cachePath);
    
    /**
     * @brief Generate cache path for file
     * 
     * @param fileId OneDrive file ID
     * @return Generated cache path
     */
    BPath _GenerateCachePath(const BString& fileId);
    
    /**
     * @brief Calculate file checksum
     * 
     * @param path File path
     * @return Checksum string
     */
    BString _CalculateChecksum(const BPath& path);

private:
    BPath fCachePath;                       ///< Base cache directory
    sqlite3* fDatabase;                     ///< Metadata database
    std::map<BString, CacheEntry> fEntries; ///< Cache entries map
    mutable BLocker fLock;                  ///< Thread safety lock
    
    off_t fMaxCacheSize;                    ///< Maximum cache size
    off_t fCurrentCacheSize;                ///< Current cache size
    EvictionPolicy fEvictionPolicy;         ///< Current eviction policy
    
    bool fInitialized;                      ///< Initialization flag
    int32 fHitCount;                        ///< Cache hit counter
    int32 fMissCount;                       ///< Cache miss counter
    
    BStringList fPinnedFolders;             ///< List of pinned folder paths
};

} // namespace OneDrive

#endif // CACHE_MANAGER_H