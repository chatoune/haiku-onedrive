cmake_minimum_required(VERSION 3.16)
project(HaikuOneDrive VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Application signature for Haiku localization
set(APP_MIME_SIG "application/x-vnd.HaikuOneDrive")

# Supported languages
set(LOCALES en de fr es ja)

# Haiku-specific settings
if(HAIKU)
    set(CMAKE_EXECUTABLE_SUFFIX "")
    set(CMAKE_SHARED_LIBRARY_SUFFIX ".so")
endif()

# Find Haiku system libraries
find_library(BE_LIB be REQUIRED)
find_library(NETWORK_LIB network REQUIRED)
find_library(STORAGE_LIB tracker REQUIRED)
find_library(LOCALESTUB_LIB localestub REQUIRED)

# Include directories
include_directories(src/shared)

# Localization support functions
function(add_catkeys target_name sources)
    set(CATKEYS_FILE "${CMAKE_SOURCE_DIR}/locales/en.catkeys")
    
    add_custom_target(${target_name}_catkeys
        COMMAND collectcatkeys -v -s ${APP_MIME_SIG} -o ${CATKEYS_FILE} ${sources}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Extracting translatable strings for ${target_name}"
        SOURCES ${sources}
    )
    
    foreach(locale ${LOCALES})
        set(CATKEYS_INPUT "${CMAKE_SOURCE_DIR}/locales/${locale}.catkeys")
        set(CATALOG_OUTPUT "${CMAKE_BINARY_DIR}/data/locale/catalogs/${APP_MIME_SIG}/${locale}.catalog")
        
        add_custom_command(
            OUTPUT ${CATALOG_OUTPUT}
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/data/locale/catalogs/${APP_MIME_SIG}"
            COMMAND linkcatkeys -o ${CATALOG_OUTPUT} -s ${APP_MIME_SIG} -l ${locale} ${CATKEYS_INPUT}
            DEPENDS ${CATKEYS_INPUT}
            COMMENT "Compiling catalog for ${locale}"
        )
        
        list(APPEND CATALOGS ${CATALOG_OUTPUT})
    endforeach()
    
    add_custom_target(${target_name}_catalogs
        DEPENDS ${CATALOGS}
        COMMENT "Building all catalogs for ${target_name}"
    )
    
    # Install catalogs
    foreach(locale ${LOCALES})
        install(FILES "${CMAKE_BINARY_DIR}/data/locale/catalogs/${APP_MIME_SIG}/${locale}.catalog"
                DESTINATION "data/locale/catalogs/${APP_MIME_SIG}")
    endforeach()
endfunction()

# Global targets for all localization
add_custom_target(catkeys)
add_custom_target(catalogs)

# Add subdirectories
add_subdirectory(src/daemon)
add_subdirectory(src/preferences)
add_subdirectory(src/api)
add_subdirectory(src/shared)
add_subdirectory(src/filesystem)
add_subdirectory(src/tracker)

# Tests (optional)
option(BUILD_TESTS "Build test programs" ON)
if(BUILD_TESTS)
    add_subdirectory(src/tests)
endif()

# Documentation generation
option(BUILD_DOCS "Build API documentation with Doxygen" ON)
if(BUILD_DOCS)
    find_program(DOXYGEN_EXECUTABLE doxygen)
    if(DOXYGEN_EXECUTABLE)
        set(DOXYGEN_CONFIG_IN ${CMAKE_SOURCE_DIR}/Doxyfile.in)
        set(DOXYGEN_CONFIG ${CMAKE_BINARY_DIR}/Doxyfile)
        
        # Configure Doxygen configuration file
        configure_file(${CMAKE_SOURCE_DIR}/Doxyfile ${DOXYGEN_CONFIG} @ONLY)
        
        # Add documentation target
        add_custom_target(docs
            COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_CONFIG}
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            COMMENT "Generating API documentation with Doxygen"
            VERBATIM
        )
        
        # Add docs to all target (optional - remove if you don't want automatic generation)
        # add_dependencies(all docs)
        
        message(STATUS "Doxygen found: ${DOXYGEN_EXECUTABLE}")
        message(STATUS "Documentation will be generated in: ${CMAKE_SOURCE_DIR}/documentation/api")
    else()
        message(WARNING "Doxygen not found. Documentation generation disabled.")
    endif()
endif()