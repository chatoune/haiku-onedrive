# OneDrive Tracker Add-on
# Context menu integration for Haiku Tracker

# Create Tracker add-on
add_library(OneDriveTracker MODULE
    OneDriveTrackerAddon.cpp
    OneDriveTrackerAddon.h
)

# Set properties for add-on
set_target_properties(OneDriveTracker PROPERTIES
    PREFIX ""  # No lib prefix for add-ons
    OUTPUT_NAME "OneDrive"  # Final name: OneDrive
)

# Include directories
target_include_directories(OneDriveTracker PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

# Link with required libraries
target_link_libraries(OneDriveTracker
    onedrive_shared
    ${BE_LIB}
    ${STORAGE_LIB}
    ${LOCALESTUB_LIB}
)

# Install to Tracker add-ons directory
install(TARGETS OneDriveTracker
    LIBRARY DESTINATION non-packaged/add-ons/Tracker
)