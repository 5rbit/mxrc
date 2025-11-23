// MetricsServer.cpp
// Copyright (C) 2025 MXRC Project

#include "MetricsServer.h"
#include <spdlog/spdlog.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <sstream>

namespace mxrc::core::monitoring {

MetricsServer::MetricsServer(std::shared_ptr<MetricsCollector> collector, uint16_t port)
    : collector_(std::move(collector))
    , port_(port) {
}

MetricsServer::~MetricsServer() {
    stop();
}

bool MetricsServer::start() {
    if (running_) {
        spdlog::warn("MetricsServer already running on port {}", port_);
        return false;
    }

    // TCP 소켓 생성
    server_socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket_ < 0) {
        spdlog::error("Failed to create socket: {}", strerror(errno));
        return false;
    }

    // SO_REUSEADDR 설정 (빠른 재시작 허용)
    int opt = 1;
    if (setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        spdlog::warn("Failed to set SO_REUSEADDR: {}", strerror(errno));
    }

    // 주소 바인딩
    struct sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr("127.0.0.1");  // localhost만
    address.sin_port = htons(port_);

    if (bind(server_socket_, (struct sockaddr*)&address, sizeof(address)) < 0) {
        spdlog::error("Failed to bind to port {}: {}", port_, strerror(errno));
        close(server_socket_);
        server_socket_ = -1;
        return false;
    }

    // Listen 시작
    if (listen(server_socket_, 10) < 0) {
        spdlog::error("Failed to listen on port {}: {}", port_, strerror(errno));
        close(server_socket_);
        server_socket_ = -1;
        return false;
    }

    running_ = true;
    server_thread_ = std::thread(&MetricsServer::serverLoop, this);

    spdlog::info("MetricsServer started on http://127.0.0.1:{}/metrics", port_);
    return true;
}

void MetricsServer::stop() {
    if (!running_) {
        return;
    }

    running_ = false;

    // 서버 소켓 닫기 (accept() 블로킹 해제)
    if (server_socket_ >= 0) {
        shutdown(server_socket_, SHUT_RDWR);
        close(server_socket_);
        server_socket_ = -1;
    }

    // 스레드 종료 대기
    if (server_thread_.joinable()) {
        server_thread_.join();
    }

    spdlog::info("MetricsServer stopped");
}

void MetricsServer::serverLoop() {
    spdlog::debug("MetricsServer loop started");

    while (running_) {
        struct sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);

        // 클라이언트 연결 대기
        int client_socket = accept(server_socket_, (struct sockaddr*)&client_addr, &client_len);

        if (client_socket < 0) {
            if (running_) {
                spdlog::error("Failed to accept connection: {}", strerror(errno));
            }
            break;
        }

        // 클라이언트 요청 처리
        handleClient(client_socket);
        close(client_socket);
    }

    spdlog::debug("MetricsServer loop stopped");
}

void MetricsServer::handleClient(int client_socket) {
    // HTTP 요청 읽기 (간단한 파싱)
    char buffer[4096];
    ssize_t bytes_read = recv(client_socket, buffer, sizeof(buffer) - 1, 0);

    if (bytes_read < 0) {
        spdlog::error("Failed to read from client: {}", strerror(errno));
        return;
    }

    buffer[bytes_read] = '\0';
    std::string request(buffer);

    // HTTP 메서드와 경로 파싱
    std::istringstream iss(request);
    std::string method, path, version;
    iss >> method >> path >> version;

    spdlog::debug("Received request: {} {}", method, path);

    std::string response;

    // /metrics 경로만 처리
    if (method == "GET" && path == "/metrics") {
        // Prometheus 포맷으로 메트릭 내보내기
        std::string metrics = collector_->exportPrometheus();
        response = buildHttpResponse(metrics);
    } else if (method == "GET" && path == "/") {
        // 루트 경로: 간단한 안내 페이지
        std::string body = R"(
<html>
<head><title>MXRC Metrics</title></head>
<body>
<h1>MXRC Metrics Exporter</h1>
<p>Metrics are available at <a href="/metrics">/metrics</a></p>
</body>
</html>
)";
        response = buildHttpResponse(body, "text/html");
    } else {
        // 404 Not Found
        std::string body = "404 Not Found\n";
        response = "HTTP/1.1 404 Not Found\r\n";
        response += "Content-Type: text/plain\r\n";
        response += "Content-Length: " + std::to_string(body.size()) + "\r\n";
        response += "Connection: close\r\n\r\n";
        response += body;
    }

    // 응답 전송
    ssize_t bytes_sent = send(client_socket, response.c_str(), response.size(), 0);
    if (bytes_sent < 0) {
        spdlog::error("Failed to send response: {}", strerror(errno));
    }
}

std::string MetricsServer::buildHttpResponse(const std::string& body, const std::string& content_type) const {
    std::ostringstream oss;
    oss << "HTTP/1.1 200 OK\r\n";
    oss << "Content-Type: " << content_type << "\r\n";
    oss << "Content-Length: " << body.size() << "\r\n";
    oss << "Connection: close\r\n";
    oss << "\r\n";
    oss << body;
    return oss.str();
}

} // namespace mxrc::core::monitoring
