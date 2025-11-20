// MetricsServer.h - 간단한 HTTP 메트릭 서버
// Copyright (C) 2025 MXRC Project

#ifndef MXRC_CORE_MONITORING_METRICSSERVER_H
#define MXRC_CORE_MONITORING_METRICSSERVER_H

#include "MetricsCollector.h"
#include <memory>
#include <thread>
#include <atomic>
#include <string>

namespace mxrc::core::monitoring {

/**
 * @brief 간단한 HTTP 메트릭 서버
 *
 * /metrics 엔드포인트에서 Prometheus 포맷으로 메트릭 제공
 */
class MetricsServer {
private:
    std::shared_ptr<MetricsCollector> collector_;
    uint16_t port_;
    std::atomic<bool> running_{false};
    std::thread server_thread_;
    int server_socket_{-1};

    void serverLoop();
    void handleClient(int client_socket);
    std::string buildHttpResponse(const std::string& body, const std::string& content_type = "text/plain") const;

public:
    /**
     * @brief MetricsServer 생성자
     *
     * @param collector 메트릭 수집기
     * @param port 포트 번호 (기본값: 9100)
     */
    explicit MetricsServer(std::shared_ptr<MetricsCollector> collector, uint16_t port = 9100);

    /**
     * @brief 소멸자
     */
    ~MetricsServer();

    // 복사 금지
    MetricsServer(const MetricsServer&) = delete;
    MetricsServer& operator=(const MetricsServer&) = delete;

    /**
     * @brief 서버 시작
     *
     * @return true이면 성공, false이면 실패
     */
    bool start();

    /**
     * @brief 서버 중지
     */
    void stop();

    /**
     * @brief 실행 상태 확인
     *
     * @return true이면 실행 중, false이면 정지 상태
     */
    bool isRunning() const { return running_; }

    /**
     * @brief 포트 번호 반환
     *
     * @return 포트 번호
     */
    uint16_t getPort() const { return port_; }
};

} // namespace mxrc::core::monitoring

#endif // MXRC_CORE_MONITORING_METRICSSERVER_H
