# Haiku OneDrive Client - Architecture Diagrams

This document contains system architecture diagrams optimized for monospace fonts.

## System Overview

### Layer Architecture

```
User Interface Layer
--------------------
- Preferences Application
- Tracker Add-on (OneDrive.so)
- Status Monitor (Deskbar Icon)

Core Service Layer
------------------
- OneDrive Daemon (BApplication)
  - Message Handler
  - Service Manager
  - Node Monitor
- Service Components
  - Authentication Service
  - Sync Engine (BLooper)
  - Cache Manager

Storage Layer
-------------
- BKeyStore (Tokens)
- File System (Sync Folder)
- SQLite Database (Metadata)
```

## Component Communication

### 1. Authentication Flow

```
Preferences App
     |
     | MSG_AUTHENTICATE
     v
OneDrive Daemon
     |
     v
Auth Service
     |
     | Launch
     v
Web Browser (OAuth2)
     |
     | Redirect
     v
Token Manager
     |
     v
BKeyStore
```

### 2. File Synchronization with Parallel Connections

```
Local File Changes:

File System --> File Monitor --> Sync Engine
                (BNodeMonitor)        |
                                     v
                              Request Dispatcher
                                     |
                    +----------------+----------------+
                    |                |                |
                    v                v                v
              Connection 1     Connection 2     Connection 3
              (BHttpSession)   (BHttpSession)   (BHttpSession)
                    |                |                |
                    +----------------+----------------+
                                     |
                                     v
                              Microsoft Graph API


Remote Changes:

Graph API --> Remote Monitor --> Sync Engine
              (Multi-Session)         |
                                     v
                                Cache Manager
                                     |
                                     v
                                File System
```

### 3. Status Update Flow

```
Sync Engine --> Daemon --> Status Monitor (Deskbar)
                  |
                  +--> Tracker Add-on (Icons)
```

## Message Protocol

### IPC Messages

```
Control Messages:
- MSG_AUTHENTICATE  'auth'
- MSG_SYNC_NOW     'sync'
- MSG_PAUSE_SYNC   'paus'
- MSG_RESUME_SYNC  'resm'
- MSG_GET_STATUS   'stat'
- MSG_OPEN_PREFS   'pref'
- MSG_QUIT_DAEMON  'quit'

Status Messages:
- MSG_STATUS_UPDATE     'stup'
- MSG_SYNC_PROGRESS    'spro'
- MSG_AUTH_SUCCESS     'asuc'
- MSG_AUTH_FAILED      'afai'
- MSG_CONFLICT_DETECTED 'conf'
```

## Storage Structure

```
~/config/settings/OneDrive/
|
+-- accounts/
|   +-- default.account
|   +-- [email].account
|
+-- cache/
|   +-- content/
|   |   +-- [file_id]
|   +-- thumbnails/
|
+-- database/
|   +-- metadata.db
|   +-- sync_queue.db
|   +-- conflicts.db
|
+-- logs/
|   +-- sync.log
|   +-- error.log
|
+-- settings
```

## Threading Model

```
Main Thread (Daemon)
- Message handling
- UI communication
- Service coordination

Sync Thread (BLooper)
- File synchronization
- Queue processing
- Conflict detection

Monitor Thread (BLooper)
- BNodeMonitor events
- File system watching
- Change detection

Network Thread Pool
- Connection 1: Upload operations
- Connection 2: Download operations
- Connection 3: Metadata operations
- Connection 4: Spare/overflow
```

## Connection Pool Architecture

```
Sync Engine
     |
     v
Request Dispatcher
     |
     +-- Small Files Queue (< 1MB)
     |   Priority: High
     |
     +-- Large Files Queue (> 1MB)
     |   Priority: Normal
     |
     v
Connection Pool Manager
     |
     +-- Session 1 [Active]
     +-- Session 2 [Active]
     +-- Session 3 [Idle]
     +-- Session 4 [Reserved]

Configuration:
- Max Connections: 4
- Max Uploads: 3
- Max Downloads: 4
- Chunk Size: 4MB
```

## Database Schema Overview

```
metadata.db
-----------
files
  - id (TEXT PRIMARY KEY)
  - path, name, size
  - modified_time, etag
  - sync_status, attributes

sync_queue
  - id (INTEGER PRIMARY KEY)
  - file_id, operation
  - priority, retry_count

accounts
  - id, email
  - display_name, drive_id
  - last_sync
```

## Error Handling Flow

```
Error Detection
     |
     +-- Network Error -----> Exponential Backoff
     |                       Queue for Retry
     |
     +-- Auth Error -------> Token Refresh
     |                       Re-authenticate
     |
     +-- File Error -------> Log Error
     |                       User Notification
     |
     +-- Sync Conflict ----> Conflict Resolution
                             User Intervention
```

---
*These diagrams use simple ASCII formatting optimized for monospace fonts and 8-space tabs.*