# OneDrive Daemon executable
add_executable(onedrive_daemon
    main.cpp
    OneDriveDaemon.cpp
    OneDriveDaemon.h
    AttributeManager.cpp
    AttributeManager.h
    CacheManager.cpp
    CacheManager.h
    SyncEngine.cpp
    SyncEngine.h
)

# Include directories
target_include_directories(onedrive_daemon PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/shared
    ${CMAKE_SOURCE_DIR}/src/api
)

# Link Haiku system libraries and project components
target_link_libraries(onedrive_daemon
    OneDriveAPI
    onedrive_shared
    onedrive_filesystem
    ${BE_LIB}
    ${NETWORK_LIB}
    ${STORAGE_LIB}
    ${LOCALESTUB_LIB}
)

# Set application signature and version for compilation
target_compile_definitions(onedrive_daemon PRIVATE 
    APP_SIGNATURE="${APP_MIME_SIG}"
    APP_VERSION="${PROJECT_VERSION}"
)

# Enable debug symbols in debug builds
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(onedrive_daemon PRIVATE -g -O0)
    target_compile_definitions(onedrive_daemon PRIVATE DEBUG=1)
else()
    target_compile_options(onedrive_daemon PRIVATE -O2)
endif()

# Add localization support (temporarily disabled for initial build)
# set(DAEMON_SOURCES main.cpp OneDriveDaemon.cpp)
# add_catkeys(onedrive_daemon "${DAEMON_SOURCES}")

# Add dependencies for localization (temporarily disabled)
# add_dependencies(catkeys onedrive_daemon_catkeys)
# add_dependencies(catalogs onedrive_daemon_catalogs)
# add_dependencies(onedrive_daemon onedrive_daemon_catalogs)

# Install target
install(TARGETS onedrive_daemon DESTINATION system/servers)

# Install launch configuration (for system daemon management)
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/onedrive_daemon.launch.in
    ${CMAKE_CURRENT_BINARY_DIR}/onedrive_daemon.launch
    @ONLY
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/onedrive_daemon.launch 
        DESTINATION system/data/launch)