#include "ScheduleCalculator.h"
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <algorithm>
#include <numeric>

namespace mxrc {
namespace core {
namespace rt {
namespace util {

uint32_t gcd(uint32_t a, uint32_t b) {
    while (b != 0) {
        uint32_t temp = b;
        b = a % b;
        a = temp;
    }
    return a;
}

uint32_t lcm(uint32_t a, uint32_t b) {
    if (a == 0 || b == 0) {
        return 0;
    }
    return (a / gcd(a, b)) * b;  // Prevent overflow by dividing first
}

ScheduleParams calculate(const std::vector<uint32_t>& periods_ms) {
    if (periods_ms.empty()) {
        throw std::invalid_argument("Period list cannot be empty");
    }

    // Check for zero or negative periods
    for (auto period : periods_ms) {
        if (period == 0) {
            throw std::invalid_argument("Period cannot be zero");
        }
    }

    // Calculate GCD of all periods (minor cycle)
    uint32_t minor = periods_ms[0];
    for (size_t i = 1; i < periods_ms.size(); ++i) {
        minor = gcd(minor, periods_ms[i]);
    }

    // Calculate LCM of all periods (major cycle)
    uint32_t major = periods_ms[0];
    for (size_t i = 1; i < periods_ms.size(); ++i) {
        major = lcm(major, periods_ms[i]);

        // Check if LCM exceeds maximum
        if (major > MAX_MAJOR_CYCLE_MS) {
            spdlog::error("Major cycle ({}ms) exceeds maximum ({}ms). "
                          "Consider using periods that are multiples of each other.",
                          major, MAX_MAJOR_CYCLE_MS);
            throw std::invalid_argument("Major cycle exceeds maximum allowed value");
        }
    }

    uint32_t num_slots = major / minor;

    spdlog::info("Schedule calculated: minor_cycle={}ms, major_cycle={}ms, slots={}",
                 minor, major, num_slots);

    return ScheduleParams{minor, major, num_slots};
}

} // namespace util
} // namespace rt
} // namespace core
} // namespace mxrc
