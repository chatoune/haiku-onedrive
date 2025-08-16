# Haiku OneDrive Client - Architecture Documentation

## Document Purpose
This document captures all architectural decisions, design patterns, and technical choices made during the development of the Haiku OneDrive client. It serves as the single source of truth for system design.

## Architecture Decision Records

### ADR-001: Documentation-First Development
**Status**: Accepted  
**Date**: 2025-08-16

**Context**: Starting fresh development requires clear documentation and state management to ensure continuity across development sessions.

**Decision**: Implement strict documentation rules requiring updates after every development step, with both human-readable and machine-readable state tracking.

**Consequences**:
- Positive: Project can be continued by any developer at any time
- Positive: Clear audit trail of all decisions
- Negative: Slightly slower initial development
- Negative: More files to maintain

**Alternatives Considered**:
- Code-first with documentation later: Rejected due to risk of knowledge loss
- Minimal documentation: Rejected as insufficient for complex project

---

### ADR-002: Haiku-Native Development Approach
**Status**: Accepted  
**Date**: 2025-08-16

**Context**: This project must be a true Haiku OS native application that integrates seamlessly with the system and follows Haiku design principles.

**Decision**: Use Haiku native APIs exclusively wherever possible, only falling back to external libraries when no native alternative exists.

**Consequences**:
- Positive: Perfect system integration and native look/feel
- Positive: Smaller binary size and fewer dependencies
- Positive: Better performance through native APIs
- Positive: Consistent with other Haiku applications
- Negative: May require more research into Haiku-specific APIs
- Negative: Less portable to other platforms

**Alternatives Considered**:
- Cross-platform approach: Rejected to ensure native quality
- Qt-based UI: Rejected in favor of native BWindow/BView
- Generic C++ with POSIX: Rejected to leverage Haiku features

---

### ADR-003: Technology Stack
**Status**: Accepted  
**Date**: 2025-08-16

**Context**: Based on ADR-002's native-first approach, we need to select specific technologies.

**Decision**: 
1. **Programming Language**: C++ (Haiku native)
2. **Build System**: CMake (with Haiku-specific configuration)
3. **UI Framework**: Native Haiku Interface Kit (BWindow, BView, etc.)
4. **Threading**: BLooper/BHandler/BMessage system
5. **Networking**: BNetworkKit with BHttpRequest
6. **Storage**: BFile, BDirectory, extended attributes
7. **IPC**: BMessage-based messaging
8. **JSON**: Haiku's native JSON support if available, minimal custom parser if needed
9. **Testing**: Native Haiku testing patterns

**Consequences**:
- All components will use native Haiku patterns
- Maximum integration with system services
- Minimal external dependencies

---

## System Architecture

> **Note**: For better formatted diagrams optimized for monospace fonts, see [ARCHITECTURE_DIAGRAMS.md](ARCHITECTURE_DIAGRAMS.md)

### High-Level Architecture

The system follows a three-layer architecture:

1. **User Interface Layer**
   - Preferences Application (standalone BApplication)
   - Tracker Add-on (OneDrive.so shared library)
   - Status Monitor (Deskbar replicant)

2. **Core Service Layer**
   - OneDrive Daemon (central BApplication service)
   - Service Components (Authentication, Sync Engine, Cache Manager)
   - API Client with Connection Pool

3. **Storage & Persistence Layer**
   - BKeyStore for secure token storage
   - File System for synced content
   - SQLite database for metadata

### Detailed Component Design

#### 1. OneDrive Daemon (onedrive_daemon)
**Type**: BApplication-based system service  
**Responsibilities**:
- Central coordination of all OneDrive operations
- Message-based IPC with UI components
- Service lifecycle management
- System integration point

**Key Classes**:
- `OneDriveDaemon : public BApplication`
- `MessageHandler : public BHandler`
- `ServiceManager` - Manages service components
- `IPCProtocol` - Defines message constants

**Message Protocol**:
```cpp
// Application signature
#define ONEDRIVE_SIGNATURE "application/x-vnd.Haiku-OneDrive"

// Message commands
enum {
    MSG_AUTHENTICATE = 'auth',
    MSG_SYNC_NOW = 'sync',
    MSG_PAUSE_SYNC = 'paus',
    MSG_RESUME_SYNC = 'resm',
    MSG_GET_STATUS = 'stat',
    MSG_OPEN_PREFS = 'pref',
    MSG_QUIT_DAEMON = 'quit'
};
```

#### 2. Authentication Service
**Type**: Service component within daemon  
**Responsibilities**:
- OAuth2 flow management
- Token storage in BKeyStore
- Automatic token refresh
- Multi-account support

**Key Classes**:
- `AuthenticationService : public BLooper`
- `OAuthHandler` - Manages OAuth2 flow
- `TokenManager` - Token storage/refresh
- `AccountManager` - Multi-account handling

**Security Design**:
- Tokens stored in system BKeyStore
- No plaintext credential storage
- Automatic token refresh before expiry
- Secure browser-based authentication

#### 3. Sync Engine
**Type**: BLooper-based asynchronous service  
**Responsibilities**:
- Bidirectional file synchronization
- Conflict detection and resolution
- File monitoring via BNodeMonitor
- Sync queue management

**Key Classes**:
- `SyncEngine : public BLooper`
- `FileMonitor` - Local change detection
- `RemoteMonitor` - Cloud change polling
- `ConflictResolver` - Conflict handling
- `SyncQueue` - Operation queuing

**Sync Algorithm**:
1. Monitor local changes via BNodeMonitor
2. Poll remote changes periodically
3. Build sync operation queue
4. Execute operations with conflict checking
5. Update local metadata cache
6. Notify UI of status changes

#### 4. Cache Manager
**Type**: Singleton service  
**Responsibilities**:
- Local file content caching
- Metadata database management
- Space management and eviction
- Offline file handling

**Storage Structure**:
```
~/config/settings/OneDrive/
├── cache.db          # SQLite metadata
├── cache/            # File content
│   └── [file_id]/    # Cached files
├── settings          # App preferences
└── sync/             # Sync folder
```

**Key Classes**:
- `CacheManager` - Main cache interface
- `MetadataDB` - SQLite wrapper
- `FileCache` - Content management
- `SpaceManager` - Eviction policies

#### 5. OneDrive API Client with Connection Pool
**Type**: Shared component with parallel connection support  
**Responsibilities**:
- Microsoft Graph API communication
- Connection pool management (multiple concurrent connections)
- Request queuing and intelligent dispatching
- Error handling and retry logic
- Response parsing and aggregation

**Key Classes**:
- `OneDriveClient` - Main API interface
- `ConnectionPool` - Manages multiple BHttpSession instances
- `GraphRequest : public BHttpRequest`
- `RequestDispatcher` - Distributes requests across connections
- `ResponseParser` - JSON handling

**Connection Pool Design**:
```cpp
class ConnectionPool {
    static const int MAX_CONNECTIONS = 4;  // Configurable
    static const int MAX_CONCURRENT_UPLOADS = 3;
    static const int MAX_CONCURRENT_DOWNLOADS = 4;
    
    BObjectList<BHttpSession> fSessions;
    BLocker fLock;
    // Load balancing across connections
};
```

**API Endpoints**:
- `/me/drive` - Drive information
- `/me/drive/root/children` - List files
- `/me/drive/items/{id}` - File operations
- `/me/drive/items/{id}/content` - File content
- `/me/drive/items/{id}/createUploadSession` - Large file uploads

#### 6. Preferences Application
**Type**: Standalone BApplication  
**Responsibilities**:
- User configuration interface
- Account management
- Sync settings
- Daemon communication

**Key Windows**:
- `PreferencesWindow : public BWindow`
- `AccountView : public BView`
- `SyncSettingsView : public BView`
- `AdvancedView : public BView`

#### 7. Tracker Add-on
**Type**: Shared library (OneDrive.so)  
**Responsibilities**:
- Context menu integration
- File status overlays
- Drag-drop handling
- Property extensions

**Implementation**:
- `process_refs()` - Handle file operations
- `AddOnMenuGen()` - Generate context menus
- Custom attributes for sync status
- Icon overlay composition

#### 8. Status Monitor (Deskbar)
**Type**: BDeskbar replicant  
**Responsibilities**:
- System tray presence
- Quick status display
- Sync control menu
- Notifications

**Key Classes**:
- `StatusReplicant : public BView`
- `StatusMenu : public BPopUpMenu`
- `NotificationManager`

### Data Flow Architecture

See [ARCHITECTURE_DIAGRAMS.md](ARCHITECTURE_DIAGRAMS.md) for detailed data flow diagrams including:
- Authentication flow
- File synchronization with parallel connections
- Status update flow

### Inter-Process Communication Design

#### BMessage Protocol Specification
```cpp
// Status Messages
enum {
    MSG_STATUS_UPDATE = 'stup',    // Daemon → UI
    MSG_SYNC_PROGRESS = 'spro',    // Daemon → UI
    MSG_AUTH_SUCCESS = 'asuc',     // Daemon → Preferences
    MSG_AUTH_FAILED = 'afai',      // Daemon → Preferences
    MSG_CONFLICT_DETECTED = 'conf', // Daemon → UI
};

// Message Fields
// MSG_STATUS_UPDATE fields:
//   "status" (int32) - Current sync status
//   "files_synced" (int32) - Number of files synced
//   "files_total" (int32) - Total files to sync
//   "errors" (int32) - Error count

// MSG_SYNC_PROGRESS fields:
//   "file_path" (string) - Current file being synced
//   "progress" (float) - Progress percentage
//   "operation" (string) - upload/download/delete
```

#### Component Communication Matrix
| From | To | Protocol | Purpose |
|------|----|---------:|---------|
| Preferences | Daemon | BMessage | Configuration, Control |
| Daemon | Preferences | BMessage | Status, Auth Results |
| Daemon | Tracker Add-on | File Attributes | Sync Status |
| Daemon | Status Monitor | BMessage | Status Updates |
| Tracker | Daemon | BMessage | File Operations |
| All Components | API Client | Direct Call | API Operations |

### Storage Architecture

#### Directory Structure

See [ARCHITECTURE_DIAGRAMS.md](ARCHITECTURE_DIAGRAMS.md) for the complete storage structure layout.

#### Database Schema (SQLite)
```sql
-- File metadata table
CREATE TABLE files (
    id TEXT PRIMARY KEY,
    path TEXT NOT NULL,
    name TEXT NOT NULL,
    size INTEGER,
    modified_time INTEGER,
    etag TEXT,
    parent_id TEXT,
    is_folder BOOLEAN,
    sync_status INTEGER,
    local_hash TEXT,
    attributes BLOB
);

-- Sync queue table
CREATE TABLE sync_queue (
    id INTEGER PRIMARY KEY,
    file_id TEXT,
    operation TEXT,
    priority INTEGER,
    created_time INTEGER,
    retry_count INTEGER
);

-- Account information
CREATE TABLE accounts (
    id TEXT PRIMARY KEY,
    email TEXT,
    display_name TEXT,
    drive_id TEXT,
    last_sync INTEGER
);
```

### Thread Architecture

#### Threading Model
1. **Main Thread (Daemon)**
   - Message handling
   - UI communication
   - Service coordination

2. **Sync Thread (BLooper)**
   - File synchronization
   - Queue processing
   - Conflict detection

3. **Monitor Thread (BLooper)**
   - BNodeMonitor events
   - File system watching
   - Change detection

4. **Network Thread Pool (Enhanced)**
   - Multiple concurrent API requests
   - Parallel uploads/downloads using connection pool
   - Intelligent request distribution
   - Per-connection rate limiting
   - Dynamic pool sizing based on bandwidth

#### Thread Safety
- All shared data protected by BLocker
- Message passing for cross-thread communication
- Atomic operations for status updates
- Thread-safe queue implementations

### Error Handling Strategy

#### Error Categories
1. **Network Errors**
   - Retry with exponential backoff
   - Queue failed operations
   - Notify user after threshold

2. **Authentication Errors**
   - Automatic token refresh
   - Re-authentication prompt
   - Account status tracking

3. **File System Errors**
   - Permission issues logged
   - Space checks before download
   - Attribute preservation fallback

4. **Sync Conflicts**
   - Automatic resolution rules
   - User intervention options
   - Conflict history tracking

### Resource Management

#### Memory Management
- Lazy loading of file metadata
- LRU cache for recent files
- Configurable cache limits
- Memory-mapped file operations

#### Network Optimization (Enhanced)
- **Parallel Operations**: Multiple concurrent uploads/downloads
- **Connection Pooling**: Reuse HTTP connections across requests
- **Smart Scheduling**: Prioritize small files for quick wins
- **Chunked Transfers**: Split large files across connections
- **Delta Sync**: Only transfer changed portions
- **Batch API Operations**: Combine multiple operations per request
- **Compression**: gzip for API responses
- **Bandwidth Management**: 
  - Per-connection throttling
  - Total bandwidth limits
  - Dynamic pool sizing based on available bandwidth
  
**Parallel Sync Strategy**:
```cpp
// Example configuration
struct SyncConfig {
    int maxConnections = 4;        // Total connections
    int maxUploads = 3;           // Concurrent uploads
    int maxDownloads = 4;         // Concurrent downloads
    int chunkSize = 4 * 1024 * 1024;  // 4MB chunks
    int smallFileThreshold = 1024 * 1024;  // 1MB
};
```

#### Power Management
- Detect system power state
- Reduce activity on battery
- Wake locks for critical ops
- Scheduled sync windows

---
*This document is updated whenever architectural decisions are made or changed.*