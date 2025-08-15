# Haiku OneDrive Documentation

This directory contains comprehensive documentation for the Haiku OneDrive project.

## Documentation Structure

### 📋 Architecture Documentation
- **[HaikuOneDrive_Architecture.md](HaikuOneDrive_Architecture.md)** - Complete technical specification and system architecture

### 🔧 API Documentation
- **[API Documentation (HTML)](api/html/index.html)** - Generated API documentation using Doxygen
- **Class Documentation**: Detailed documentation for all classes and functions
- **Source Code Browser**: Browsable source code with cross-references

## Quick Start

### For Users
1. See the **Architecture** document for high-level overview
2. Check the **Installation** section in the main README

### For Developers
1. Start with the **[Architecture document](HaikuOneDrive_Architecture.md)** for system overview
2. Browse the **[API documentation](api/html/index.html)** for detailed class information
3. Examine the **Source Code** sections for implementation details

## Key Components

The API documentation covers these main components:

- **OneDriveDaemon**: Core background service (BApplication-based)
- **Authentication System**: OAuth2 integration with Microsoft Graph
- **Sync Engine**: Bidirectional file synchronization
- **Attribute Manager**: BFS extended attribute synchronization
- **Cache Manager**: Local file caching and optimization

## Generating Documentation

To regenerate the API documentation:

```bash
# From project root
doxygen Doxyfile

# Or with CMake
make docs  # (when integrated)
```

## Navigation Tips

- Use the **search function** in the API docs for quick lookups
- **Class hierarchy** shows inheritance relationships  
- **File list** provides source code browsing
- **Function index** lists all available methods

## Documentation Standards

All code follows Doxygen documentation standards:
- **@brief**: Short description
- **@param**: Parameter descriptions
- **@return/@retval**: Return value documentation
- **@see**: Cross-references
- **@since**: Version information

---

*Documentation automatically updated with each code change.*