/**
 * @file main.cpp
 * @brief Main entry point for OneDrive preferences application
 * @author Haiku OneDrive Team
 * @version 1.0.0
 * @date 2025-08-14
 * 
 * This file contains the main function for the OneDrive preferences application.
 * The preferences app provides a native Haiku interface for configuring OneDrive
 * synchronization settings, managing user accounts, and adjusting sync options.
 */

#include "OneDrivePrefs.h"
#include "../shared/OneDriveConstants.h"

#include <syslog.h>
#include <stdio.h>
#include <getopt.h>

/**
 * @brief Print usage information
 * 
 * @param programName Name of the program executable
 */
static void
print_usage(const char* programName)
{
    printf("OneDrive Preferences %s\n", ONEDRIVE_VERSION);
    printf("Usage: %s [options]\n\n", programName);
    printf("Options:\n");
    printf("  -h, --help     Show this help message\n");
    printf("  -v, --version  Show version information\n");
    printf("\n");
    printf("OneDrive preferences application for Haiku OS.\n");
    printf("Provides configuration interface for OneDrive synchronization.\n");
}

/**
 * @brief Print version information
 */
static void
print_version()
{
    printf("OneDrive Preferences %s\n", ONEDRIVE_VERSION);
    printf("Build: %s\n", ONEDRIVE_BUILD_DATE);
    printf("Component: Preferences Application\n");
    printf("\n");
    printf("Part of the Haiku OneDrive native client.\n");
    printf("For more information, visit the project documentation.\n");
}

/**
 * @brief Main entry point
 * 
 * Creates and runs the OneDrive preferences application.
 * Handles command-line arguments and application initialization.
 * 
 * @param argc Argument count
 * @param argv Argument vector
 * @return Exit code (0 for success, non-zero for error)
 */
int
main(int argc, char* argv[])
{
    // Parse command line arguments
    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"version", no_argument, 0, 'v'},
        {0, 0, 0, 0}
    };
    
    int option_index = 0;
    int c;
    
    while ((c = getopt_long(argc, argv, "hv", long_options, &option_index)) != -1) {
        switch (c) {
            case 'h':
                print_usage(argv[0]);
                return 0;
                
            case 'v':
                print_version();
                return 0;
                
            case '?':
                fprintf(stderr, "Unknown option. Use --help for usage information.\n");
                return 1;
                
            default:
                break;
        }
    }
    
    // Initialize syslog
    openlog("OneDrivePrefs", LOG_PID, LOG_USER);
    
    syslog(LOG_INFO, "OneDrive Preferences starting (version %s)", ONEDRIVE_VERSION);
    
    try {
        // Create and run the application
        OneDrivePrefs app;
        app.Run();
        
        syslog(LOG_INFO, "OneDrive Preferences exiting normally");
        closelog();
        return 0;
        
    } catch (const std::exception& e) {
        syslog(LOG_ERR, "OneDrive Preferences: Unhandled exception: %s", e.what());
        fprintf(stderr, "Error: %s\n", e.what());
        closelog();
        return 1;
        
    } catch (...) {
        syslog(LOG_ERR, "OneDrive Preferences: Unknown exception occurred");
        fprintf(stderr, "An unknown error occurred.\n");
        closelog();
        return 1;
    }
}