/**
 * @file main.cpp
 * @brief Entry point for the OneDrive daemon process
 * @author Haiku OneDrive Team
 * @version 1.0.0
 * @date 2025-08-14
 * 
 * This file contains the main entry point and command-line processing
 * for the OneDrive daemon. It handles signal management, argument parsing,
 * and daemon lifecycle management.
 */

#include "OneDriveDaemon.h"
#include "../shared/OneDriveConstants.h"
#include <stdio.h>
#include <signal.h>
#include <syslog.h>
#include <unistd.h>
#include <getopt.h>

/**
 * @brief Global daemon instance for signal handling
 * 
 * Used by signal handlers to gracefully shutdown the daemon when
 * receiving termination signals.
 */
static OneDriveDaemon* gDaemon = nullptr;

/**
 * @brief Signal handler for graceful daemon shutdown
 * 
 * Handles SIGTERM, SIGINT, and SIGHUP signals by posting a shutdown
 * message to the daemon's message loop. This ensures proper cleanup
 * of resources and state saving.
 * 
 * @param signal The signal number that was received
 * 
 * @note This function is called asynchronously and should only perform
 *       signal-safe operations.
 */
void
signal_handler(int signal)
{
    if (gDaemon != nullptr) {
        printf("OneDrive daemon: Received signal %d, shutting down gracefully\n", signal);
        syslog(LOG_INFO, "OneDrive daemon: Received signal %d, shutting down", signal);
        gDaemon->PostMessage(MSG_SHUTDOWN_DAEMON);
    }
}

/**
 * @brief Print command-line usage information
 * 
 * Displays help text showing available command-line options and their
 * descriptions.
 * 
 * @param progname The program name (typically argv[0])
 */
void
print_usage(const char* progname)
{
    printf("Usage: %s [OPTIONS]\n", progname);
    printf("OneDrive daemon for Haiku OS\n\n");
    printf("Options:\n");
    printf("  -h, --help        Show this help message\n");
    printf("  -v, --version     Show version information\n");
    printf("  -d, --daemon      Run as daemon (default)\n");
    printf("  -f, --foreground  Run in foreground\n");
    printf("  --debug           Enable debug logging\n");
}

/**
 * @brief Print daemon version and build information
 * 
 * Displays version number, application signature, and other build-time
 * information about the daemon.
 */
void
print_version()
{
    printf("OneDrive daemon for Haiku OS\n");
    printf("Version: %s\n", APP_VERSION);
    printf("Application signature: %s\n", APP_SIGNATURE);
}

/**
 * @brief Main entry point for the OneDrive daemon
 * 
 * This function serves as the main entry point for the OneDrive daemon process.
 * It handles command-line argument parsing, logging setup, signal handling,
 * and daemon lifecycle management.
 * 
 * The daemon supports several operating modes:
 * - Background daemon mode (default): Runs as a system service
 * - Foreground mode (-f): Runs in the foreground with console output
 * - Debug mode (--debug): Enables verbose debug logging
 * 
 * Signal handling:
 * - SIGTERM: Graceful shutdown
 * - SIGINT: Graceful shutdown (Ctrl+C)
 * - SIGHUP: Graceful shutdown
 * 
 * @param argc Number of command-line arguments
 * @param argv Array of command-line argument strings
 * 
 * @return Exit code: 0 for success, 1 for error
 * 
 * @retval 0 Daemon ran successfully and exited normally
 * @retval 1 Error occurred during startup or execution
 */
int
main(int argc, char* argv[])
{
    bool foreground = false;  ///< Whether to run in foreground mode
    bool debug = false;       ///< Whether to enable debug logging
    
    // Parse command line arguments
    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"version", no_argument, 0, 'v'},
        {"daemon", no_argument, 0, 'd'},
        {"foreground", no_argument, 0, 'f'},
        {"debug", no_argument, 0, 0},
        {0, 0, 0, 0}
    };
    
    int c;
    int option_index = 0;
    
    while ((c = getopt_long(argc, argv, "hvdf", long_options, &option_index)) != -1) {
        switch (c) {
            case 'h':
                print_usage(argv[0]);
                return 0;
                
            case 'v':
                print_version();
                return 0;
                
            case 'd':
                foreground = false;
                break;
                
            case 'f':
                foreground = true;
                break;
                
            case 0:
                if (strcmp(long_options[option_index].name, "debug") == 0) {
                    debug = true;
                }
                break;
                
            case '?':
                print_usage(argv[0]);
                return 1;
                
            default:
                break;
        }
    }
    
    // Initialize syslog
    openlog("onedrive_daemon", LOG_PID | (foreground ? LOG_PERROR : 0), LOG_DAEMON);
    
    if (debug) {
        setlogmask(LOG_UPTO(LOG_DEBUG));
    } else {
        setlogmask(LOG_UPTO(LOG_INFO));
    }
    
    // Set up signal handlers for graceful shutdown
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
    signal(SIGHUP, signal_handler);
    
    syslog(LOG_INFO, "OneDrive daemon starting (version %s)", APP_VERSION);
    
    if (foreground) {
        printf("OneDrive daemon starting in foreground mode...\n");
        printf("Version: %s\n", APP_VERSION);
        printf("Press Ctrl+C to stop\n\n");
    }
    
    // Create and run daemon
    OneDriveDaemon daemon;
    gDaemon = &daemon;
    
    status_t result = daemon.Run();
    
    gDaemon = nullptr;
    
    if (result == B_OK) {
        syslog(LOG_INFO, "OneDrive daemon exited normally");
        if (foreground) {
            printf("OneDrive daemon exited normally\n");
        }
    } else {
        syslog(LOG_ERR, "OneDrive daemon exited with error: %s", strerror(result));
        if (foreground) {
            printf("OneDrive daemon exited with error: %s\n", strerror(result));
        }
    }
    
    closelog();
    return result == B_OK ? 0 : 1;
}