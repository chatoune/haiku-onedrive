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

### ADR-002: Technology Stack (PENDING)
**Status**: Proposed  
**Date**: 2025-08-16

**Context**: Need to choose appropriate technologies for Haiku native development.

**Decision**: TBD

**Options Under Consideration**:
1. **Programming Language**: C++ (Haiku native)
2. **Build System**: 
   - CMake (cross-platform, modern)
   - Jam (Haiku native)
3. **HTTP Client**:
   - Haiku's native network kit
   - libcurl
4. **JSON Parser**:
   - Haiku's native JSON support
   - RapidJSON
   - nlohmann/json
5. **Testing Framework**:
   - Native Haiku testing
   - Google Test
   - Catch2

---

## System Architecture (DRAFT)

### Component Overview

```
┌─────────────────────────────────────────────────────────────┐
│                         User Space                           │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────────┐  ┌──────────────┐  ┌─────────────────┐   │
│  │ Preferences │  │   Tracker    │  │  Notification   │   │
│  │     App     │  │   Add-on     │  │     Daemon      │   │
│  └──────┬──────┘  └──────┬───────┘  └────────┬────────┘   │
│         │                 │                    │            │
│         └─────────────────┴────────────────────┘            │
│                           │                                  │
│                    ┌──────▼───────┐                         │
│                    │     IPC      │                         │
│                    │   Messages   │                         │
│                    └──────┬───────┘                         │
├─────────────────────────────────────────────────────────────┤
│                      System Services                         │
├─────────────────────────────────────────────────────────────┤
│                    ┌──────▼───────┐                         │
│                    │   OneDrive   │                         │
│                    │    Daemon    │                         │
│                    └──────┬───────┘                         │
│         ┌─────────────────┼─────────────────┐               │
│         │                 │                 │               │
│    ┌────▼─────┐   ┌──────▼──────┐  ┌──────▼──────┐       │
│    │   Auth   │   │    Sync     │  │    Cache    │       │
│    │ Manager  │   │   Engine    │  │   Manager   │       │
│    └────┬─────┘   └──────┬──────┘  └──────┬──────┘       │
│         │                 │                 │               │
│    ┌────▼─────────────────▼─────────────────▼──────┐       │
│    │              OneDrive API Client              │       │
│    └───────────────────────┬───────────────────────┘       │
├─────────────────────────────────────────────────────────────┤
│                         Network                              │
├─────────────────────────────────────────────────────────────┤
│                    ┌───────▼────────┐                       │
│                    │  Microsoft     │                       │
│                    │  Graph API     │                       │
│                    └────────────────┘                       │
└─────────────────────────────────────────────────────────────┘
```

### Component Responsibilities

#### OneDrive Daemon
- Core service running in background
- Manages all sync operations
- Handles authentication state
- Coordinates between components
- Provides IPC interface

#### Authentication Manager
- OAuth2 flow implementation
- Token management and refresh
- Secure credential storage
- Session management

#### Sync Engine
- File monitoring (local changes)
- Change detection (remote changes)
- Conflict resolution
- Delta synchronization
- Metadata preservation

#### Cache Manager
- Local file cache
- Metadata database
- Offline file management
- Space management

#### API Client
- Microsoft Graph API wrapper
- Request queuing
- Rate limiting
- Error handling
- Retry logic

#### Preferences Application
- User configuration UI
- Account management
- Sync folder selection
- Advanced settings

#### Tracker Add-on
- File status overlays
- Context menu integration
- Drag & drop support
- Property integration

### Data Flow

1. **Authentication Flow**:
   ```
   User → Preferences App → Daemon → Auth Manager → Browser → OAuth2 → Token
   ```

2. **Sync Flow**:
   ```
   File Change → Monitor → Sync Engine → API Client → Graph API
   Graph API → API Client → Sync Engine → Cache → Local File
   ```

3. **UI Update Flow**:
   ```
   Sync State Change → Daemon → IPC → Tracker Add-on → Icon Update
   ```

### Design Principles

1. **Modularity**: Each component has a single, well-defined responsibility
2. **Asynchronous**: All network operations are non-blocking
3. **Resilient**: Graceful handling of network failures and conflicts
4. **Native**: Follow Haiku design patterns and UI guidelines
5. **Efficient**: Minimal resource usage, especially when idle
6. **Secure**: No credentials stored in plain text

### Security Considerations

1. **Credential Storage**: Use Haiku's keystore for OAuth tokens
2. **Communication**: All API calls over HTTPS
3. **Local Cache**: Respect file permissions
4. **IPC**: Authenticate daemon connections

### Performance Goals

1. **Startup**: < 2 seconds to daemon ready
2. **Sync Latency**: < 5 seconds for small file changes
3. **Memory**: < 50MB when idle
4. **CPU**: < 1% when idle
5. **Battery**: Minimal impact on mobile devices

### Scalability Considerations

1. **File Count**: Support up to 100,000 files
2. **File Size**: Support files up to 10GB
3. **Concurrent Ops**: Handle 10+ simultaneous transfers
4. **Change Frequency**: Handle rapid successive changes

---
*This document is updated whenever architectural decisions are made or changed.*