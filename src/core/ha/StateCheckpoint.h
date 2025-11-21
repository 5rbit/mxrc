#pragma once

#include <string>
#include <chrono>
#include <filesystem>
#include <vector>
#include <nlohmann/json.hpp>

namespace mxrc {
namespace ha {

/**
 * @brief State checkpoint data structure
 *
 * Production readiness: Checkpoint for process recovery.
 * Contains serialized process state for failover scenarios.
 *
 * Based on data-model.md: StateCheckpoint
 */
struct StateCheckpoint {
    std::string checkpoint_id;                          // UUID
    std::string process_name;                           // Process name
    std::chrono::system_clock::time_point timestamp;    // Checkpoint creation time
    nlohmann::json rt_state;                            // RT process state (Task/Sequence/Action)
    nlohmann::json datastore_snapshot;                  // RTDataStore snapshot (optional)
    std::vector<std::string> eventbus_queue_snapshot;   // EventBus queue event IDs
    uint64_t checkpoint_size_bytes;                     // Checkpoint size in bytes
    bool is_complete;                                   // Whether checkpoint is complete
    std::filesystem::path file_path;                    // Checkpoint file path

    // Default constructor
    StateCheckpoint()
        : checkpoint_size_bytes(0)
        , is_complete(false) {}
};

/**
 * @brief Checkpoint state enumeration
 *
 * Represents the lifecycle state of a checkpoint.
 */
enum class CheckpointState {
    CREATING,   // Checkpoint being created
    COMPLETE,   // Checkpoint creation completed
    LOADED,     // Checkpoint loaded for recovery
    EXPIRED     // Checkpoint expired (retention period exceeded)
};

/**
 * @brief State checkpoint interface
 *
 * Interface for implementing checkpoint providers.
 * Follows MXRC Constitution principle: Interface-based design (I-prefix).
 */
class IStateCheckpoint {
public:
    virtual ~IStateCheckpoint() = default;

    /**
     * @brief Create a new checkpoint
     *
     * Serializes current process state to JSON and saves to file.
     *
     * @return StateCheckpoint Created checkpoint data
     */
    virtual StateCheckpoint createCheckpoint() = 0;

    /**
     * @brief Load checkpoint from file
     *
     * @param checkpoint_id UUID of checkpoint to load
     * @return StateCheckpoint Loaded checkpoint data
     * @throws std::runtime_error if checkpoint not found or invalid
     */
    virtual StateCheckpoint loadCheckpoint(const std::string& checkpoint_id) = 0;

    /**
     * @brief Save checkpoint to file
     *
     * @param checkpoint Checkpoint data to save
     * @return true if successfully saved, false otherwise
     */
    virtual bool saveCheckpoint(const StateCheckpoint& checkpoint) = 0;

    /**
     * @brief Delete checkpoint file
     *
     * @param checkpoint_id UUID of checkpoint to delete
     * @return true if successfully deleted, false otherwise
     */
    virtual bool deleteCheckpoint(const std::string& checkpoint_id) = 0;

    /**
     * @brief List all available checkpoints
     *
     * @return std::vector<std::string> List of checkpoint IDs
     */
    virtual std::vector<std::string> listCheckpoints() const = 0;

    /**
     * @brief Clean up expired checkpoints
     *
     * Removes checkpoints older than retention period.
     *
     * @param retention_hours Retention period in hours
     * @return size_t Number of checkpoints deleted
     */
    virtual size_t cleanupExpiredCheckpoints(size_t retention_hours = 24) = 0;

    /**
     * @brief Verify checkpoint integrity
     *
     * Checks if checkpoint file exists and matches expected size.
     *
     * @param checkpoint_id UUID of checkpoint to verify
     * @return true if checkpoint is valid, false otherwise
     */
    virtual bool verifyCheckpoint(const std::string& checkpoint_id) const = 0;
};

/**
 * @brief Convert CheckpointState to string
 */
inline std::string checkpointStateToString(CheckpointState state) {
    switch (state) {
        case CheckpointState::CREATING: return "CREATING";
        case CheckpointState::COMPLETE: return "COMPLETE";
        case CheckpointState::LOADED: return "LOADED";
        case CheckpointState::EXPIRED: return "EXPIRED";
        default: return "UNKNOWN";
    }
}

} // namespace ha
} // namespace mxrc
