# Haiku OneDrive Client

A native OneDrive client for Haiku OS that provides seamless filesystem integration with Microsoft OneDrive cloud storage.

## Features

- **Native Haiku Integration**: Built specifically for Haiku OS using native APIs and UI components
- **BFS Attribute Sync**: Full support for Haiku's extended file attributes with bidirectional synchronization
- **Real-time File Sync**: Automatic synchronization of files and folders with OneDrive
- **Virtual Filesystem**: Support for online-only files with on-demand downloading
- **Custom Icon Overlays**: Visual sync status indicators in Tracker
- **OAuth2 Authentication**: Secure authentication with Microsoft accounts
- **Conflict Resolution**: Smart conflict detection and resolution strategies
- **Bandwidth Management**: Configurable upload/download limits
- **Offline Support**: Work offline with automatic sync when connection returns

## Architecture

The project consists of several components:

- **OneDrive Daemon**: Background service handling all synchronization operations
- **Preferences Application**: Native Haiku GUI for configuration and account management
- **API Library**: Microsoft Graph API client for OneDrive operations
- **Attribute Manager**: BFS extended attribute synchronization system
- **Virtual Filesystem**: File state tracking and on-demand download system

## Building from Source

### Prerequisites

- Haiku R1/beta4 or later
- CMake 3.16+
- Doxygen (for documentation generation)
- Git

### Build Instructions

```bash
# Clone the repository
git clone https://github.com/yourusername/haiku-onedrive.git
cd haiku-onedrive

# Create build directory
mkdir build && cd build

# Configure with CMake
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build the project
make -j4

# Generate documentation (optional)
make docs

# Install (optional)
make install
```

### Running

```bash
# Run the preferences application
./OneDrive\ Preferences

# Run the daemon manually (for testing)
./onedrive_daemon --foreground --debug

# The daemon will normally be started automatically via Haiku's launch services
```

## Development Status

The project is currently in active development. See [CLAUDE.md](CLAUDE.md) for detailed development progress and milestones.

### Completed Features (Milestones 1-2)
- ✅ Core daemon architecture with message handling
- ✅ OAuth2 authentication with secure token storage
- ✅ Microsoft Graph API client implementation
- ✅ Native Haiku preferences interface
- ✅ BFS attribute synchronization system
- ✅ JSON serialization for OneDrive metadata
- ✅ Conflict detection and resolution
- ✅ Comprehensive API documentation

### In Progress (Milestone 3 - 75% Complete)
- ✅ Virtual filesystem implementation
- ✅ Custom sync state icons
- 🔄 Tracker integration (menus pending)
- ✅ Real-time file monitoring

### Planned Features
- Local caching system with SQLite
- Delta synchronization
- LRU cache eviction
- Parallel upload/download
- Comprehensive test suite
- Haiku package (.hpkg) distribution

## Documentation

- [Architecture Documentation](documentation/HaikuOneDrive_Architecture.md) - Detailed technical specification
- [API Documentation](documentation/api/html/index.html) - Generated Doxygen documentation (run `make docs` first)
- [Development Guide](CLAUDE.md) - Development progress and session history

## Contributing

Contributions are welcome! Please follow these guidelines:

1. Follow Haiku coding standards and conventions
2. Use native Haiku APIs whenever possible
3. Add Doxygen documentation for all public APIs
4. Test your changes thoroughly
5. Submit pull requests with clear descriptions

## License

This project is licensed under the MIT License. See [LICENSE](LICENSE) file for details.

## Authors

- Haiku OneDrive Team

## Acknowledgments

- The Haiku OS development team for the excellent APIs and documentation
- Microsoft for the Graph API and OneDrive service
- The Haiku community for support and feedback