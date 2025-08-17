# OneDrive for Haiku - Dependencies

## Runtime Dependencies

### Core Haiku Libraries (Required)
- **haiku** >= r1~beta4_hrev57937
  - Application Kit (BApplication, BLooper, BHandler)
  - Interface Kit (BWindow, BView, BButton, etc.)
  - Storage Kit (BFile, BDirectory, BNodeMonitor)
  - Network Kit (BHttpSession, BHttpRequest)
  - Support Kit (BString, BMessage)
  - BKeyStore (for secure token storage)

### External Libraries
- **libcrypto** >= 3.0.11 (OpenSSL)
  - For secure random number generation
  - For future TLS certificate validation
  
- **libssl** >= 3.0.11 (OpenSSL)
  - For HTTPS connections
  - Required by BHttpSession for SSL/TLS
  
- **libsqlite3** >= 3.43.1
  - For local metadata cache
  - For sync state tracking

## Build Dependencies

### Required
- **cmake** >= 3.26
- **gcc** >= 13.2
- **haiku_devel** >= r1~beta4_hrev57937

### Optional (for development)
- **git** - Version control
- **gdb** - Debugging
- **strace** - System call tracing

## NOT Used
- **liboauth** - Only supports OAuth 1.0, we need OAuth 2.0
- **Qt** - We use native Haiku Interface Kit
- **boost** - Using standard C++ library features

## OAuth 2.0 Implementation
Since liboauth only supports OAuth 1.0/1.0a, we implement OAuth 2.0 directly using:
- Native Haiku HTTP APIs (BHttpSession, BHttpRequest)
- Manual URL encoding and parameter handling
- Direct implementation of authorization code flow
- Token management using BKeyStore

## Installation Commands
```bash
# Install runtime dependencies
pkgman install openssl sqlite

# Install build dependencies
pkgman install cmake_devel gcc haiku_devel

# For development
pkgman install git gdb strace
```

## Future Considerations
- **JSON Library**: Currently implementing minimal JSON parsing. May add a library if needs grow.
- **Compression**: May add zlib for response compression support.
- **Image Processing**: May need for thumbnail generation in future versions.