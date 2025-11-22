#!/usr/bin/env python3
"""
MXRC Prometheus Exporter

User Story 5: systemd 메트릭을 Prometheus 형식으로 노출

이 스크립트는:
1. systemctl show로 메트릭 수집
2. Prometheus exposition format으로 변환
3. HTTP 엔드포인트 (/metrics) 제공
"""

import subprocess
import time
from http.server import BaseHTTPRequestHandler, HTTPServer
from typing import Dict, List


class SystemdMetricsCollector:
    """systemd 서비스 메트릭 수집"""

    def __init__(self, services: List[str]):
        self.services = services

    def get_service_metrics(self, service_name: str) -> Dict[str, str]:
        """서비스 메트릭 조회"""
        properties = [
            "ActiveState",
            "SubState",
            "CPUUsageNSec",
            "MemoryCurrent",
            "NRestarts",
            "LoadState",
        ]

        cmd = ["systemctl", "show", service_name, f"--property={','.join(properties)}"]

        try:
            result = subprocess.run(
                cmd, capture_output=True, text=True, timeout=5, check=True
            )
            metrics = {}
            for line in result.stdout.strip().split("\n"):
                if "=" in line:
                    key, value = line.split("=", 1)
                    metrics[key] = value
            return metrics
        except subprocess.SubprocessError:
            return {}

    def collect_all_metrics(self) -> Dict[str, Dict[str, str]]:
        """모든 서비스 메트릭 수집"""
        all_metrics = {}
        for service in self.services:
            all_metrics[service] = self.get_service_metrics(service)
        return all_metrics


class PrometheusFormatter:
    """Prometheus exposition format 변환"""

    @staticmethod
    def format_metrics(metrics_data: Dict[str, Dict[str, str]]) -> str:
        """메트릭을 Prometheus 형식으로 변환"""
        output = []

        # mxrc_service_state
        output.append("# HELP mxrc_service_state Service state (1=active, 0=inactive)")
        output.append("# TYPE mxrc_service_state gauge")
        for service, metrics in metrics_data.items():
            state = metrics.get("ActiveState", "unknown")
            value = 1 if state == "active" else 0
            service_label = service.replace(".service", "")
            output.append(f'mxrc_service_state{{service="{service_label}"}} {value}')

        # mxrc_cpu_usage_seconds_total
        output.append("# HELP mxrc_cpu_usage_seconds_total Total CPU time in seconds")
        output.append("# TYPE mxrc_cpu_usage_seconds_total counter")
        for service, metrics in metrics_data.items():
            cpu_ns = metrics.get("CPUUsageNSec", "0")
            try:
                cpu_seconds = int(cpu_ns) / 1_000_000_000
            except ValueError:
                cpu_seconds = 0.0
            service_label = service.replace(".service", "")
            output.append(
                f'mxrc_cpu_usage_seconds_total{{service="{service_label}"}} {cpu_seconds:.6f}'
            )

        # mxrc_memory_bytes
        output.append("# HELP mxrc_memory_bytes Memory usage in bytes")
        output.append("# TYPE mxrc_memory_bytes gauge")
        for service, metrics in metrics_data.items():
            memory = metrics.get("MemoryCurrent", "0")
            try:
                memory_bytes = int(memory)
            except ValueError:
                memory_bytes = 0
            service_label = service.replace(".service", "")
            output.append(f'mxrc_memory_bytes{{service="{service_label}"}} {memory_bytes}')

        # mxrc_restart_count_total
        output.append("# HELP mxrc_restart_count_total Number of service restarts")
        output.append("# TYPE mxrc_restart_count_total counter")
        for service, metrics in metrics_data.items():
            restarts = metrics.get("NRestarts", "0")
            try:
                restart_count = int(restarts)
            except ValueError:
                restart_count = 0
            service_label = service.replace(".service", "")
            output.append(
                f'mxrc_restart_count_total{{service="{service_label}"}} {restart_count}'
            )

        return "\n".join(output) + "\n"


class MetricsHandler(BaseHTTPRequestHandler):
    """HTTP 요청 핸들러"""

    collector: SystemdMetricsCollector = None

    def do_GET(self):
        """GET 요청 처리"""
        if self.path == "/metrics":
            # 메트릭 수집
            metrics_data = self.collector.collect_all_metrics()

            # Prometheus 형식으로 변환
            prometheus_output = PrometheusFormatter.format_metrics(metrics_data)

            # HTTP 응답
            self.send_response(200)
            self.send_header("Content-Type", "text/plain; version=0.0.4")
            self.end_headers()
            self.wfile.write(prometheus_output.encode("utf-8"))
        elif self.path == "/health":
            # Health check endpoint
            self.send_response(200)
            self.send_header("Content-Type", "text/plain")
            self.end_headers()
            self.wfile.write(b"OK\n")
        else:
            self.send_response(404)
            self.end_headers()

    def log_message(self, format, *args):
        """로그 메시지 (조용히)"""
        pass  # 로그 출력 비활성화


def run_server(port: int, services: List[str]):
    """HTTP 서버 실행"""
    # Collector 설정
    MetricsHandler.collector = SystemdMetricsCollector(services)

    # HTTP 서버 시작
    server_address = ("127.0.0.1", port)  # localhost만 허용
    httpd = HTTPServer(server_address, MetricsHandler)

    print(f"MXRC Prometheus Exporter started on http://127.0.0.1:{port}/metrics")
    print(f"Monitoring services: {', '.join(services)}")

    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nShutting down server...")
        httpd.shutdown()


if __name__ == "__main__":
    # 모니터링할 서비스 목록
    SERVICES = [
        "mxrc-rt.service",
        "mxrc-nonrt.service",
    ]

    # 포트 설정 (기본 9100)
    PORT = 9100

    run_server(PORT, SERVICES)
