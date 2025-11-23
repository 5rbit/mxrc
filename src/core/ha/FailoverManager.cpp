// FailoverManager.cpp
// Copyright (C) 2025 MXRC Project

#include "FailoverManager.h"
#include "StateCheckpoint.h"
#include <spdlog/spdlog.h>
#include <fstream>
#include <nlohmann/json.hpp>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

namespace mxrc::ha {

/**
 * @brief FailoverManagerImpl implementation
 *
 * Manages process failover with restart policy and checkpoint-based recovery.
 * Implements IFailoverManager interface.
 */
class FailoverManagerImpl : public IFailoverManager {
private:
    FailoverPolicy policy_;
    std::shared_ptr<IStateCheckpoint> checkpoint_manager_;

    // Restart tracking
    struct RestartInfo {
        uint32_t count;
        std::chrono::system_clock::time_point window_start;
    };
    std::map<std::string, RestartInfo> restart_tracking_;
    mutable std::mutex restart_mutex_;

    std::atomic<bool> running_{false};

    // Clean up expired restart tracking entries
    void cleanupRestartTracking() {
        std::lock_guard<std::mutex> lock(restart_mutex_);
        auto now = std::chrono::system_clock::now();
        auto window = std::chrono::seconds(policy_.restart_window_sec);

        auto it = restart_tracking_.begin();
        while (it != restart_tracking_.end()) {
            auto age = now - it->second.window_start;
            if (age > window) {
                it = restart_tracking_.erase(it);
            } else {
                ++it;
            }
        }
    }

public:
    explicit FailoverManagerImpl(const FailoverPolicy& policy,
                                 std::shared_ptr<IStateCheckpoint> checkpoint_manager = nullptr)
        : policy_(policy)
        , checkpoint_manager_(std::move(checkpoint_manager)) {

        if (!policy_.isValid()) {
            throw std::invalid_argument("Invalid failover policy");
        }
    }

    // T051: Start failover monitoring
    bool start() override {
        if (running_) {
            spdlog::warn("FailoverManager already running for {}", policy_.process_name);
            return false;
        }

        running_ = true;
        spdlog::info("FailoverManager started for process: {}", policy_.process_name);
        return true;
    }

    // T051: Stop failover monitoring
    void stop() override {
        if (!running_) {
            return;
        }

        running_ = false;
        spdlog::info("FailoverManager stopped for process: {}", policy_.process_name);
    }

    // T051: Handle process failure
    void handleProcessFailure(const std::string& process_name) override {
        if (!running_) {
            spdlog::warn("FailoverManager not running, ignoring failure for {}", process_name);
            return;
        }

        if (process_name != policy_.process_name) {
            spdlog::warn("Process name mismatch: expected {}, got {}",
                        policy_.process_name, process_name);
            return;
        }

        spdlog::error("Process failure detected: {}", process_name);

        // T052: Check if restart is allowed
        if (!canRestart(process_name)) {
            spdlog::error("Restart limit exceeded for process {}, giving up", process_name);
            // TODO: Notify external monitoring system
            return;
        }

        // T053: Trigger restart with state recovery
        bool recover_state = policy_.enable_state_recovery;
        if (triggerRestart(process_name, recover_state)) {
            spdlog::info("Successfully triggered restart for process {}", process_name);
        } else {
            spdlog::error("Failed to trigger restart for process {}", process_name);
        }
    }

    // T053: Trigger process restart
    bool triggerRestart(const std::string& process_name, bool recover_state = true) override {
        if (process_name != policy_.process_name) {
            spdlog::error("Process name mismatch in triggerRestart");
            return false;
        }

        spdlog::info("Triggering restart for process {} (recover_state: {})",
                    process_name, recover_state);

        // T052: Update restart count
        {
            std::lock_guard<std::mutex> lock(restart_mutex_);
            auto& info = restart_tracking_[process_name];
            auto now = std::chrono::system_clock::now();
            auto window = std::chrono::seconds(policy_.restart_window_sec);

            // Reset window if expired
            if (now - info.window_start > window) {
                info.count = 0;
                info.window_start = now;
            }

            info.count++;
            spdlog::info("Restart count for {}: {}/{} (window: {}s)",
                        process_name, info.count, policy_.max_restart_count,
                        policy_.restart_window_sec);
        }

        // T053: Apply restart delay
        if (policy_.restart_delay_ms > 0) {
            spdlog::debug("Applying restart delay: {}ms", policy_.restart_delay_ms);
            std::this_thread::sleep_for(std::chrono::milliseconds(policy_.restart_delay_ms));
        }

        // T054: Load checkpoint if state recovery is enabled
        StateCheckpoint checkpoint;
        bool checkpoint_loaded = false;

        if (recover_state && policy_.enable_state_recovery && checkpoint_manager_) {
            try {
                auto checkpoints = checkpoint_manager_->listCheckpoints();
                if (!checkpoints.empty()) {
                    // Load the most recent checkpoint
                    std::string latest_checkpoint_id = checkpoints.back();
                    checkpoint = checkpoint_manager_->loadCheckpoint(latest_checkpoint_id);
                    checkpoint_loaded = true;

                    spdlog::info("Loaded checkpoint {} for recovery (created: {})",
                                checkpoint.checkpoint_id,
                                std::chrono::system_clock::to_time_t(checkpoint.timestamp));
                } else {
                    spdlog::warn("No checkpoints available for recovery");
                }
            } catch (const std::exception& e) {
                spdlog::error("Failed to load checkpoint for recovery: {}", e.what());
                // Continue with restart without state recovery
            }
        }

        // TODO: Actual process restart implementation
        // This would typically involve:
        // 1. Fork/exec new process
        // 2. If checkpoint_loaded, restore state from checkpoint
        // 3. Monitor new process startup

        spdlog::info("Process restart would be executed here (not implemented yet)");

        // For now, just return success
        return true;
    }

    // T052: Check if restart is allowed
    bool canRestart(const std::string& process_name) const override {
        std::lock_guard<std::mutex> lock(restart_mutex_);

        auto it = restart_tracking_.find(process_name);
        if (it == restart_tracking_.end()) {
            return true;  // No restart history, allowed
        }

        auto now = std::chrono::system_clock::now();
        auto window = std::chrono::seconds(policy_.restart_window_sec);
        auto age = now - it->second.window_start;

        // If window expired, restart is allowed
        if (age > window) {
            return true;
        }

        // Check if within limit
        return it->second.count < policy_.max_restart_count;
    }

    // T052: Get current restart count
    uint32_t getRestartCount(const std::string& process_name) const override {
        std::lock_guard<std::mutex> lock(restart_mutex_);

        auto it = restart_tracking_.find(process_name);
        if (it == restart_tracking_.end()) {
            return 0;
        }

        auto now = std::chrono::system_clock::now();
        auto window = std::chrono::seconds(policy_.restart_window_sec);
        auto age = now - it->second.window_start;

        // If window expired, return 0
        if (age > window) {
            return 0;
        }

        return it->second.count;
    }

    // T052: Reset restart count
    void resetRestartCount(const std::string& process_name) override {
        std::lock_guard<std::mutex> lock(restart_mutex_);
        restart_tracking_.erase(process_name);
        spdlog::info("Reset restart count for process: {}", process_name);
    }

    // T055: Load failover policy from JSON file
    bool loadPolicy(const std::string& config_path) override {
        try {
            std::ifstream file(config_path);
            if (!file.is_open()) {
                spdlog::error("Failed to open failover policy file: {}", config_path);
                return false;
            }

            nlohmann::json j;
            file >> j;

            // Parse policy fields
            policy_.process_name = j.value("process_name", policy_.process_name);
            policy_.health_check_interval_ms = j.value("health_check_interval_ms", policy_.health_check_interval_ms);
            policy_.health_check_timeout_ms = j.value("health_check_timeout_ms", policy_.health_check_timeout_ms);
            policy_.failure_threshold = j.value("failure_threshold", policy_.failure_threshold);
            policy_.restart_delay_ms = j.value("restart_delay_ms", policy_.restart_delay_ms);
            policy_.max_restart_count = j.value("max_restart_count", policy_.max_restart_count);
            policy_.restart_window_sec = j.value("restart_window_sec", policy_.restart_window_sec);
            policy_.enable_state_recovery = j.value("enable_state_recovery", policy_.enable_state_recovery);
            policy_.checkpoint_interval_sec = j.value("checkpoint_interval_sec", policy_.checkpoint_interval_sec);
            policy_.enable_leader_election = j.value("enable_leader_election", policy_.enable_leader_election);

            // Validate loaded policy
            if (!policy_.isValid()) {
                spdlog::error("Loaded policy is invalid");
                return false;
            }

            spdlog::info("Loaded failover policy from: {}", config_path);
            spdlog::debug("Policy: process={}, failure_threshold={}, max_restart={}, window={}s",
                         policy_.process_name, policy_.failure_threshold,
                         policy_.max_restart_count, policy_.restart_window_sec);

            return true;

        } catch (const std::exception& e) {
            spdlog::error("Failed to load failover policy: {}", e.what());
            return false;
        }
    }

    // Get current failover policy
    const FailoverPolicy& getPolicy() const override {
        return policy_;
    }
};

// Factory function to create FailoverManager
std::unique_ptr<IFailoverManager> createFailoverManager(
    const FailoverPolicy& policy,
    std::shared_ptr<IStateCheckpoint> checkpoint_manager) {

    return std::make_unique<FailoverManagerImpl>(policy, checkpoint_manager);
}

} // namespace mxrc::ha
