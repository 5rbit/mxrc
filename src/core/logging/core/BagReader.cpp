#include "BagReader.h"
#include <spdlog/spdlog.h>
#include <algorithm>

namespace mxrc::core::logging {

BagReader::BagReader()
    : currentPosition_(0) {
}

BagReader::~BagReader() {
    close();
}

bool BagReader::open(const std::string& filepath) {
    // 이미 열려있으면 먼저 닫기
    if (ifs_.is_open()) {
        close();
    }

    filepath_ = filepath;
    ifs_.open(filepath_, std::ios::binary);

    if (!ifs_.is_open()) {
        spdlog::error("BagReader::open - Failed to open file: {}", filepath);
        return false;
    }

    // Footer 및 인덱스 로드
    footer_ = indexer_.readFromFile(filepath_);

    if (!footer_.isValid()) {
        spdlog::error("BagReader::open - Invalid bag file: {}", filepath);
        close();
        return false;
    }

    if (!footer_.isSupportedVersion()) {
        spdlog::error("BagReader::open - Unsupported version: {}",
                      static_cast<uint32_t>(footer_.version));
        close();
        return false;
    }

    // 파일 시작 위치로 이동
    seekToStart();

    spdlog::debug("BagReader::open - Opened {}, {} messages",
                  filepath, getMessageCount());

    return true;
}

void BagReader::close() {
    if (ifs_.is_open()) {
        ifs_.close();
    }
    filepath_.clear();
    indexer_.clear();
    topicFilter_.clear();
    currentPosition_ = 0;
}

bool BagReader::hasNext() const {
    if (!ifs_.is_open()) {
        return false;
    }

    // 현재 위치가 데이터 영역 내인지 확인
    return isInDataArea();
}

std::optional<BagMessage> BagReader::readNext() {
    if (!hasNext()) {
        return std::nullopt;
    }

    while (isInDataArea()) {
        auto line = readLine();
        if (!line) {
            return std::nullopt;
        }

        try {
            BagMessage msg = BagMessage::fromJsonLine(*line);

            // 토픽 필터링
            if (!topicFilter_.empty() && msg.topic != topicFilter_) {
                continue;  // 필터링된 토픽은 건너뛰기
            }

            return msg;
        } catch (const std::exception& e) {
            spdlog::error("BagReader::readNext - Failed to parse message: {}", e.what());
            continue;  // 파싱 실패 시 다음 줄 시도
        }
    }

    return std::nullopt;
}

bool BagReader::seekToTimestamp(uint64_t timestamp_ns) {
    if (!ifs_.is_open()) {
        spdlog::error("BagReader::seekToTimestamp - File not open");
        return false;
    }

    if (indexer_.empty()) {
        spdlog::warn("BagReader::seekToTimestamp - No index available");
        return false;
    }

    bool found = false;
    IndexEntry entry = indexer_.findByTimestamp(timestamp_ns, found);

    if (!found) {
        spdlog::error("BagReader::seekToTimestamp - Timestamp not found: {}", timestamp_ns);
        return false;
    }

    // 파일 위치 이동
    ifs_.seekg(static_cast<std::streamoff>(entry.file_offset), std::ios::beg);

    if (!ifs_.good()) {
        spdlog::error("BagReader::seekToTimestamp - Failed to seek to offset: {}",
                      static_cast<uint64_t>(entry.file_offset));
        return false;
    }

    currentPosition_ = entry.file_offset;

    spdlog::debug("BagReader::seekToTimestamp - Seeked to timestamp {}, offset {}",
                  timestamp_ns, static_cast<uint64_t>(entry.file_offset));

    return true;
}

void BagReader::seekToStart() {
    if (ifs_.is_open()) {
        ifs_.seekg(0, std::ios::beg);
        currentPosition_ = 0;
    }
}

void BagReader::setTopicFilter(const std::string& topic) {
    topicFilter_ = topic;
    spdlog::debug("BagReader::setTopicFilter - Filter set to: {}", topic);
}

void BagReader::clearTopicFilter() {
    topicFilter_.clear();
    spdlog::debug("BagReader::clearTopicFilter - Filter cleared");
}

size_t BagReader::getMessageCount() const {
    return indexer_.size();
}

uint64_t BagReader::getStartTimestamp() const {
    if (indexer_.empty()) {
        return 0;
    }

    bool found = false;
    IndexEntry entry = indexer_.findByTimestamp(0, found);
    return found ? entry.timestamp_ns : 0;
}

uint64_t BagReader::getEndTimestamp() const {
    if (indexer_.empty()) {
        return 0;
    }

    bool found = false;
    IndexEntry entry = indexer_.findByTimestamp(UINT64_MAX, found);
    return found ? entry.timestamp_ns : 0;
}

std::optional<std::string> BagReader::readLine() {
    if (!ifs_.is_open() || !isInDataArea()) {
        return std::nullopt;
    }

    std::string line;
    std::getline(ifs_, line);

    if (!ifs_.good() && !ifs_.eof()) {
        spdlog::error("BagReader::readLine - Failed to read line");
        return std::nullopt;
    }

    // 현재 위치 업데이트
    currentPosition_ = ifs_.tellg();

    // 빈 줄은 건너뛰기
    if (line.empty()) {
        return readLine();
    }

    return line;
}

bool BagReader::isInDataArea() const {
    if (!ifs_.is_open()) {
        return false;
    }

    // 현재 위치가 데이터 영역 내인지 확인
    // 데이터 영역: [0, index_offset)
    uint64_t indexOffset = static_cast<uint64_t>(footer_.index_offset);

    return currentPosition_ < indexOffset;
}

} // namespace mxrc::core::logging
