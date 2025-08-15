/**
 * @file ConnectionPool.cpp
 * @brief Implementation of adaptive connection pool for OneDrive API
 * @author Haiku OneDrive Team
 * @version 1.0.0
 * @date 2025-08-15
 */

#include "ConnectionPool.h"
#include "OneDriveAPI.h"
#include "../shared/ErrorLogger.h"
#include "../shared/OneDriveConstants.h"

#include <Application.h>
#include <Autolock.h>
#include <MessageRunner.h>
#include <OS.h>

#include <algorithm>
#include <chrono>
#include <thread>

using namespace OneDrive;

// HttpConnection implementation

HttpConnection::HttpConnection(int32 id, void* context)
    : fID(id),
      fState(kConnectionIdle),
      fUrlContext(context),
      fCurrentRequest(nullptr),
      fFailureCount(0),
      fLastUsed(0)
{
}

HttpConnection::~HttpConnection()
{
    // Clean up current request if any
    // TODO: Stop request when HTTP API is integrated
}

status_t
HttpConnection::SendRequest(void* request)
{
    if (!request || fState != kConnectionIdle) {
        return B_ERROR;
    }
    
    fCurrentRequest = request;
    fState = kConnectionBusy;
    fLastUsed = system_time();
    
    // Start the request
    // Note: In real implementation, this would use BHttpSession
    // For now, we simulate the request
    return B_OK;
}

void
HttpConnection::Reset()
{
    if (fCurrentRequest) {
        // TODO: Stop request when HTTP API is integrated
        fCurrentRequest = nullptr;
    }
    
    fState = kConnectionIdle;
    fFailureCount = 0;
}

// ConnectionPool implementation

ConnectionPool::ConnectionPool(::OneDriveAPI& api, void* urlContext)
    : BHandler("ConnectionPool"),
      fAPI(api),
      fUrlContext(urlContext),
      fLock("ConnectionPool Lock"),
      fMaxConnections(kInitialConnections),
      fMinConnections(1),
      fDiscoveryInProgress(false),
      fDiscoveryThread(-1),
      fLastDiscoveryTime(0),
      fProbeInterval(kDefaultProbeInterval),
      fBackoffFactor(kDefaultBackoffFactor),
      fActiveConnections(0),
      fSuccessfulRequests(0),
      fFailedRequests(0),
      fTotalLatency(0),
      fDiscoveryDuration(0),
      fInitialized(false),
      fShuttingDown(false)
{
    LOG_INFO("ConnectionPool", "Created connection pool");
}

ConnectionPool::~ConnectionPool()
{
    Shutdown();
}

status_t
ConnectionPool::Initialize()
{
    BAutolock lock(fLock);
    
    if (fInitialized) {
        return B_OK;
    }
    
    LOG_INFO("ConnectionPool", "Initializing connection pool");
    
    // Create initial connections
    for (int32 i = 0; i < fMinConnections; i++) {
        HttpConnection* conn = _CreateConnection();
        if (conn) {
            fConnections.push_back(std::unique_ptr<HttpConnection>(conn));
        }
    }
    
    if (fConnections.empty()) {
        LOG_ERROR("ConnectionPool", "Failed to create any connections");
        return B_ERROR;
    }
    
    fInitialized = true;
    
    // Start adaptive discovery
    _StartDiscovery();
    
    // Set up periodic re-discovery
    BMessage rediscoverMsg(kMsgRediscoveryTimer);
    new BMessageRunner(this, &rediscoverMsg, fProbeInterval * 1000000LL);
    
    return B_OK;
}

void
ConnectionPool::Shutdown()
{
    BAutolock lock(fLock);
    
    if (!fInitialized) {
        return;
    }
    
    LOG_INFO("ConnectionPool", "Shutting down connection pool");
    
    fShuttingDown = true;
    
    // Stop discovery thread
    if (fDiscoveryThread >= 0) {
        status_t result;
        wait_for_thread(fDiscoveryThread, &result);
    }
    
    // Close all connections
    fConnections.clear();
    
    // Clear request queue
    while (!fRequestQueue.empty()) {
        QueuedRequest req = fRequestQueue.top();
        fRequestQueue.pop();
        // TODO: Delete BHttpRequest when HTTP API is integrated
    }
    
    fInitialized = false;
}

status_t
ConnectionPool::SubmitRequest(void* request, ::BHandler* replyTarget,
                             uint32 replyWhat, int32 priority)
{
    BAutolock lock(fLock);
    
    if (!fInitialized || fShuttingDown) {
        return B_NOT_INITIALIZED;
    }
    
    // Create queued request
    QueuedRequest queuedReq;
    queuedReq.request = request;
    queuedReq.replyTarget = replyTarget;
    queuedReq.replyWhat = replyWhat;
    queuedReq.priority = priority;
    queuedReq.queueTime = system_time();
    queuedReq.retryCount = 0;
    
    // Try to get an available connection
    HttpConnection* conn = _GetAvailableConnection();
    if (conn) {
        // Send immediately
        conn->SetBusy();
        fActiveConnections++;
        
        status_t result = conn->SendRequest(request);
        if (result != B_OK) {
            conn->SetFailed();
            fActiveConnections--;
            return result;
        }
        
        LOG_DEBUG("ConnectionPool", "Request submitted on connection %d", conn->GetID());
    } else {
        // Queue the request
        fRequestQueue.push(queuedReq);
        LOG_DEBUG("ConnectionPool", "Request queued (queue size: %zu)", fRequestQueue.size());
        
        // Try to process queue in case a connection just became available
        BLooper* looper = Looper();
        if (looper) {
            looper->PostMessage(kMsgProcessQueue, this);
        }
    }
    
    return B_OK;
}

ConnectionStats
ConnectionPool::GetStatistics() const
{
    BAutolock lock(fLock);
    
    ConnectionStats stats;
    stats.totalConnections = fConnections.size();
    stats.activeConnections = fActiveConnections;
    stats.idleConnections = 0;
    stats.failedConnections = 0;
    
    // Count connection states
    for (const auto& conn : fConnections) {
        switch (conn->GetState()) {
            case kConnectionIdle:
                stats.idleConnections++;
                break;
            case kConnectionFailed:
                stats.failedConnections++;
                break;
            default:
                break;
        }
    }
    
    stats.maxConcurrent = fMaxConnections;
    stats.successfulRequests = fSuccessfulRequests;
    stats.failedRequests = fFailedRequests;
    
    // Calculate average latency
    if (fSuccessfulRequests > 0) {
        stats.averageLatency = (float)fTotalLatency / fSuccessfulRequests / 1000.0f; // Convert to ms
    } else {
        stats.averageLatency = 0.0f;
    }
    
    stats.discoveryTime = fDiscoveryDuration;
    
    return stats;
}

void
ConnectionPool::ForceRediscovery()
{
    BAutolock lock(fLock);
    
    if (!fDiscoveryInProgress) {
        LOG_INFO("ConnectionPool", "Forcing connection limit rediscovery");
        _StartDiscovery();
    }
}

void
ConnectionPool::SetMinConnections(int32 min)
{
    BAutolock lock(fLock);
    
    fMinConnections = std::max(1, min);
    
    // Create additional connections if needed
    while (fConnections.size() < fMinConnections) {
        HttpConnection* conn = _CreateConnection();
        if (conn) {
            fConnections.push_back(std::unique_ptr<HttpConnection>(conn));
        } else {
            break;
        }
    }
}

void
ConnectionPool::SetDiscoveryParams(int32 probeInterval, float backoffFactor)
{
    BAutolock lock(fLock);
    
    fProbeInterval = std::max(60, probeInterval); // Minimum 1 minute
    fBackoffFactor = std::min(0.95f, std::max(0.5f, backoffFactor));
}

void
ConnectionPool::MessageReceived(BMessage* message)
{
    switch (message->what) {
        case kMsgDiscoveryComplete:
        {
            int32 maxConnections;
            if (message->FindInt32("max_connections", &maxConnections) == B_OK) {
                fMaxConnections = maxConnections;
                fDiscoveryInProgress = false;
                
                bigtime_t duration;
                if (message->FindInt64("duration", &duration) == B_OK) {
                    fDiscoveryDuration = duration;
                }
                
                LOG_INFO("ConnectionPool", "Discovery complete: max connections = %d (took %.2f seconds)",
                    maxConnections, duration / 1000000.0);
                
                // Adjust connection pool size
                BAutolock lock(fLock);
                
                // Add connections up to discovered maximum
                while (fConnections.size() < fMaxConnections) {
                    HttpConnection* conn = _CreateConnection();
                    if (conn) {
                        fConnections.push_back(std::unique_ptr<HttpConnection>(conn));
                    } else {
                        break;
                    }
                }
                
                // Process any queued requests
                _ProcessRequestQueue();
            }
            break;
        }
        
        case kMsgProcessQueue:
            _ProcessRequestQueue();
            break;
            
        case kMsgConnectionComplete:
        {
            int32 connID;
            bool success;
            bigtime_t latency;
            
            if (message->FindInt32("connection_id", &connID) == B_OK &&
                message->FindBool("success", &success) == B_OK &&
                message->FindInt64("latency", &latency) == B_OK) {
                
                // Find the connection
                for (auto& conn : fConnections) {
                    if (conn->GetID() == connID) {
                        _HandleConnectionComplete(conn.get(), success);
                        _UpdateStatistics(latency, success);
                        break;
                    }
                }
            }
            break;
        }
        
        case kMsgRediscoveryTimer:
            // Periodic re-discovery
            if (!fDiscoveryInProgress && 
                (system_time() - fLastDiscoveryTime) > fProbeInterval * 1000000LL) {
                _StartDiscovery();
            }
            break;
            
        default:
            BHandler::MessageReceived(message);
            break;
    }
}

void
ConnectionPool::_StartDiscovery()
{
    if (fDiscoveryInProgress || fShuttingDown) {
        return;
    }
    
    fDiscoveryInProgress = true;
    fLastDiscoveryTime = system_time();
    
    // Start discovery thread
    fDiscoveryThread = spawn_thread(_DiscoveryThread, "connection_discovery",
                                   B_NORMAL_PRIORITY, this);
    
    if (fDiscoveryThread >= 0) {
        resume_thread(fDiscoveryThread);
    } else {
        fDiscoveryInProgress = false;
        LOG_ERROR("ConnectionPool", "Failed to start discovery thread");
    }
}

int32
ConnectionPool::_DiscoveryThread(void* data)
{
    ConnectionPool* pool = static_cast<ConnectionPool*>(data);
    return pool->_PerformDiscoveryProbe();
}

status_t
ConnectionPool::_PerformDiscoveryProbe()
{
    LOG_INFO("ConnectionPool", "Starting adaptive connection discovery");
    
    bigtime_t startTime = system_time();
    
    // Binary search for optimal connection limit
    int32 minWorking = fMinConnections;
    int32 maxTested = kMaxPossibleConnections;
    int32 optimal = _BinarySearchLimit(minWorking, maxTested);
    
    bigtime_t duration = system_time() - startTime;
    
    // Send completion message
    BMessage complete(kMsgDiscoveryComplete);
    complete.AddInt32("max_connections", optimal);
    complete.AddInt64("duration", duration);
    
    if (Looper()) {
        Looper()->PostMessage(&complete, this);
    }
    
    return B_OK;
}

bool
ConnectionPool::_TestConnectionLimit(int32 targetConnections)
{
    LOG_DEBUG("ConnectionPool", "Testing %d concurrent connections", targetConnections);
    
    // Create test requests
    // TODO: Uncomment when BHttpRequest is available
    // std::vector<std::unique_ptr<BHttpRequest>> testRequests;
    std::atomic<int32> successCount(0);
    std::atomic<int32> failureCount(0);
    
    // Use a simple endpoint for testing (e.g., drive info)
    for (int32 i = 0; i < targetConnections; i++) {
        // In real implementation, create actual HTTP requests
        // For now, we simulate the test
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // Simulate success/failure based on connection count
    // In reality, this would be based on actual HTTP responses
    bool success = true;
    
    // Heuristic: OneDrive typically allows 4-8 concurrent connections
    // We'll simulate failures above a certain threshold
    if (targetConnections > 8) {
        // Increasing chance of failure as we go higher
        float failureProbability = (targetConnections - 8) / 20.0f;
        success = (rand() / (float)RAND_MAX) > failureProbability;
    }
    
    if (success) {
        LOG_DEBUG("ConnectionPool", "Successfully tested %d connections", targetConnections);
    } else {
        LOG_DEBUG("ConnectionPool", "Failed at %d connections", targetConnections);
    }
    
    return success;
}

HttpConnection*
ConnectionPool::_CreateConnection()
{
    static std::atomic<int32> nextID(1);
    
    HttpConnection* conn = new(std::nothrow) HttpConnection(nextID++, fUrlContext);
    if (!conn) {
        LOG_ERROR("ConnectionPool", "Failed to allocate connection");
        return nullptr;
    }
    
    LOG_DEBUG("ConnectionPool", "Created connection %d", conn->GetID());
    return conn;
}

HttpConnection*
ConnectionPool::_GetAvailableConnection()
{
    for (auto& conn : fConnections) {
        if (conn->IsAvailable() && conn->GetFailureCount() < 3) {
            return conn.get();
        }
    }
    
    // Try to create a new connection if under limit
    if (fConnections.size() < fMaxConnections) {
        HttpConnection* newConn = _CreateConnection();
        if (newConn) {
            fConnections.push_back(std::unique_ptr<HttpConnection>(newConn));
            return newConn;
        }
    }
    
    return nullptr;
}

void
ConnectionPool::_ProcessRequestQueue()
{
    BAutolock lock(fLock);
    
    while (!fRequestQueue.empty()) {
        HttpConnection* conn = _GetAvailableConnection();
        if (!conn) {
            break; // No available connections
        }
        
        QueuedRequest req = fRequestQueue.top();
        fRequestQueue.pop();
        
        conn->SetBusy();
        fActiveConnections++;
        
        status_t result = conn->SendRequest(req.request);
        if (result != B_OK) {
            conn->SetFailed();
            fActiveConnections--;
            
            // Retry if below retry limit
            if (req.retryCount < 3) {
                req.retryCount++;
                fRequestQueue.push(req);
            } else {
                // Send failure notification
                if (req.replyTarget) {
                    BMessage reply(req.replyWhat);
                    reply.AddInt32("error", result);
                    BMessenger(req.replyTarget).SendMessage(&reply);
                }
                // TODO: Delete BHttpRequest when HTTP API is integrated
            }
        }
    }
}

void
ConnectionPool::_HandleConnectionComplete(HttpConnection* connection, bool success)
{
    if (!connection) {
        return;
    }
    
    fActiveConnections--;
    
    if (success) {
        connection->SetIdle();
        fSuccessfulRequests++;
    } else {
        connection->SetFailed();
        fFailedRequests++;
        
        // If too many failures, mark for cleanup
        if (connection->GetFailureCount() >= 3) {
            LOG_WARNING("ConnectionPool", "Connection %d has failed too many times",
                       connection->GetID());
        }
    }
    
    // Process any queued requests
    _ProcessRequestQueue();
}

void
ConnectionPool::_CleanupFailedConnections()
{
    BAutolock lock(fLock);
    
    // Remove connections with too many failures
    fConnections.erase(
        std::remove_if(fConnections.begin(), fConnections.end(),
            [](const std::unique_ptr<HttpConnection>& conn) {
                return conn->GetFailureCount() >= 3;
            }),
        fConnections.end()
    );
    
    // Ensure minimum connections
    while (fConnections.size() < fMinConnections) {
        HttpConnection* conn = _CreateConnection();
        if (conn) {
            fConnections.push_back(std::unique_ptr<HttpConnection>(conn));
        } else {
            break;
        }
    }
}

void
ConnectionPool::_UpdateStatistics(bigtime_t latency, bool success)
{
    if (success) {
        fTotalLatency += latency;
    }
}

int32
ConnectionPool::_BinarySearchLimit(int32 min, int32 max)
{
    LOG_INFO("ConnectionPool", "Binary searching for optimal limit between %d and %d", min, max);
    
    int32 lastWorking = min;
    
    while (min <= max) {
        int32 mid = min + (max - min) / 2;
        
        // Test this connection limit
        bool success = _TestConnectionLimit(mid);
        
        if (success) {
            lastWorking = mid;
            min = mid + 1; // Try higher
            
            // Also test a bit higher to confirm we're near the limit
            if (_TestConnectionLimit(mid + 2)) {
                min = mid + 2;
            }
        } else {
            max = mid - 1; // Back off
        }
        
        // Add small delay between tests to avoid hammering the server
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
    // Apply backoff factor for safety
    int32 optimal = (int32)(lastWorking * fBackoffFactor);
    optimal = std::max(fMinConnections.load(), optimal);
    
    LOG_INFO("ConnectionPool", "Optimal connection limit: %d (tested up to %d)", 
            optimal, lastWorking);
    
    return optimal;
}