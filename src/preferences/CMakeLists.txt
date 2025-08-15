# OneDrive Preferences Application
add_executable(onedrive_preferences
    main.cpp
    OneDrivePrefs.cpp
    OneDrivePrefs.h
)

# Include directories
target_include_directories(onedrive_preferences PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/shared
    ${CMAKE_SOURCE_DIR}/src/api
)

# Link libraries
target_link_libraries(onedrive_preferences
    OneDriveAPI
    ${BE_LIB}
    ${INTERFACE_LIB}
    ${STORAGE_LIB}
    ${TRACKER_LIB}
    ${LOCALESTUB_LIB}
)

# Set application signature for localization
target_compile_definitions(onedrive_preferences PRIVATE 
    PREFS_APP_SIGNATURE="application/x-vnd.Haiku-OneDrivePrefs"
    APP_VERSION="${PROJECT_VERSION}"
)

# Enable debug symbols in debug builds
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(onedrive_preferences PRIVATE -g -O0)
    target_compile_definitions(onedrive_preferences PRIVATE DEBUG=1)
else()
    target_compile_options(onedrive_preferences PRIVATE -O2)
endif()

# Set proper executable properties for Haiku
set_target_properties(onedrive_preferences PROPERTIES
    OUTPUT_NAME "OneDrive Preferences"
)

# Install targets
install(TARGETS onedrive_preferences 
    DESTINATION system/bin
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

# Install application signature for MIME database
install(CODE "
    execute_process(COMMAND settype -t application/x-vnd.Haiku-OneDrivePrefs \${CMAKE_INSTALL_PREFIX}/system/bin/OneDrive\\ Preferences)
")