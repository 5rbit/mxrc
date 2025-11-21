#pragma once

#include "HealthCheck.h"
#include <memory>
#include <atomic>
#include <thread>
#include <cstdint>

namespace mxrc {
namespace ha {

/**
 * @brief Health Check HTTP server
 *
 * Production readiness: HTTP server for Kubernetes-style health probes.
 * Provides /health, /health/ready, /health/live, /health/details endpoints.
 *
 * T036-T040: Health Check HTTP API implementation
 */
class HealthCheckServer {
private:
    std::shared_ptr<IHealthCheck> health_check_;
    uint16_t port_;
    std::atomic<bool> running_{false};
    std::thread server_thread_;
    int server_socket_{-1};

    void serverLoop();
    void handleClient(int client_socket);
    std::string buildHttpResponse(const std::string& body,
                                   const std::string& content_type = "application/json",
                                   int status_code = 200,
                                   const std::string& status_text = "OK") const;

    // Endpoint handlers
    std::string handleHealthEndpoint() const;        // T037: GET /health
    std::string handleReadyEndpoint() const;         // T038: GET /health/ready
    std::string handleLiveEndpoint() const;          // T039: GET /health/live
    std::string handleDetailsEndpoint() const;       // T040: GET /health/details

public:
    /**
     * @brief HealthCheckServer constructor
     *
     * @param health_check Health check provider interface
     * @param port Port number (default: 8081)
     */
    explicit HealthCheckServer(std::shared_ptr<IHealthCheck> health_check, uint16_t port = 8081);

    /**
     * @brief Destructor
     */
    ~HealthCheckServer();

    // Prevent copying
    HealthCheckServer(const HealthCheckServer&) = delete;
    HealthCheckServer& operator=(const HealthCheckServer&) = delete;

    /**
     * @brief Start server
     *
     * @return true if successful, false otherwise
     */
    bool start();

    /**
     * @brief Stop server
     */
    void stop();

    /**
     * @brief Check if server is running
     *
     * @return true if running, false otherwise
     */
    bool isRunning() const { return running_; }

    /**
     * @brief Get port number
     *
     * @return Port number
     */
    uint16_t getPort() const { return port_; }
};

} // namespace ha
} // namespace mxrc
