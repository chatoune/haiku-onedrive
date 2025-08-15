# Filesystem module for Haiku OneDrive
# Virtual filesystem and Tracker integration

# Source files
set(FILESYSTEM_SOURCES
    VirtualFolder.cpp
    SyncStateIcons.cpp
    ISyncStateHandler.h
    DragDropHandler.cpp
    DragDropHandler.h
)

# Create static library
add_library(onedrive_filesystem STATIC ${FILESYSTEM_SOURCES})

# Include directories
target_include_directories(onedrive_filesystem PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

# Link with Haiku libraries
target_link_libraries(onedrive_filesystem PUBLIC
    be
    tracker
    ${HAIKU_LOCALE_LIBS}
)

# Set C++ standard
set_target_properties(onedrive_filesystem PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)