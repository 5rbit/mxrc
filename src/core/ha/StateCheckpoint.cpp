// StateCheckpoint.cpp
// Copyright (C) 2025 MXRC Project

#include "StateCheckpoint.h"
#include <spdlog/spdlog.h>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <random>
#include <ctime>

namespace mxrc::ha {

/**
 * @brief StateCheckpointManager implementation
 *
 * Manages checkpoint creation, storage, and recovery for RT processes.
 * Implements IStateCheckpoint interface.
 */
class StateCheckpointManager : public IStateCheckpoint {
private:
    std::string process_name_;
    std::filesystem::path checkpoint_dir_;
    size_t max_checkpoints_;
    size_t retention_hours_;

    // Generate UUID for checkpoint ID
    std::string generateCheckpointId() const {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 15);

        const char* hex_chars = "0123456789abcdef";
        std::ostringstream oss;

        // UUID format: xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
        for (int i = 0; i < 32; i++) {
            if (i == 8 || i == 12 || i == 16 || i == 20) {
                oss << '-';
            }
            if (i == 12) {
                oss << '4';  // Version 4
            } else if (i == 16) {
                oss << hex_chars[(dis(gen) & 0x3) | 0x8];  // Variant
            } else {
                oss << hex_chars[dis(gen)];
            }
        }

        return oss.str();
    }

    // Get checkpoint file path from ID
    std::filesystem::path getCheckpointPath(const std::string& checkpoint_id) const {
        return checkpoint_dir_ / (checkpoint_id + ".json");
    }

public:
    explicit StateCheckpointManager(const std::string& process_name,
                                   const std::filesystem::path& checkpoint_dir = "/tmp/mxrc/checkpoints",
                                   size_t max_checkpoints = 10,
                                   size_t retention_hours = 24)
        : process_name_(process_name)
        , checkpoint_dir_(checkpoint_dir)
        , max_checkpoints_(max_checkpoints)
        , retention_hours_(retention_hours) {

        // Create checkpoint directory if it doesn't exist
        if (!std::filesystem::exists(checkpoint_dir_)) {
            std::filesystem::create_directories(checkpoint_dir_);
            spdlog::info("Created checkpoint directory: {}", checkpoint_dir_.string());
        }
    }

    // T045: Create a new checkpoint
    StateCheckpoint createCheckpoint() override {
        StateCheckpoint checkpoint;
        checkpoint.checkpoint_id = generateCheckpointId();
        checkpoint.process_name = process_name_;
        checkpoint.timestamp = std::chrono::system_clock::now();
        checkpoint.is_complete = false;
        checkpoint.checkpoint_size_bytes = 0;
        checkpoint.file_path = getCheckpointPath(checkpoint.checkpoint_id);

        spdlog::info("Created checkpoint {} for process {}",
                    checkpoint.checkpoint_id, process_name_);

        return checkpoint;
    }

    // T049: Load checkpoint from file
    StateCheckpoint loadCheckpoint(const std::string& checkpoint_id) override {
        auto file_path = getCheckpointPath(checkpoint_id);

        if (!std::filesystem::exists(file_path)) {
            throw std::runtime_error("Checkpoint file not found: " + file_path.string());
        }

        std::ifstream file(file_path);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open checkpoint file: " + file_path.string());
        }

        nlohmann::json j;
        file >> j;

        StateCheckpoint checkpoint;
        checkpoint.checkpoint_id = j["checkpoint_id"];
        checkpoint.process_name = j["process_name"];

        // Parse timestamp
        auto timestamp_str = j["timestamp"].get<std::string>();
        std::tm tm = {};
        std::istringstream ss(timestamp_str);
        ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
        checkpoint.timestamp = std::chrono::system_clock::from_time_t(std::mktime(&tm));

        checkpoint.rt_state = j["rt_state"];

        if (j.contains("datastore_snapshot")) {
            checkpoint.datastore_snapshot = j["datastore_snapshot"];
        }

        if (j.contains("eventbus_queue_snapshot")) {
            checkpoint.eventbus_queue_snapshot = j["eventbus_queue_snapshot"].get<std::vector<std::string>>();
        }

        checkpoint.checkpoint_size_bytes = j["checkpoint_size_bytes"];
        checkpoint.is_complete = j["is_complete"];
        checkpoint.file_path = file_path;

        spdlog::info("Loaded checkpoint {} for process {}", checkpoint_id, process_name_);

        return checkpoint;
    }

    // T049: Save checkpoint to file
    bool saveCheckpoint(const StateCheckpoint& checkpoint) override {
        try {
            nlohmann::json j;
            j["checkpoint_id"] = checkpoint.checkpoint_id;
            j["process_name"] = checkpoint.process_name;

            // Format timestamp as ISO 8601
            auto time_t_val = std::chrono::system_clock::to_time_t(checkpoint.timestamp);
            std::ostringstream oss;
            oss << std::put_time(std::gmtime(&time_t_val), "%Y-%m-%dT%H:%M:%SZ");
            j["timestamp"] = oss.str();

            j["rt_state"] = checkpoint.rt_state;
            j["datastore_snapshot"] = checkpoint.datastore_snapshot;
            j["eventbus_queue_snapshot"] = checkpoint.eventbus_queue_snapshot;
            j["checkpoint_size_bytes"] = checkpoint.checkpoint_size_bytes;
            j["is_complete"] = checkpoint.is_complete;

            // Write to file
            std::ofstream file(checkpoint.file_path);
            if (!file.is_open()) {
                spdlog::error("Failed to open checkpoint file for writing: {}",
                             checkpoint.file_path.string());
                return false;
            }

            file << j.dump(2);  // Pretty print with 2-space indent
            file.close();

            spdlog::info("Saved checkpoint {} to {}",
                        checkpoint.checkpoint_id, checkpoint.file_path.string());

            // Cleanup old checkpoints if exceeding max
            auto checkpoints = listCheckpoints();
            if (checkpoints.size() > max_checkpoints_) {
                // Delete oldest checkpoints
                size_t to_delete = checkpoints.size() - max_checkpoints_;
                for (size_t i = 0; i < to_delete; i++) {
                    deleteCheckpoint(checkpoints[i]);
                }
            }

            return true;

        } catch (const std::exception& e) {
            spdlog::error("Failed to save checkpoint {}: {}",
                         checkpoint.checkpoint_id, e.what());
            return false;
        }
    }

    // T049: Delete checkpoint file
    bool deleteCheckpoint(const std::string& checkpoint_id) override {
        auto file_path = getCheckpointPath(checkpoint_id);

        if (!std::filesystem::exists(file_path)) {
            spdlog::warn("Checkpoint file not found for deletion: {}", file_path.string());
            return false;
        }

        try {
            std::filesystem::remove(file_path);
            spdlog::info("Deleted checkpoint {}", checkpoint_id);
            return true;
        } catch (const std::exception& e) {
            spdlog::error("Failed to delete checkpoint {}: {}", checkpoint_id, e.what());
            return false;
        }
    }

    // List all available checkpoints (sorted by timestamp, oldest first)
    std::vector<std::string> listCheckpoints() const override {
        std::vector<std::pair<std::string, std::filesystem::file_time_type>> checkpoints;

        if (!std::filesystem::exists(checkpoint_dir_)) {
            return {};
        }

        for (const auto& entry : std::filesystem::directory_iterator(checkpoint_dir_)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                std::string checkpoint_id = entry.path().stem().string();
                checkpoints.push_back({checkpoint_id, entry.last_write_time()});
            }
        }

        // Sort by timestamp (oldest first)
        std::sort(checkpoints.begin(), checkpoints.end(),
                 [](const auto& a, const auto& b) { return a.second < b.second; });

        std::vector<std::string> result;
        for (const auto& [id, _] : checkpoints) {
            result.push_back(id);
        }

        return result;
    }

    // Clean up expired checkpoints
    size_t cleanupExpiredCheckpoints(size_t retention_hours = 24) override {
        auto now = std::chrono::system_clock::now();
        auto retention_duration = std::chrono::hours(retention_hours);
        size_t deleted_count = 0;

        auto checkpoints = listCheckpoints();
        for (const auto& checkpoint_id : checkpoints) {
            try {
                auto checkpoint = loadCheckpoint(checkpoint_id);
                auto age = now - checkpoint.timestamp;

                if (age > retention_duration) {
                    if (deleteCheckpoint(checkpoint_id)) {
                        deleted_count++;
                    }
                }
            } catch (const std::exception& e) {
                spdlog::warn("Failed to check checkpoint {} for expiry: {}",
                            checkpoint_id, e.what());
            }
        }

        if (deleted_count > 0) {
            spdlog::info("Cleaned up {} expired checkpoints (retention: {}h)",
                        deleted_count, retention_hours);
        }

        return deleted_count;
    }

    // Verify checkpoint integrity
    bool verifyCheckpoint(const std::string& checkpoint_id) const override {
        auto file_path = getCheckpointPath(checkpoint_id);

        if (!std::filesystem::exists(file_path)) {
            return false;
        }

        try {
            // Try to load the checkpoint to verify it's valid JSON
            std::ifstream file(file_path);
            nlohmann::json j;
            file >> j;

            // Basic validation
            if (!j.contains("checkpoint_id") || !j.contains("process_name") ||
                !j.contains("timestamp") || !j.contains("is_complete")) {
                spdlog::warn("Checkpoint {} missing required fields", checkpoint_id);
                return false;
            }

            // Check file size matches
            auto actual_size = std::filesystem::file_size(file_path);
            auto recorded_size = j["checkpoint_size_bytes"].get<uint64_t>();

            if (actual_size != recorded_size) {
                spdlog::warn("Checkpoint {} size mismatch: actual={}, recorded={}",
                            checkpoint_id, actual_size, recorded_size);
                // Don't fail on size mismatch, just warn
            }

            return true;

        } catch (const std::exception& e) {
            spdlog::error("Checkpoint {} verification failed: {}", checkpoint_id, e.what());
            return false;
        }
    }
};

// Factory function to create StateCheckpointManager
std::unique_ptr<IStateCheckpoint> createStateCheckpointManager(
    const std::string& process_name,
    const std::filesystem::path& checkpoint_dir,
    size_t max_checkpoints,
    size_t retention_hours) {

    return std::make_unique<StateCheckpointManager>(
        process_name, checkpoint_dir, max_checkpoints, retention_hours);
}

} // namespace mxrc::ha
