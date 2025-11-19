#include "core/SimpleBagWriter.h"
#include "util/FileUtils.h"
#include <spdlog/spdlog.h>
#include <filesystem>

namespace fs = std::filesystem;

namespace mxrc::core::logging {

SimpleBagWriter::SimpleBagWriter(const std::string& bagDirectory,
                                 const std::string& baseFilename,
                                 size_t queueCapacity)
    : bagDirectory_(bagDirectory),
      baseFilename_(baseFilename),
      queueCapacity_(queueCapacity),
      rotationPolicy_(RotationPolicy::createSizePolicy(1024)),  // 기본 1GB
      retentionPolicy_(RetentionPolicy::createTimePolicy(7)) {  // 기본 7일

    // 디렉토리 생성
    if (!FileUtils::createDirectories(bagDirectory_)) {
        spdlog::warn("Failed to create bag directory: {}", bagDirectory_);
    }

    // RetentionManager 생성
    retentionManager_ = std::make_unique<RetentionManager>(bagDirectory_, retentionPolicy_);

    spdlog::info("SimpleBagWriter created: directory={}, baseFilename={}, queueCapacity={}",
                 bagDirectory_, baseFilename_, queueCapacity_);
}

SimpleBagWriter::~SimpleBagWriter() {
    stop();
    close();
    spdlog::info("SimpleBagWriter destroyed");
}

bool SimpleBagWriter::open(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (isOpen_) {
        spdlog::warn("SimpleBagWriter already open");
        return false;
    }

    currentFilePath_ = filepath;
    asyncWriter_ = std::make_unique<AsyncWriter>(currentFilePath_, queueCapacity_);

    try {
        asyncWriter_->start();
        fileStartTime_ = std::chrono::steady_clock::now();
        isOpen_ = true;
        spdlog::info("SimpleBagWriter opened: {}", currentFilePath_);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to open SimpleBagWriter: {}", e.what());
        asyncWriter_.reset();
        return false;
    }
}

void SimpleBagWriter::close() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!isOpen_) {
        return;
    }

    if (asyncWriter_) {
        asyncWriter_->flush(5000);
        asyncWriter_->stop();
        asyncWriter_.reset();
    }

    isOpen_ = false;
    spdlog::info("SimpleBagWriter closed: {}", currentFilePath_);
}

bool SimpleBagWriter::appendAsync(const BagMessage& msg) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!isOpen_ || !asyncWriter_) {
        spdlog::warn("SimpleBagWriter not open");
        return false;
    }

    // 순환 조건 확인은 flush() 호출 시 수행하여 성능 최적화
    return asyncWriter_->tryPush(msg);
}

bool SimpleBagWriter::append(const BagMessage& msg) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!isOpen_ || !asyncWriter_) {
        spdlog::warn("SimpleBagWriter not open");
        return false;
    }

    // 동기 쓰기: tryPush 후 즉시 flush
    if (!asyncWriter_->tryPush(msg)) {
        return false;
    }

    bool flushed = asyncWriter_->flush(1000);

    // flush 후 순환 조건 확인
    checkAndRotate();

    return flushed;
}

bool SimpleBagWriter::flush(uint32_t timeoutMs) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!isOpen_ || !asyncWriter_) {
        return false;
    }

    bool flushed = asyncWriter_->flush(timeoutMs);

    // flush 후 순환 조건 확인
    checkAndRotate();

    return flushed;
}

void SimpleBagWriter::setRotationPolicy(const RotationPolicy& policy) {
    std::lock_guard<std::mutex> lock(mutex_);
    rotationPolicy_ = policy;
    spdlog::info("Rotation policy updated: type={}",
                 (policy.type == RotationType::SIZE) ? "SIZE" : "TIME");
}

void SimpleBagWriter::setRetentionPolicy(const RetentionPolicy& policy) {
    std::lock_guard<std::mutex> lock(mutex_);
    retentionPolicy_ = policy;
    retentionManager_->setPolicy(policy);
    spdlog::info("Retention policy updated: type={}",
                 (policy.type == RetentionType::TIME) ? "TIME" : "COUNT");
}

bool SimpleBagWriter::shouldRotate() const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!isOpen_ || !asyncWriter_) {
        return false;
    }

    uint64_t currentSize = asyncWriter_->getBytesWritten();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - fileStartTime_
    ).count();

    return rotationPolicy_.shouldRotate(currentSize, elapsed);
}

bool SimpleBagWriter::rotate() {
    // mutex는 호출자(checkAndRotate)가 이미 잡고 있음

    if (!isOpen_ || !asyncWriter_) {
        return false;
    }

    spdlog::info("Rotating bag file: current={}", currentFilePath_);

    // 1. 현재 파일 flush 및 닫기
    asyncWriter_->flush(5000);

    // 2. 현재 Writer의 통계 누적 (순환 전 저장)
    totalMessagesWritten_ += asyncWriter_->getWrittenCount();
    totalMessagesDropped_ += asyncWriter_->getDroppedCount();
    totalBytesWritten_ += asyncWriter_->getBytesWritten();

    asyncWriter_->stop();
    asyncWriter_.reset();

    // 3. 보존 정책 적용
    applyRetentionPolicy();

    // 4. 새 파일 생성
    std::string newFilePath = createNewBagFile();
    asyncWriter_ = std::make_unique<AsyncWriter>(newFilePath, queueCapacity_);

    try {
        asyncWriter_->start();
        currentFilePath_ = newFilePath;
        fileStartTime_ = std::chrono::steady_clock::now();
        rotationCount_++;

        spdlog::info("Bag file rotated: new={}, rotationCount={}",
                     currentFilePath_, rotationCount_);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to rotate bag file: {}", e.what());
        isOpen_ = false;
        return false;
    }
}

BagWriterStats SimpleBagWriter::getStats() const {
    std::lock_guard<std::mutex> lock(mutex_);

    BagWriterStats stats;
    stats.currentFilePath = currentFilePath_;
    stats.rotationCount = rotationCount_;

    if (asyncWriter_) {
        // 누적된 통계 + 현재 Writer의 통계
        stats.messagesWritten = totalMessagesWritten_ + asyncWriter_->getWrittenCount();
        stats.messagesDropped = totalMessagesDropped_ + asyncWriter_->getDroppedCount();
        stats.bytesWritten = totalBytesWritten_ + asyncWriter_->getBytesWritten();
        stats.currentFileSize = asyncWriter_->getBytesWritten();
    } else {
        // Writer가 없으면 누적된 통계만 반환
        stats.messagesWritten = totalMessagesWritten_;
        stats.messagesDropped = totalMessagesDropped_;
        stats.bytesWritten = totalBytesWritten_;
        stats.currentFileSize = 0;
    }

    return stats;
}

std::string SimpleBagWriter::getCurrentFilePath() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return currentFilePath_;
}

bool SimpleBagWriter::isOpen() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return isOpen_;
}

void SimpleBagWriter::start() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (isOpen_) {
        spdlog::warn("SimpleBagWriter already started");
        return;
    }

    // 새 파일 생성 및 시작
    std::string filepath = createNewBagFile();
    currentFilePath_ = filepath;
    asyncWriter_ = std::make_unique<AsyncWriter>(currentFilePath_, queueCapacity_);

    try {
        asyncWriter_->start();
        fileStartTime_ = std::chrono::steady_clock::now();
        isOpen_ = true;
        spdlog::info("SimpleBagWriter started: {}", currentFilePath_);
    } catch (const std::exception& e) {
        spdlog::error("Failed to start SimpleBagWriter: {}", e.what());
        asyncWriter_.reset();
        throw;
    }
}

void SimpleBagWriter::stop() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!isOpen_) {
        return;
    }

    if (asyncWriter_) {
        asyncWriter_->flush(5000);
        asyncWriter_->stop();
        asyncWriter_.reset();
    }

    isOpen_ = false;
    spdlog::info("SimpleBagWriter stopped");
}

std::string SimpleBagWriter::createNewBagFile() {
    // mutex는 호출자가 이미 잡고 있음

    std::string filename = FileUtils::generateTimestampedFilename(baseFilename_, "bag");
    std::string fullPath = (fs::path(bagDirectory_) / filename).string();

    spdlog::info("Creating new bag file: {}", fullPath);
    return fullPath;
}

void SimpleBagWriter::checkAndRotate() {
    // mutex는 호출자가 이미 잡고 있음

    if (shouldRotateInternal()) {
        rotate();
    }
}

bool SimpleBagWriter::shouldRotateInternal() const {
    // mutex는 호출자가 이미 잡고 있음 - 재획득하지 않음

    if (!isOpen_ || !asyncWriter_) {
        return false;
    }

    uint64_t currentSize = asyncWriter_->getBytesWritten();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - fileStartTime_
    ).count();

    return rotationPolicy_.shouldRotate(currentSize, elapsed);
}

void SimpleBagWriter::applyRetentionPolicy() {
    // mutex는 호출자(rotate)가 이미 잡고 있음

    if (retentionManager_) {
        size_t deletedCount = retentionManager_->deleteOldFiles();
        if (deletedCount > 0) {
            spdlog::info("Retention policy applied: {} files deleted", deletedCount);
        }
    }
}

} // namespace mxrc::core::logging
