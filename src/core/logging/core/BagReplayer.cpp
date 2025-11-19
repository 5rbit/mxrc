#include "BagReplayer.h"
#include <spdlog/spdlog.h>
#include <thread>

namespace mxrc::core::logging {

BagReplayer::BagReplayer()
    : reader_(std::make_unique<BagReader>()) {
}

BagReplayer::~BagReplayer() {
    stop();
    if (replayThread_ && replayThread_->joinable()) {
        replayThread_->join();
    }
}

bool BagReplayer::open(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(controlMutex_);

    if (isPlaying_) {
        spdlog::error("BagReplayer::open - Cannot open while playing");
        return false;
    }

    bool result = reader_->open(filepath);
    if (result) {
        spdlog::info("BagReplayer::open - Opened {}", filepath);
    }

    return result;
}

void BagReplayer::close() {
    stop();

    std::lock_guard<std::mutex> lock(controlMutex_);
    reader_->close();
}

bool BagReplayer::start(const ReplaySpeed& speed) {
    std::lock_guard<std::mutex> lock(controlMutex_);

    if (isPlaying_) {
        spdlog::warn("BagReplayer::start - Already playing");
        return false;
    }

    if (!reader_->isOpen()) {
        spdlog::error("BagReplayer::start - No file open");
        return false;
    }

    speed_ = speed;
    shouldStop_ = false;
    isPaused_ = false;

    // 통계 초기화
    {
        std::lock_guard<std::mutex> statsLock(statsMutex_);
        stats_ = ReplayStats();
    }

    // 시작 시간 범위가 설정되어 있으면 해당 위치로 이동
    if (startTime_ > 0) {
        reader_->seekToTimestamp(startTime_);
    } else {
        reader_->seekToStart();
    }

    // 재생 스레드 시작
    isPlaying_ = true;
    replayThread_ = std::make_unique<std::thread>(&BagReplayer::replayThread, this);

    spdlog::info("BagReplayer::start - Started with speed {}", speed.multiplier);
    return true;
}

void BagReplayer::pause() {
    if (isPlaying_ && !isPaused_) {
        isPaused_ = true;
        spdlog::info("BagReplayer::pause - Paused");
    }
}

void BagReplayer::resume() {
    if (isPlaying_ && isPaused_) {
        isPaused_ = false;
        spdlog::info("BagReplayer::resume - Resumed");
    }
}

void BagReplayer::stop() {
    if (!isPlaying_) {
        return;
    }

    shouldStop_ = true;
    isPaused_ = false;  // 일시정지 해제하여 스레드가 종료되도록

    if (replayThread_ && replayThread_->joinable()) {
        replayThread_->join();
    }

    isPlaying_ = false;
    spdlog::info("BagReplayer::stop - Stopped");
}

void BagReplayer::waitUntilFinished() {
    if (replayThread_ && replayThread_->joinable()) {
        replayThread_->join();
    }
}

void BagReplayer::setMessageCallback(MessageCallback callback) {
    std::lock_guard<std::mutex> lock(controlMutex_);
    messageCallback_ = std::move(callback);
}

void BagReplayer::setTopicFilter(const std::string& topic) {
    std::lock_guard<std::mutex> lock(controlMutex_);
    topicFilter_ = topic;
    if (!topicFilter_.empty()) {
        spdlog::debug("BagReplayer::setTopicFilter - Filter set to: {}", topic);
    }
}

void BagReplayer::setTimeRange(uint64_t startTime, uint64_t endTime) {
    std::lock_guard<std::mutex> lock(controlMutex_);
    startTime_ = startTime;
    endTime_ = endTime;
    spdlog::debug("BagReplayer::setTimeRange - Range: {} to {}", startTime, endTime);
}

ReplayStats BagReplayer::getStats() const {
    std::lock_guard<std::mutex> lock(statsMutex_);
    return stats_;
}

bool BagReplayer::isPlaying() const {
    return isPlaying_;
}

bool BagReplayer::isPaused() const {
    return isPaused_;
}

void BagReplayer::replayThread() {
    replayStartTime_ = std::chrono::steady_clock::now();
    firstMessageTime_ = 0;

    BagMessage previousMsg;
    bool hasPreviousMsg = false;

    while (!shouldStop_ && reader_->hasNext()) {
        // 일시정지 대기
        while (isPaused_ && !shouldStop_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        if (shouldStop_) {
            break;
        }

        // 메시지 읽기
        auto msgOpt = reader_->readNext();
        if (!msgOpt) {
            continue;
        }

        const auto& msg = *msgOpt;

        // 토픽 필터링
        if (!topicFilter_.empty() && msg.topic != topicFilter_) {
            std::lock_guard<std::mutex> lock(statsMutex_);
            stats_.messagesSkipped++;
            continue;
        }

        // 시간 범위 확인
        if (!isInTimeRange(msg)) {
            std::lock_guard<std::mutex> lock(statsMutex_);
            stats_.messagesSkipped++;
            continue;
        }

        // 첫 메시지 타임스탬프 기록
        if (firstMessageTime_ == 0) {
            firstMessageTime_ = static_cast<uint64_t>(msg.timestamp_ns);
        }

        // 이전 메시지가 있으면 타이밍 조절
        if (hasPreviousMsg) {
            waitForNextMessage(previousMsg, msg);
        }

        // 메시지 콜백 호출
        if (messageCallback_) {
            try {
                messageCallback_(msg);
            } catch (const std::exception& e) {
                spdlog::error("BagReplayer::replayThread - Callback exception: {}", e.what());
            }
        }

        // 통계 업데이트
        {
            std::lock_guard<std::mutex> lock(statsMutex_);
            stats_.messagesReplayed++;

            auto now = std::chrono::steady_clock::now();
            stats_.elapsedTime = std::chrono::duration<double>(now - replayStartTime_).count();

            // 진행률 계산 (전체 메시지 수 기준)
            size_t totalMessages = reader_->getMessageCount();
            if (totalMessages > 0) {
                stats_.progress = static_cast<double>(stats_.messagesReplayed) / totalMessages;
            }
        }

        previousMsg = msg;
        hasPreviousMsg = true;
    }

    isPlaying_ = false;

    auto finalStats = getStats();
    spdlog::info("BagReplayer::replayThread - Finished. Replayed: {}, Skipped: {}, Elapsed: {:.2f}s",
                 finalStats.messagesReplayed, finalStats.messagesSkipped, finalStats.elapsedTime);
}

void BagReplayer::waitForNextMessage(const BagMessage& currentMsg, const BagMessage& nextMsg) {
    // 최대 속도 모드면 대기 안 함
    if (!speed_.isRealtime()) {
        return;
    }

    // 타임스탬프 차이 계산 (나노초)
    int64_t timeDiff = nextMsg.timestamp_ns - currentMsg.timestamp_ns;
    if (timeDiff <= 0) {
        return;
    }

    // 재생 속도 적용
    double adjustedDiff = static_cast<double>(timeDiff) / speed_.multiplier;

    // 나노초를 마이크로초로 변환하여 대기
    auto waitTime = std::chrono::microseconds(static_cast<int64_t>(adjustedDiff / 1000.0));

    std::this_thread::sleep_for(waitTime);
}

bool BagReplayer::isInTimeRange(const BagMessage& msg) const {
    uint64_t timestamp = static_cast<uint64_t>(msg.timestamp_ns);
    return timestamp >= startTime_ && timestamp <= endTime_;
}

} // namespace mxrc::core::logging
