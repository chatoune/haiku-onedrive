# OneDrive API library
add_library(OneDriveAPI SHARED
    AuthManager.cpp
    AuthManager.h
    OneDriveAPI.cpp
    OneDriveAPI.h
    ConnectionPool.cpp
    ConnectionPool.h
)

# Include directories
target_include_directories(OneDriveAPI PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/shared
)

# Link Haiku system libraries
target_link_libraries(OneDriveAPI
    ${BE_LIB}
    ${NETWORK_LIB}
    ${STORAGE_LIB}
    ${LOCALESTUB_LIB}
    onedrive_shared
)

# Set application signature for localization
target_compile_definitions(OneDriveAPI PRIVATE 
    APP_SIGNATURE="${APP_MIME_SIG}"
    APP_VERSION="${PROJECT_VERSION}"
)

# Enable debug symbols in debug builds
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(OneDriveAPI PRIVATE -g -O0)
    target_compile_definitions(OneDriveAPI PRIVATE DEBUG=1)
else()
    target_compile_options(OneDriveAPI PRIVATE -O2)
endif()

# Install target
install(TARGETS OneDriveAPI DESTINATION system/lib)
install(FILES AuthManager.h DESTINATION system/develop/headers/OneDrive)