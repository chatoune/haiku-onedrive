/**
 * @file ConnectionPool.h
 * @brief Adaptive connection pool for OneDrive API with dynamic limit discovery
 * @author Haiku OneDrive Team
 * @version 1.0.0
 * @date 2025-08-15
 * 
 * This class manages a pool of HTTP connections to OneDrive, automatically
 * discovering the maximum number of concurrent connections allowed by the
 * service through adaptive probing.
 */

#ifndef CONNECTION_POOL_H
#define CONNECTION_POOL_H

#include <Handler.h>
#include <List.h>
#include <Locker.h>
#include <MessageQueue.h>
#include <String.h>

#include <atomic>
#include <memory>
#include <queue>
#include <vector>

// Forward declaration
class OneDriveAPI;

namespace OneDrive {

/**
 * @brief Connection state
 */
enum ConnectionState {
    kConnectionIdle = 0,        ///< Connection is idle and available
    kConnectionBusy,            ///< Connection is processing a request
    kConnectionFailed,          ///< Connection has failed
    kConnectionClosed           ///< Connection is closed
};

/**
 * @brief Connection statistics
 */
struct ConnectionStats {
    int32 totalConnections;     ///< Total number of connections in pool
    int32 activeConnections;    ///< Currently active connections
    int32 idleConnections;      ///< Currently idle connections
    int32 failedConnections;    ///< Failed connections
    int32 maxConcurrent;        ///< Maximum concurrent connections discovered
    int32 successfulRequests;   ///< Total successful requests
    int32 failedRequests;       ///< Total failed requests
    float averageLatency;       ///< Average request latency (ms)
    bigtime_t discoveryTime;    ///< Time taken to discover limits (µs)
};

/**
 * @brief HTTP connection wrapper
 */
class HttpConnection {
public:
    HttpConnection(int32 id, void* context);
    ~HttpConnection();
    
    /**
     * @brief Get connection ID
     */
    int32 GetID() const { return fID; }
    
    /**
     * @brief Get connection state
     */
    ConnectionState GetState() const { return fState; }
    
    /**
     * @brief Check if connection is available
     */
    bool IsAvailable() const { return fState == kConnectionIdle; }
    
    /**
     * @brief Send HTTP request
     */
    status_t SendRequest(void* request); // BHttpRequest*
    
    /**
     * @brief Mark connection as busy
     */
    void SetBusy() { fState = kConnectionBusy; }
    
    /**
     * @brief Mark connection as idle
     */
    void SetIdle() { fState = kConnectionIdle; }
    
    /**
     * @brief Mark connection as failed
     */
    void SetFailed() { fState = kConnectionFailed; fFailureCount++; }
    
    /**
     * @brief Get failure count
     */
    int32 GetFailureCount() const { return fFailureCount; }
    
    /**
     * @brief Reset connection
     */
    void Reset();

private:
    int32 fID;                          ///< Connection identifier
    ConnectionState fState;             ///< Current connection state
    void* fUrlContext;                  ///< URL context for cookies/auth (BUrlContext*)
    void* fCurrentRequest;              ///< Current request (BHttpRequest*)
    int32 fFailureCount;                ///< Number of failures
    bigtime_t fLastUsed;                ///< Last time connection was used
};

/**
 * @brief Request queue item
 */
struct QueuedRequest {
    void* request;                      ///< HTTP request (BHttpRequest*)
    ::BHandler* replyTarget;            ///< Handler for reply
    uint32 replyWhat;                   ///< Message what for reply
    int32 priority;                     ///< Request priority (higher = more important)
    bigtime_t queueTime;                ///< Time when queued
    int32 retryCount;                   ///< Number of retries
};

/**
 * @brief Comparison function for QueuedRequest priority queue
 */
struct QueuedRequestCompare {
    bool operator()(const QueuedRequest& a, const QueuedRequest& b) const {
        // Higher priority first, then older requests first
        if (a.priority != b.priority) {
            return a.priority < b.priority; // Note: < because priority_queue is max heap
        }
        return a.queueTime > b.queueTime; // Older requests first
    }
};

/**
 * @brief Adaptive connection pool with dynamic limit discovery
 * 
 * This class manages a pool of HTTP connections to OneDrive and automatically
 * discovers the maximum number of concurrent connections allowed by probing
 * the service. It uses an adaptive algorithm that:
 * 
 * 1. Starts with a conservative number of connections
 * 2. Gradually increases connections until failures occur
 * 3. Backs off and finds the optimal limit
 * 4. Periodically re-probes to adapt to changing conditions
 * 
 * @see OneDriveAPI
 * @since 1.0.0
 */
class ConnectionPool : public ::BHandler {
public:
    /**
     * @brief Constructor
     * 
     * @param api Reference to OneDrive API for callbacks
     * @param urlContext Shared URL context for authentication
     */
    ConnectionPool(::OneDriveAPI& api, void* urlContext);
    
    /**
     * @brief Destructor
     */
    virtual ~ConnectionPool();
    
    /**
     * @brief Initialize the connection pool
     * 
     * Starts the adaptive discovery process to find maximum connections.
     * 
     * @return B_OK on success
     */
    status_t Initialize();
    
    /**
     * @brief Shutdown the connection pool
     * 
     * Closes all connections and cleans up resources.
     */
    void Shutdown();
    
    /**
     * @brief Submit a request to the pool
     * 
     * @param request HTTP request to execute (BHttpRequest*)
     * @param replyTarget Handler to receive reply
     * @param replyWhat Message what for reply
     * @param priority Request priority (0-10, higher = more important)
     * @return B_OK on success
     */
    status_t SubmitRequest(void* request, ::BHandler* replyTarget,
                          uint32 replyWhat, int32 priority = 5);
    
    /**
     * @brief Get connection pool statistics
     * 
     * @return Current connection pool statistics
     */
    ConnectionStats GetStatistics() const;
    
    /**
     * @brief Get current maximum connections
     * 
     * @return Maximum concurrent connections discovered
     */
    int32 GetMaxConnections() const { return fMaxConnections; }
    
    /**
     * @brief Force re-discovery of connection limits
     * 
     * Useful when network conditions change or after errors.
     */
    void ForceRediscovery();
    
    /**
     * @brief Set minimum connections
     * 
     * @param min Minimum number of connections to maintain
     */
    void SetMinConnections(int32 min);
    
    /**
     * @brief Set discovery parameters
     * 
     * @param probeInterval How often to re-probe (seconds)
     * @param backoffFactor Multiplier for exponential backoff
     */
    void SetDiscoveryParams(int32 probeInterval, float backoffFactor);
    
    /**
     * @brief Handle messages
     */
    virtual void MessageReceived(BMessage* message);

private:
    /**
     * @brief Start adaptive discovery process
     */
    void _StartDiscovery();
    
    /**
     * @brief Discovery worker thread
     */
    static int32 _DiscoveryThread(void* data);
    
    /**
     * @brief Perform discovery probe
     */
    status_t _PerformDiscoveryProbe();
    
    /**
     * @brief Test connection limit
     * 
     * @param targetConnections Number of connections to test
     * @return true if limit is successful, false if too many
     */
    bool _TestConnectionLimit(int32 targetConnections);
    
    /**
     * @brief Create new connection
     * 
     * @return New connection or nullptr on failure
     */
    HttpConnection* _CreateConnection();
    
    /**
     * @brief Get available connection
     * 
     * @return Available connection or nullptr if none available
     */
    HttpConnection* _GetAvailableConnection();
    
    /**
     * @brief Process request queue
     */
    void _ProcessRequestQueue();
    
    /**
     * @brief Handle connection completion
     * 
     * @param connection Connection that completed
     * @param success Whether request was successful
     */
    void _HandleConnectionComplete(HttpConnection* connection, bool success);
    
    /**
     * @brief Clean up failed connections
     */
    void _CleanupFailedConnections();
    
    /**
     * @brief Update connection statistics
     * 
     * @param latency Request latency in microseconds
     * @param success Whether request was successful
     */
    void _UpdateStatistics(bigtime_t latency, bool success);
    
    /**
     * @brief Binary search for optimal connection limit
     * 
     * @param min Minimum connections that work
     * @param max Maximum connections to test
     * @return Optimal connection limit
     */
    int32 _BinarySearchLimit(int32 min, int32 max);

private:
    // Message constants
    static const uint32 kMsgDiscoveryComplete = 'disc';
    static const uint32 kMsgProcessQueue = 'prcq';
    static const uint32 kMsgConnectionComplete = 'conn';
    static const uint32 kMsgRediscoveryTimer = 'redi';
    
    // Discovery parameters
    static const int32 kInitialConnections = 2;
    static const int32 kMaxPossibleConnections = 50;
    static const int32 kDiscoveryTimeout = 30000000; // 30 seconds
    static constexpr float kDefaultBackoffFactor = 0.8f;
    static const int32 kDefaultProbeInterval = 300; // 5 minutes
    
    ::OneDriveAPI& fAPI;                        ///< OneDrive API reference
    void* fUrlContext;                          ///< Shared URL context (BUrlContext*)
    mutable BLocker fLock;                      ///< Thread safety lock
    
    // Connection management
    std::vector<std::unique_ptr<HttpConnection>> fConnections; ///< Connection pool
    std::priority_queue<QueuedRequest, std::vector<QueuedRequest>, QueuedRequestCompare> fRequestQueue; ///< Pending requests
    
    // Discovery state
    std::atomic<int32> fMaxConnections;        ///< Maximum concurrent connections
    std::atomic<int32> fMinConnections;        ///< Minimum connections to maintain
    std::atomic<bool> fDiscoveryInProgress;    ///< Discovery in progress flag
    thread_id fDiscoveryThread;                ///< Discovery thread ID
    bigtime_t fLastDiscoveryTime;              ///< Last discovery timestamp
    int32 fProbeInterval;                      ///< Probe interval (seconds)
    float fBackoffFactor;                      ///< Backoff factor for discovery
    
    // Statistics
    std::atomic<int32> fActiveConnections;     ///< Currently active connections
    std::atomic<int32> fSuccessfulRequests;    ///< Total successful requests
    std::atomic<int32> fFailedRequests;        ///< Total failed requests
    std::atomic<int64> fTotalLatency;          ///< Total latency for averaging
    bigtime_t fDiscoveryDuration;              ///< Time taken for discovery
    
    // Flags
    bool fInitialized;                         ///< Initialization flag
    bool fShuttingDown;                        ///< Shutdown in progress
};

} // namespace OneDrive

#endif // CONNECTION_POOL_H