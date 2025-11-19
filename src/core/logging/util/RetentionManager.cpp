#include "util/RetentionManager.h"
#include <spdlog/spdlog.h>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

namespace mxrc::core::logging {

RetentionManager::RetentionManager(const std::string& bagDirectory,
                                   const RetentionPolicy& policy)
    : bagDirectory_(bagDirectory), policy_(policy) {
    // 디렉토리가 없으면 생성
    if (!FileUtils::directoryExists(bagDirectory_)) {
        FileUtils::createDirectories(bagDirectory_);
    }
    spdlog::info("RetentionManager created for directory: {}", bagDirectory_);
}

void RetentionManager::setPolicy(const RetentionPolicy& policy) {
    policy_ = policy;
    spdlog::info("RetentionPolicy updated: type={}, maxAgeDays={}, maxFileCount={}",
                 policy_.type == RetentionType::TIME ? "TIME" : "COUNT",
                 policy_.maxAgeDays, policy_.maxFileCount);
}

size_t RetentionManager::deleteOldFiles() {
    auto files = listBagFiles();
    size_t deletedCount = 0;

    for (size_t i = 0; i < files.size(); i++) {
        try {
            auto fileTime = std::chrono::system_clock::from_time_t(
                FileUtils::getLastModifiedTime(files[i])
            );

            if (policy_.shouldDelete(fileTime, files.size(), i)) {
                if (FileUtils::deleteFile(files[i])) {
                    deletedCount++;
                    spdlog::info("Deleted old bag file: {}", files[i]);
                } else {
                    spdlog::warn("Failed to delete file: {}", files[i]);
                }
            }
        } catch (const std::exception& e) {
            spdlog::error("Error processing file {}: {}", files[i], e.what());
        }
    }

    if (deletedCount > 0) {
        spdlog::info("RetentionPolicy cleanup: deleted {} files", deletedCount);
    }

    return deletedCount;
}

size_t RetentionManager::emergencyDeleteOldest(uint64_t requiredBytes) {
    spdlog::warn("Emergency deletion triggered, required bytes: {}", requiredBytes);

    auto files = listBagFiles();
    size_t deletedCount = 0;
    uint64_t freedSpace = 0;

    // 가장 오래된 파일부터 삭제
    for (const auto& file : files) {
        try {
            uint64_t fileSize = FileUtils::getFileSize(file);

            if (FileUtils::deleteFile(file)) {
                freedSpace += fileSize;
                deletedCount++;
                spdlog::warn("Emergency deleted: {} (size: {} bytes)", file, fileSize);

                // 충분한 공간 확보되면 중단
                if (freedSpace >= requiredBytes) {
                    break;
                }
            }
        } catch (const std::exception& e) {
            spdlog::error("Error during emergency deletion of {}: {}", file, e.what());
        }
    }

    spdlog::warn("Emergency deletion completed: {} files deleted, {} bytes freed",
                 deletedCount, freedSpace);

    return deletedCount;
}

std::vector<std::string> RetentionManager::listBagFiles() const {
    return FileUtils::listFiles(bagDirectory_, "*.bag");
}

uint64_t RetentionManager::getTotalSize() const {
    auto files = listBagFiles();
    uint64_t totalSize = 0;

    for (const auto& file : files) {
        try {
            totalSize += FileUtils::getFileSize(file);
        } catch (const std::exception& e) {
            spdlog::warn("Failed to get size of {}: {}", file, e.what());
        }
    }

    return totalSize;
}

bool RetentionManager::ensureDiskSpace(uint64_t requiredBytes) {
    uint64_t available = FileUtils::getAvailableSpace(bagDirectory_);

    if (available >= requiredBytes) {
        return true;
    }

    spdlog::warn("Insufficient disk space: available={}, required={}",
                 available, requiredBytes);

    // 긴급 삭제 시도
    uint64_t shortage = requiredBytes - available;
    size_t deleted = emergencyDeleteOldest(shortage);

    // 재확인
    available = FileUtils::getAvailableSpace(bagDirectory_);
    bool success = available >= requiredBytes;

    if (success) {
        spdlog::info("Disk space ensured after deleting {} files", deleted);
    } else {
        spdlog::error("Failed to ensure disk space even after emergency deletion");
    }

    return success;
}

} // namespace mxrc::core::logging
