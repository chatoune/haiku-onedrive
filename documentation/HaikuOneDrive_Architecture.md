# Haiku OneDrive - Architecture & Specification

## Project Overview

A native OneDrive client for Haiku OS that provides seamless filesystem integration, complete with file attribute synchronization, caching, and system-level integration similar to the Windows OneDrive experience.

## Core Requirements

- **Native Filesystem Integration**: OneDrive appears as a virtual folder with real-time sync status
- **Haiku File Attributes Sync**: BFS extended attributes synchronized across all Haiku instances
- **System Integration**: Background daemon, preferences panel, custom icons, context menus
- **Offline Support**: Local caching with intelligent prefetching and LRU eviction
- **OAuth2 Authentication**: Microsoft Graph API integration with secure token management

---

## System Architecture

### High-Level Component Overview

```
┌─────────────────────────────────────────────────────────────────────┐
│                          USER INTERFACE                            │
├─────────────────────┬───────────────────┬───────────────────────────┤
│  OneDrivePrefs      │    Tracker        │    Virtual OneDrive       │
│  (Configuration)    │   Integration     │       Folder              │
└─────────────────────┼───────────────────┼───────────────────────────┘
                      │                   │
├─────────────────────┼───────────────────┼───────────────────────────┤
│                     SYSTEM INTEGRATION                             │
├─────────────────────┼───────────────────┼───────────────────────────┤
│   Node Monitoring   │   Custom Icons    │    Context Menus          │
└─────────────────────┼───────────────────┼───────────────────────────┘
                      │                   │
├─────────────────────┼───────────────────┼───────────────────────────┤
│                    CORE SERVICES                                   │
├─────────────────────┼───────────────────┼───────────────────────────┤
│  OneDrive Daemon    │   Sync Engine     │   Attribute Manager       │
│  (Background)       │   (Bi-directional)│   (BFS ↔ OneDrive)       │
└─────────────────────┼───────────────────┼───────────────────────────┘
                      │                   │
├─────────────────────┼───────────────────┼───────────────────────────┤
│                      DATA LAYER                                    │
├─────────────────────┼───────────────────┼───────────────────────────┤
│   Local Cache       │   Metadata DB     │     OneDrive API          │
│   (Files + Attrs)   │   (SQLite)        │   (Microsoft Graph)       │
└─────────────────────┴───────────────────┴───────────────────────────┘
```

---

## Component Specifications

### 1. OneDrive Daemon (`onedrive_daemon`)

**Purpose**: Background service managing all OneDrive operations

**Location**: `/system/servers/onedrive_daemon`

**Launch Configuration**: `/system/data/launch/onedrive_daemon`

```json
{
    "service": true,
    "launch": "on_demand",
    "conditions": {
        "deskbar_running": true,
        "network_available": true
    },
    "env": {
        "HOME": "%h"
    }
}
```

**Core Responsibilities**:
- OAuth2 token management and refresh
- Bidirectional file synchronization
- Haiku attribute synchronization
- Local cache management
- File conflict resolution
- Network change detection

**Key Classes**:
```cpp
class OneDriveDaemon : public BApplication {
public:
    OneDriveDaemon();
    void MessageReceived(BMessage* message) override;
    void ReadyToRun() override;
    
private:
    OneDriveSyncEngine*     fSyncEngine;
    AttributeManager*       fAttributeManager;
    CacheManager*          fCacheManager;
    AuthenticationManager* fAuthManager;
    NetworkMonitor*        fNetworkMonitor;
};
```

### 2. Configuration Application (`OneDrivePrefs`)

**Purpose**: User interface for OneDrive settings and account management

**Location**: `/system/preferences/OneDrive`

**Features**:
- Account authentication (OAuth2 flow)
- Sync folder selection and exclusions
- Bandwidth and sync interval settings
- File type filters
- Conflict resolution preferences
- Cache size management
- Offline availability settings

**User Interface**:
```cpp
class OneDrivePrefsWindow : public BWindow {
public:
    OneDrivePrefsWindow();
    void MessageReceived(BMessage* message) override;
    
private:
    BTabView*           fTabView;
    AccountTab*         fAccountTab;
    SyncTab*           fSyncTab;
    AdvancedTab*       fAdvancedTab;
    
    void AuthenticateAccount();
    void SelectSyncFolders();
    void ApplySettings();
};
```

### 3. Virtual OneDrive Filesystem

**Purpose**: Present OneDrive as a native filesystem folder

**Location**: User-configurable (default: `~/OneDrive/`)

**Implementation**: BFS add-on or FUSE-style virtual folder

**Features**:
- Real-time sync status via custom HVIF icons
- Lazy loading of file content
- Attribute preservation and sync
- Context menu integration
- Drag-and-drop support

**File Status Icons**:
- 🟢 **Synced**: Green checkmark overlay
- ☁️ **Cloud-only**: Cloud icon overlay  
- ↕️ **Syncing**: Bidirectional arrow overlay
- ⚠️ **Conflict**: Warning triangle overlay
- ❌ **Error**: Red X overlay
- 📌 **Pinned**: Pin icon for always-available files

### 4. Sync Engine

**Purpose**: Core synchronization logic between local files and OneDrive

```cpp
class OneDriveSyncEngine {
public:
    status_t StartSync();
    status_t PauseSync();
    status_t SyncFile(const BPath& localPath, const BString& remoteId);
    status_t SyncDirectory(const BPath& localPath, const BString& remoteId);
    
    // Conflict resolution
    status_t ResolveConflict(const BString& fileId, ConflictResolution resolution);
    
    // Event handling
    void HandleLocalChange(const BPath& path, uint32 changeFlags);
    void HandleRemoteChange(const BString& fileId, const FileMetadata& metadata);
    
private:
    AttributeManager*   fAttributeManager;
    CacheManager*      fCacheManager;
    OneDriveAPI*       fAPIClient;
    ConflictResolver*  fConflictResolver;
};
```

### 5. Attribute Management System

**Purpose**: Synchronize Haiku BFS extended attributes with OneDrive metadata

```cpp
class AttributeManager {
public:
    // Local attribute operations
    status_t ReadAttributes(const BPath& path, AttributeMap& attributes);
    status_t WriteAttributes(const BPath& path, const AttributeMap& attributes);
    void MonitorAttributes(const BPath& path);
    
    // Remote attribute operations  
    status_t UploadAttributes(const BString& fileId, const AttributeMap& attributes);
    status_t DownloadAttributes(const BString& fileId, AttributeMap& attributes);
    
    // Synchronization
    status_t SyncAttributes(const BPath& localPath, const BString& remoteId);
    bool AttributesNeedSync(const BPath& localPath, const BString& remoteId);
    
private:
    OneDriveMetadataClient* fMetadataClient;
    AttributeCache*        fAttributeCache;
};
```

**Attribute Storage Strategies**:

1. **OneDrive for Business**: SharePoint list item custom fields
2. **OneDrive Personal**: Custom facets (requires registration)
3. **Universal Fallback**: Hidden `.haiku_attrs` sidecar files

**Attribute Serialization Format**:
```json
{
    "haiku_attributes": {
        "version": "1.0",
        "timestamp": "2025-08-14T12:30:45Z",
        "checksum": "sha256:abc123...",
        "attributes": [
            {
                "name": "BEOS:TYPE",
                "type": "string", 
                "size": 28,
                "data": "application/x-vnd.Be-bookmark"
            },
            {
                "name": "META:url",
                "type": "string",
                "size": 19, 
                "data": "https://example.com"
            },
            {
                "name": "custom:rating",
                "type": "int32",
                "size": 4,
                "data": 5
            }
        ]
    }
}
```

### 6. Caching System

**Purpose**: Local storage for offline access and performance optimization

**Cache Structure**:
```
~/config/cache/onedrive/
├── config.json          # Cache configuration
├── files/               # Cached file content
│   ├── content/         # File data (hash-named)
│   └── thumbnails/      # Image thumbnails
├── metadata/            # File metadata cache
│   ├── files.db         # SQLite database
│   └── attributes/      # Serialized attribute data
└── sync/                # Sync state tracking
    ├── local_changes.db
    └── conflict_log.db
```

**Cache Management**:
```cpp
class CacheManager {
public:
    // File caching
    status_t CacheFile(const BString& fileId, const BPath& sourcePath);
    BPath GetCachedFilePath(const BString& fileId);
    bool IsFileCached(const BString& fileId);
    status_t EvictFile(const BString& fileId);
    
    // Cache policy
    void SetCacheSize(off_t maxSize);
    void SetRetentionPolicy(RetentionPolicy policy);
    void CleanupCache();
    
    // Statistics
    CacheStats GetCacheStats();
    
private:
    SQLiteDatabase* fMetadataDB;
    LRUEvictionPolicy* fEvictionPolicy;
    off_t fMaxCacheSize;
    off_t fCurrentCacheSize;
};
```

### 7. Authentication Manager

**Purpose**: OAuth2 flow management and token storage

```cpp
class AuthenticationManager {
public:
    status_t Authenticate();
    status_t RefreshToken();
    bool IsAuthenticated();
    BString GetAccessToken();
    status_t Logout();
    
    // OAuth2 flow
    status_t StartAuthFlow();
    status_t HandleAuthCallback(const BString& authCode);
    
private:
    TokenStorage* fTokenStorage;
    BString fClientId;
    BString fAccessToken;
    BString fRefreshToken;
    time_t fTokenExpiry;
};
```

**OAuth2 Configuration**:
- **Authorization Endpoint**: `https://login.microsoftonline.com/common/oauth2/v2.0/authorize`
- **Token Endpoint**: `https://login.microsoftonline.com/common/oauth2/v2.0/token`
- **Redirect URI**: `https://login.live.com/oauth20_desktop.srf`
- **Scopes**: `Files.ReadWrite.All`, `offline_access`

### 8. OneDrive API Client

**Purpose**: Microsoft Graph API integration

```cpp
class OneDriveAPI {
public:
    // File operations
    status_t UploadFile(const BPath& localPath, const BString& remotePath);
    status_t DownloadFile(const BString& fileId, const BPath& localPath);
    status_t DeleteFile(const BString& fileId);
    status_t MoveFile(const BString& fileId, const BString& newParentId);
    
    // Metadata operations
    FileMetadata GetFileMetadata(const BString& fileId);
    status_t UpdateFileMetadata(const BString& fileId, const FileMetadata& metadata);
    
    // Delta sync
    DeltaResponse GetDelta(const BString& deltaToken = "");
    
    // Custom metadata (attributes)
    status_t SetCustomProperties(const BString& fileId, const BString& properties);
    BString GetCustomProperties(const BString& fileId);
    
private:
    BHttpSession* fHttpSession;
    AuthenticationManager* fAuthManager;
    BString fBaseURL;
};
```

---

## Data Flow Architecture

### File Upload Flow
```
1. User adds file to OneDrive folder
2. Node monitoring detects file creation
3. Sync engine queues upload task
4. Read local file attributes
5. Upload file content to OneDrive
6. Serialize and upload attributes as metadata
7. Update local cache and sync state
8. Update file icon to "synced" status
```

### File Download Flow  
```
1. Remote change detected via delta sync
2. Download file metadata first
3. Check if file should be cached locally
4. Download file content if needed
5. Download and deserialize attributes
6. Apply attributes to local file
7. Update cache database
8. Notify Tracker of changes
```

### Attribute Sync Flow
```
1. Attribute change detected via node monitoring
2. Serialize changed attributes to JSON
3. Upload to OneDrive as custom metadata
4. Update attribute cache
5. Broadcast change to other Haiku instances
6. Handle conflicts if simultaneous changes
```

---

## File Organization

### Source Code Structure
```
src/
├── daemon/                 # OneDrive daemon
│   ├── OneDriveDaemon.cpp
│   ├── SyncEngine.cpp
│   ├── AttributeManager.cpp
│   └── CacheManager.cpp
├── preferences/           # Configuration application
│   ├── OneDrivePrefs.cpp
│   ├── AccountTab.cpp
│   ├── SyncTab.cpp  
│   └── AdvancedTab.cpp
├── filesystem/           # Virtual filesystem integration
│   ├── OneDriveFilesystem.cpp
│   └── VirtualFolder.cpp
├── api/                  # OneDrive API client
│   ├── OneDriveAPI.cpp
│   ├── AuthManager.cpp
│   └── HTTPClient.cpp
├── shared/               # Shared libraries
│   ├── AttributeUtils.cpp
│   ├── CacheUtils.cpp
│   └── OneDriveConstants.h
└── tests/                # Unit tests
    ├── SyncEngineTest.cpp
    ├── AttributeTest.cpp
    └── CacheTest.cpp
```

### Build System (CMake)
```cmake
cmake_minimum_required(VERSION 3.16)
project(HaikuOneDrive)

set(CMAKE_CXX_STANDARD 17)

# Haiku-specific libraries
find_library(BE_LIB be)
find_library(NETWORK_LIB network)  
find_library(STORAGE_LIB tracker)

# Third-party dependencies
find_package(OpenSSL REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(JSON REQUIRED)

# Build targets
add_executable(onedrive_daemon src/daemon/OneDriveDaemon.cpp ...)
add_executable(OneDrivePrefs src/preferences/OneDrivePrefs.cpp ...)
add_library(OneDriveAPI SHARED src/api/OneDriveAPI.cpp ...)
```

---

## Installation & Deployment

### Package Structure
```
packages/onedrive-1.0.0-x86_64.hpkg
├── system/
│   ├── servers/onedrive_daemon
│   ├── preferences/OneDrive  
│   ├── data/launch/onedrive_daemon
│   └── lib/libOneDriveAPI.so
├── documentation/
│   ├── ReadMe.html
│   └── UserGuide.html
└── .PackageInfo
```

### Installation Process
1. Install package via HaikuDepot or pkgman
2. Launch OneDrive preferences to authenticate
3. Select sync folder location
4. Daemon automatically starts on next boot
5. OneDrive folder appears in selected location

### System Requirements
- Haiku R1/beta4 or later
- 100MB free disk space (minimum)
- Active internet connection
- Microsoft account with OneDrive access

---

## Security Considerations

### Authentication Security
- OAuth2 tokens stored in secure settings files (`~/config/settings/OneDrive/`)  
- Automatic token refresh prevents long-lived credentials
- No password storage - uses secure token exchange only

### Data Security
- All API communications over HTTPS/TLS
- Local cache files protected by filesystem permissions
- Attribute data encrypted before OneDrive storage (optional)

### Privacy
- Only syncs explicitly selected folders
- File content scanning opt-out available
- Local activity logging can be disabled

---

## Performance Specifications

### Sync Performance
- **Delta Sync**: Only changed files synchronized
- **Parallel Operations**: Up to 4 concurrent uploads/downloads
- **Bandwidth Throttling**: User-configurable limits
- **Background Priority**: Low-priority sync to avoid UI blocking

### Cache Performance  
- **SQLite Database**: Fast metadata lookups
- **LRU Eviction**: Intelligent cache management
- **Prefetch Strategy**: Download frequently accessed files
- **Thumbnail Cache**: Quick file previews

### Memory Usage
- **Daemon**: ~50MB base memory usage
- **Cache Index**: ~1MB per 10,000 files
- **Preferences**: ~20MB when running

---

## Future Enhancements

### Phase 2 Features
- Real-time collaborative editing indicators
- OneDrive sharing integration with Haiku
- Backup and versioning support
- Advanced conflict resolution UI
- Mobile device sync status

### Phase 3 Features  
- OneDrive for Business team sites
- SharePoint document library integration
- Advanced query and search capabilities
- Haiku-specific workflow automation
- Third-party app integration APIs

---

## Development Roadmap

### Milestone 1: Core Foundation (8 weeks)
- [ ] Basic daemon architecture
- [ ] OAuth2 authentication 
- [ ] Simple file sync (no attributes)
- [ ] Preferences application UI

### Milestone 2: Attribute Sync (6 weeks)
- [ ] BFS attribute reading/writing
- [ ] Attribute serialization format
- [ ] OneDrive metadata integration
- [ ] Conflict resolution logic

### Milestone 3: System Integration (4 weeks)  
- [ ] Virtual filesystem implementation
- [ ] Custom icon system
- [ ] Tracker context menus
- [ ] Node monitoring integration

### Milestone 4: Caching & Performance (6 weeks)
- [ ] Local cache implementation
- [ ] LRU eviction policy
- [ ] Delta sync optimization
- [ ] Background sync engine

### Milestone 5: Polish & Testing (4 weeks)
- [ ] Comprehensive testing suite
- [ ] Error handling and recovery
- [ ] Documentation and user guides
- [ ] Package creation and distribution

**Total Development Time**: ~28 weeks (7 months)

---

This specification provides a comprehensive blueprint for creating a native, fully-integrated OneDrive client for Haiku OS that preserves the unique characteristics of the BeOS/Haiku experience while providing modern cloud storage capabilities.