# Shared constants and utilities
add_library(onedrive_shared
    JSONSerializer.cpp
    JSONSerializer.h
    OneDriveConstants.h
    FileSystemConstants.h
    ColorConstants.h
    ErrorLogger.cpp
    ErrorLogger.h
    AttributeHelper.cpp
    AttributeHelper.h
)

target_include_directories(onedrive_shared PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Link Haiku system libraries
target_link_libraries(onedrive_shared
    ${BE_LIB}
    ${STORAGE_LIB}
)