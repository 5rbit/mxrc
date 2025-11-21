// HealthCheckServer.cpp
// Copyright (C) 2025 MXRC Project

#include "HealthCheckServer.h"
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <sstream>
#include <chrono>
#include <iomanip>

namespace mxrc::ha {

HealthCheckServer::HealthCheckServer(std::shared_ptr<IHealthCheck> health_check, uint16_t port)
    : health_check_(std::move(health_check))
    , port_(port) {
}

HealthCheckServer::~HealthCheckServer() {
    stop();
}

bool HealthCheckServer::start() {
    if (running_) {
        spdlog::warn("HealthCheckServer already running on port {}", port_);
        return false;
    }

    // TCP socket creation
    server_socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket_ < 0) {
        spdlog::error("Failed to create socket: {}", strerror(errno));
        return false;
    }

    // SO_REUSEADDR option (allow fast restart)
    int opt = 1;
    if (setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        spdlog::warn("Failed to set SO_REUSEADDR: {}", strerror(errno));
    }

    // Address binding
    struct sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr("127.0.0.1");  // localhost only
    address.sin_port = htons(port_);

    if (bind(server_socket_, (struct sockaddr*)&address, sizeof(address)) < 0) {
        spdlog::error("Failed to bind to port {}: {}", port_, strerror(errno));
        close(server_socket_);
        server_socket_ = -1;
        return false;
    }

    // Start listening
    if (listen(server_socket_, 10) < 0) {
        spdlog::error("Failed to listen on port {}: {}", port_, strerror(errno));
        close(server_socket_);
        server_socket_ = -1;
        return false;
    }

    running_ = true;
    server_thread_ = std::thread(&HealthCheckServer::serverLoop, this);

    spdlog::info("HealthCheckServer started on http://127.0.0.1:{}/health", port_);
    return true;
}

void HealthCheckServer::stop() {
    if (!running_) {
        return;
    }

    running_ = false;

    // Close server socket (unblock accept())
    if (server_socket_ >= 0) {
        shutdown(server_socket_, SHUT_RDWR);
        close(server_socket_);
        server_socket_ = -1;
    }

    // Wait for thread termination
    if (server_thread_.joinable()) {
        server_thread_.join();
    }

    spdlog::info("HealthCheckServer stopped");
}

void HealthCheckServer::serverLoop() {
    spdlog::debug("HealthCheckServer loop started");

    while (running_) {
        struct sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);

        // Wait for client connection
        int client_socket = accept(server_socket_, (struct sockaddr*)&client_addr, &client_len);

        if (client_socket < 0) {
            if (running_) {
                spdlog::error("Failed to accept connection: {}", strerror(errno));
            }
            break;
        }

        // Handle client request
        handleClient(client_socket);
        close(client_socket);
    }

    spdlog::debug("HealthCheckServer loop stopped");
}

void HealthCheckServer::handleClient(int client_socket) {
    // Read HTTP request (simple parsing)
    char buffer[4096];
    ssize_t bytes_read = recv(client_socket, buffer, sizeof(buffer) - 1, 0);

    if (bytes_read < 0) {
        spdlog::error("Failed to read from client: {}", strerror(errno));
        return;
    }

    buffer[bytes_read] = '\0';
    std::string request(buffer);

    // Parse HTTP method and path
    std::istringstream iss(request);
    std::string method, path, version;
    iss >> method >> path >> version;

    spdlog::debug("Received request: {} {}", method, path);

    std::string response;

    // Route handling
    if (method == "GET") {
        if (path == "/health") {
            // T037: GET /health endpoint
            std::string body = handleHealthEndpoint();
            auto status = health_check_->getHealthStatus();
            int status_code = (status.status == HealthStatus::HEALTHY) ? 200 : 503;
            std::string status_text = (status.status == HealthStatus::HEALTHY) ? "OK" : "Service Unavailable";
            response = buildHttpResponse(body, "application/json", status_code, status_text);
        } else if (path == "/health/ready") {
            // T038: GET /health/ready endpoint (Readiness probe)
            std::string body = handleReadyEndpoint();
            int status_code = health_check_->isReady() ? 200 : 503;
            std::string status_text = health_check_->isReady() ? "OK" : "Service Unavailable";
            response = buildHttpResponse(body, "application/json", status_code, status_text);
        } else if (path == "/health/live") {
            // T039: GET /health/live endpoint (Liveness probe)
            std::string body = handleLiveEndpoint();
            int status_code = health_check_->isAlive() ? 200 : 503;
            std::string status_text = health_check_->isAlive() ? "OK" : "Service Unavailable";
            response = buildHttpResponse(body, "application/json", status_code, status_text);
        } else if (path == "/health/details") {
            // T040: GET /health/details endpoint (Detailed diagnostics)
            std::string body = handleDetailsEndpoint();
            response = buildHttpResponse(body, "application/json");
        } else if (path == "/") {
            // Root path: simple guide page
            std::string body = R"(
<html>
<head><title>MXRC Health Check</title></head>
<body>
<h1>MXRC Health Check Endpoints</h1>
<ul>
<li><a href="/health">/health</a> - Overall health status</li>
<li><a href="/health/ready">/health/ready</a> - Readiness probe (Kubernetes)</li>
<li><a href="/health/live">/health/live</a> - Liveness probe (Kubernetes)</li>
<li><a href="/health/details">/health/details</a> - Detailed diagnostics</li>
</ul>
</body>
</html>
)";
            response = buildHttpResponse(body, "text/html");
        } else {
            // 404 Not Found
            std::string body = R"({"error":"Not Found","path":")" + path + R"("})";
            response = buildHttpResponse(body, "application/json", 404, "Not Found");
        }
    } else {
        // 405 Method Not Allowed
        std::string body = R"({"error":"Method Not Allowed","method":")" + method + R"("})";
        response = buildHttpResponse(body, "application/json", 405, "Method Not Allowed");
    }

    // Send response
    ssize_t bytes_sent = send(client_socket, response.c_str(), response.size(), 0);
    if (bytes_sent < 0) {
        spdlog::error("Failed to send response: {}", strerror(errno));
    }
}

std::string HealthCheckServer::buildHttpResponse(const std::string& body,
                                                   const std::string& content_type,
                                                   int status_code,
                                                   const std::string& status_text) const {
    std::ostringstream oss;
    oss << "HTTP/1.1 " << status_code << " " << status_text << "\r\n";
    oss << "Content-Type: " << content_type << "\r\n";
    oss << "Content-Length: " << body.size() << "\r\n";
    oss << "Connection: close\r\n";
    oss << "\r\n";
    oss << body;
    return oss.str();
}

// T037: GET /health endpoint implementation
std::string HealthCheckServer::handleHealthEndpoint() const {
    auto status = health_check_->getHealthStatus();

    nlohmann::json j;
    j["status"] = healthStatusToString(status.status);
    j["process_name"] = status.process_name;
    j["pid"] = status.pid;

    // Convert last_heartbeat to ISO 8601 string
    auto time_t_val = std::chrono::system_clock::to_time_t(status.last_heartbeat);
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time_t_val), "%Y-%m-%dT%H:%M:%SZ");
    j["last_heartbeat"] = oss.str();

    j["response_time_ms"] = status.response_time_ms;
    j["cpu_usage_percent"] = status.cpu_usage_percent;
    j["memory_usage_mb"] = status.memory_usage_mb;
    j["deadline_miss_count"] = status.deadline_miss_count;
    j["restart_count"] = status.restart_count;

    if (!status.error_message.empty()) {
        j["error_message"] = status.error_message;
    }

    return j.dump(2);  // Pretty print with 2-space indent
}

// T038: GET /health/ready endpoint implementation
std::string HealthCheckServer::handleReadyEndpoint() const {
    bool ready = health_check_->isReady();
    auto status = health_check_->getHealthStatus();

    nlohmann::json j;
    j["ready"] = ready;
    j["status"] = healthStatusToString(status.status);
    j["process_name"] = status.process_name;

    if (!ready) {
        j["reason"] = "Process not ready to accept requests";
        if (!status.error_message.empty()) {
            j["error"] = status.error_message;
        }
    }

    return j.dump(2);
}

// T039: GET /health/live endpoint implementation
std::string HealthCheckServer::handleLiveEndpoint() const {
    bool alive = health_check_->isAlive();
    auto status = health_check_->getHealthStatus();

    nlohmann::json j;
    j["alive"] = alive;
    j["status"] = healthStatusToString(status.status);
    j["process_name"] = status.process_name;
    j["pid"] = status.pid;

    if (!alive) {
        j["reason"] = "Process not responding";
        if (!status.error_message.empty()) {
            j["error"] = status.error_message;
        }
    }

    return j.dump(2);
}

// T040: GET /health/details endpoint implementation
std::string HealthCheckServer::handleDetailsEndpoint() const {
    auto status = health_check_->getHealthStatus();

    nlohmann::json j;
    j["process_name"] = status.process_name;
    j["pid"] = status.pid;
    j["status"] = healthStatusToString(status.status);
    j["is_healthy"] = health_check_->isHealthy();
    j["is_ready"] = health_check_->isReady();
    j["is_alive"] = health_check_->isAlive();

    // Convert last_heartbeat to ISO 8601 string
    auto time_t_val = std::chrono::system_clock::to_time_t(status.last_heartbeat);
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time_t_val), "%Y-%m-%dT%H:%M:%SZ");
    j["last_heartbeat"] = oss.str();

    // Performance metrics
    nlohmann::json perf;
    perf["response_time_ms"] = status.response_time_ms;
    perf["cpu_usage_percent"] = status.cpu_usage_percent;
    perf["memory_usage_mb"] = status.memory_usage_mb;
    perf["deadline_miss_count"] = status.deadline_miss_count;
    j["performance"] = perf;

    // Restart tracking
    nlohmann::json restart;
    restart["restart_count"] = status.restart_count;
    j["restart"] = restart;

    // Error information (if any)
    if (!status.error_message.empty()) {
        j["error_message"] = status.error_message;
    }

    // Health assessment
    nlohmann::json assessment;
    if (status.status == HealthStatus::HEALTHY) {
        assessment["level"] = "good";
        assessment["message"] = "Process is operating normally";
    } else if (status.status == HealthStatus::DEGRADED) {
        assessment["level"] = "warning";
        assessment["message"] = "Process is experiencing performance degradation";
    } else if (status.status == HealthStatus::UNHEALTHY) {
        assessment["level"] = "critical";
        assessment["message"] = "Process is unhealthy and may require restart";
    } else if (status.status == HealthStatus::STARTING) {
        assessment["level"] = "info";
        assessment["message"] = "Process is starting up";
    } else if (status.status == HealthStatus::STOPPING) {
        assessment["level"] = "info";
        assessment["message"] = "Process is shutting down";
    } else if (status.status == HealthStatus::STOPPED) {
        assessment["level"] = "info";
        assessment["message"] = "Process is stopped";
    }
    j["assessment"] = assessment;

    return j.dump(2);  // Pretty print with 2-space indent
}

} // namespace mxrc::ha
