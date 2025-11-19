#include "core/AsyncWriter.h"
#include <spdlog/spdlog.h>
#include <chrono>

namespace mxrc::core::logging {

AsyncWriter::AsyncWriter(const std::string& filepath, size_t queueCapacity)
    : filepath_(filepath), queueCapacity_(queueCapacity) {
    spdlog::info("AsyncWriter created for file: {}, queue capacity: {}", filepath_, queueCapacity_);
}

AsyncWriter::~AsyncWriter() {
    stop();
    spdlog::info("AsyncWriter destroyed. Written: {}, Dropped: {}, Bytes: {}",
                 writtenCount_.load(), droppedCount_.load(), bytesWritten_.load());
}

void AsyncWriter::start() {
    if (running_.load()) {
        spdlog::warn("AsyncWriter already running");
        return;
    }

    // 파일 열기
    ofs_.open(filepath_, std::ios::out | std::ios::app);
    if (!ofs_.is_open()) {
        spdlog::error("Failed to open file for writing: {}", filepath_);
        throw std::runtime_error("Failed to open file: " + filepath_);
    }

    running_.store(true);
    writerThread_ = std::thread(&AsyncWriter::writerLoop, this);
    spdlog::info("AsyncWriter started");
}

void AsyncWriter::stop() {
    if (!running_.load()) {
        return;
    }

    spdlog::info("Stopping AsyncWriter...");
    running_.store(false);
    cv_.notify_all();

    if (writerThread_.joinable()) {
        writerThread_.join();
    }

    if (ofs_.is_open()) {
        ofs_.flush();
        ofs_.close();
    }

    spdlog::info("AsyncWriter stopped");
}

bool AsyncWriter::tryPush(const BagMessage& msg) {
    std::lock_guard<std::mutex> lock(queueMutex_);

    if (messageQueue_.size() >= queueCapacity_) {
        droppedCount_.fetch_add(1, std::memory_order_relaxed);
        spdlog::warn("Message queue full, dropping message. Dropped count: {}", droppedCount_.load());
        return false;
    }

    messageQueue_.push(msg);
    cv_.notify_one();
    return true;
}

bool AsyncWriter::flush(uint32_t timeoutMs) {
    auto startTime = std::chrono::steady_clock::now();

    while (queueSize() > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        if (timeoutMs > 0) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - startTime
            ).count();

            if (elapsed >= timeoutMs) {
                spdlog::warn("Flush timeout after {} ms, {} messages remaining", timeoutMs, queueSize());
                return false;
            }
        }
    }

    // 파일 스트림 flush
    if (ofs_.is_open()) {
        ofs_.flush();
    }

    return true;
}

size_t AsyncWriter::queueSize() const {
    std::lock_guard<std::mutex> lock(queueMutex_);
    return messageQueue_.size();
}

uint64_t AsyncWriter::getDroppedCount() const {
    return droppedCount_.load(std::memory_order_relaxed);
}

uint64_t AsyncWriter::getWrittenCount() const {
    return writtenCount_.load(std::memory_order_relaxed);
}

uint64_t AsyncWriter::getBytesWritten() const {
    return bytesWritten_.load(std::memory_order_relaxed);
}

bool AsyncWriter::isOpen() const {
    return ofs_.is_open();
}

void AsyncWriter::writerLoop() {
    spdlog::info("Writer thread started");

    while (running_.load() || !messageQueue_.empty()) {
        std::unique_lock<std::mutex> lock(queueMutex_);

        // 큐가 비어있으면 대기
        cv_.wait(lock, [this] {
            return !messageQueue_.empty() || !running_.load();
        });

        // 메시지 처리
        while (!messageQueue_.empty()) {
            BagMessage msg = messageQueue_.front();
            messageQueue_.pop();

            // 락 해제 후 디스크 쓰기 (I/O는 락 외부에서)
            lock.unlock();

            try {
                std::string line = msg.toJsonLine();
                ofs_ << line;

                writtenCount_.fetch_add(1, std::memory_order_relaxed);
                bytesWritten_.fetch_add(line.size(), std::memory_order_relaxed);

            } catch (const std::exception& e) {
                spdlog::error("Failed to write message: {}", e.what());
            }

            lock.lock();
        }
    }

    // 남은 메시지가 있으면 모두 처리
    std::lock_guard<std::mutex> lock(queueMutex_);
    while (!messageQueue_.empty()) {
        try {
            BagMessage msg = messageQueue_.front();
            messageQueue_.pop();

            std::string line = msg.toJsonLine();
            ofs_ << line;

            writtenCount_.fetch_add(1, std::memory_order_relaxed);
            bytesWritten_.fetch_add(line.size(), std::memory_order_relaxed);

        } catch (const std::exception& e) {
            spdlog::error("Failed to write remaining message: {}", e.what());
        }
    }

    ofs_.flush();
    spdlog::info("Writer thread stopped. Total written: {}", writtenCount_.load());
}

} // namespace mxrc::core::logging
