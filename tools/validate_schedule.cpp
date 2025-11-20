#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "../src/core/rt/util/ScheduleCalculator.h"

using json = nlohmann::json;
using namespace mxrc::core::rt::util;

// CPU 사용률 임계값
constexpr double MAX_CPU_UTILIZATION = 0.70;  // 70%
constexpr uint32_t WARNING_MAJOR_CYCLE_MS = 1000;

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <config.json>\n";
        std::cerr << "Example: " << argv[0] << " config/rt_schedule.json\n";
        return 1;
    }

    const std::string config_path = argv[1];
    bool has_error = false;
    bool has_warning = false;

    try {
        // JSON 파일 읽기
        std::ifstream config_file(config_path);
        if (!config_file.is_open()) {
            std::cerr << "❌ Error: Cannot open config file: " << config_path << "\n";
            return 1;
        }

        json config;
        config_file >> config;
        config_file.close();

        // periods_ms 배열 검증
        if (!config.contains("periods_ms") || !config["periods_ms"].is_array()) {
            std::cerr << "❌ Error: 'periods_ms' field is missing or not an array\n";
            return 1;
        }

        std::vector<uint32_t> periods_ms = config["periods_ms"].get<std::vector<uint32_t>>();
        if (periods_ms.empty()) {
            std::cerr << "❌ Error: 'periods_ms' array is empty\n";
            return 1;
        }

        // GCD/LCM 계산
        ScheduleParams params;
        try {
            params = calculate(periods_ms);
        } catch (const std::exception& e) {
            std::cerr << "❌ Error: Schedule calculation failed: " << e.what() << "\n";
            return 1;
        }

        std::cout << "Schedule Parameters:\n";
        std::cout << "  Minor cycle: " << params.minor_cycle_ms << " ms\n";
        std::cout << "  Major cycle: " << params.major_cycle_ms << " ms\n";
        std::cout << "  Number of slots: " << params.num_slots << "\n\n";

        // Major cycle 경고
        if (params.major_cycle_ms > WARNING_MAJOR_CYCLE_MS) {
            std::cout << "⚠️  Warning: Major cycle (" << params.major_cycle_ms
                      << " ms) exceeds " << WARNING_MAJOR_CYCLE_MS << " ms\n";
            has_warning = true;
        }

        // Actions 검증
        if (!config.contains("actions") || !config["actions"].is_array()) {
            std::cout << "⚠️  Warning: 'actions' field is missing or not an array\n";
            has_warning = true;
        } else {
            const auto& actions = config["actions"];
            double total_utilization = 0.0;

            std::cout << "Action Validation:\n";
            std::cout << "  Total actions: " << actions.size() << "\n\n";

            for (const auto& action : actions) {
                // 필수 필드 검증
                if (!action.contains("name") || !action.contains("period_ms") ||
                    !action.contains("wcet_us")) {
                    std::cerr << "❌ Error: Action missing required fields (name, period_ms, wcet_us)\n";
                    has_error = true;
                    continue;
                }

                std::string name = action["name"].get<std::string>();
                uint32_t period_ms = action["period_ms"].get<uint32_t>();
                uint32_t wcet_us = action["wcet_us"].get<uint32_t>();

                // WCET > Period 검증
                if (wcet_us > period_ms * 1000) {
                    std::cerr << "❌ Error: Action '" << name << "' WCET (" << wcet_us
                              << " μs) exceeds period (" << period_ms << " ms)\n";
                    has_error = true;
                }

                // Period가 minor의 배수인지 검증
                if (period_ms % params.minor_cycle_ms != 0) {
                    std::cerr << "❌ Error: Action '" << name << "' period (" << period_ms
                              << " ms) is not a multiple of minor cycle ("
                              << params.minor_cycle_ms << " ms)\n";
                    has_error = true;
                }

                // CPU 사용률 계산
                double utilization = (wcet_us / 1000.0) / period_ms;
                total_utilization += utilization;

                std::cout << "  - " << name << ":\n";
                std::cout << "      Period: " << period_ms << " ms\n";
                std::cout << "      WCET: " << wcet_us << " μs\n";
                std::cout << "      Utilization: " << (utilization * 100) << "%\n";
            }

            std::cout << "\nCPU Utilization:\n";
            std::cout << "  Total: " << (total_utilization * 100) << "%\n";
            std::cout << "  Threshold: " << (MAX_CPU_UTILIZATION * 100) << "%\n";

            if (total_utilization > MAX_CPU_UTILIZATION) {
                std::cerr << "\n❌ Error: CPU utilization (" << (total_utilization * 100)
                          << "%) exceeds threshold (" << (MAX_CPU_UTILIZATION * 100) << "%)\n";
                std::cerr << "   System may not be schedulable!\n";
                has_error = true;
            } else {
                std::cout << "\n✅ CPU utilization is within acceptable range\n";
            }
        }

        // 최종 결과
        std::cout << "\n";
        if (has_error) {
            std::cout << "❌ Schedule validation FAILED\n";
            return 1;
        } else if (has_warning) {
            std::cout << "⚠️  Schedule is VALID with warnings\n";
            return 0;
        } else {
            std::cout << "✅ Schedule is VALID\n";
            return 0;
        }

    } catch (const json::exception& e) {
        std::cerr << "❌ JSON parsing error: " << e.what() << "\n";
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << "\n";
        return 1;
    }
}
