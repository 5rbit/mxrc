#include "Indexer.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cstring>

namespace mxrc::core::logging {

void Indexer::addEntry(uint64_t timestamp_ns, uint64_t file_offset) {
    entries_.emplace_back(timestamp_ns, file_offset);
}

bool Indexer::writeToFile(std::ofstream& ofs, uint64_t dataSize) {
    if (!ofs.is_open()) {
        spdlog::error("Indexer::writeToFile - File not open");
        return false;
    }

    // 현재 파일 위치 = 인덱스 블록 시작 위치
    uint64_t indexOffset = ofs.tellp();

    // 1. 인덱스 블록 쓰기
    for (const auto& entry : entries_) {
        ofs.write(reinterpret_cast<const char*>(&entry), sizeof(IndexEntry));
    }

    if (!ofs.good()) {
        spdlog::error("Indexer::writeToFile - Failed to write index block");
        return false;
    }

    uint64_t indexSize = entries_.size() * sizeof(IndexEntry);

    // 2. BagFooter 생성
    BagFooter footer;
    footer.setDataSize(dataSize);
    footer.setIndexInfo(indexOffset, entries_.size());

    // 3. 체크섬 계산 (데이터 + 인덱스 블록)
    // NOTE: 실제 체크섬 계산은 파일 전체를 읽어야 하므로
    //       여기서는 0으로 설정 (향후 개선 가능)
    footer.setChecksum(0);

    // 4. Footer 쓰기
    ofs.write(reinterpret_cast<const char*>(&footer), sizeof(BagFooter));

    if (!ofs.good()) {
        spdlog::error("Indexer::writeToFile - Failed to write footer");
        return false;
    }

    spdlog::debug("Indexer::writeToFile - Wrote {} index entries, footer at offset {}",
                  entries_.size(), indexOffset + indexSize);

    return true;
}

BagFooter Indexer::readFromFile(const std::string& filepath) {
    std::ifstream ifs(filepath, std::ios::binary);

    if (!ifs.is_open()) {
        spdlog::error("Indexer::readFromFile - Failed to open file: {}", filepath);
        return BagFooter::createInvalid();
    }

    // 1. 파일 크기 확인
    ifs.seekg(0, std::ios::end);
    std::streampos fileSize = ifs.tellg();

    if (fileSize < static_cast<std::streampos>(sizeof(BagFooter))) {
        spdlog::error("Indexer::readFromFile - File too small: {} bytes", fileSize);
        return BagFooter::createInvalid();
    }

    // 2. Footer 읽기 (파일 끝에서 64바이트)
    ifs.seekg(-static_cast<std::streamoff>(sizeof(BagFooter)), std::ios::end);

    BagFooter footer;
    ifs.read(reinterpret_cast<char*>(&footer), sizeof(BagFooter));

    if (!ifs.good()) {
        spdlog::error("Indexer::readFromFile - Failed to read footer");
        return BagFooter::createInvalid();
    }

    // 3. Footer 유효성 검증
    if (!footer.isValid()) {
        spdlog::error("Indexer::readFromFile - Invalid footer magic");
        return BagFooter::createInvalid();
    }

    if (!footer.isSupportedVersion()) {
        spdlog::error("Indexer::readFromFile - Unsupported version: {}",
                      static_cast<uint32_t>(footer.version));
        return BagFooter::createInvalid();
    }

    // 4. 인덱스 블록 읽기
    if (footer.index_count == 0) {
        spdlog::warn("Indexer::readFromFile - No index entries");
        return footer;  // Valid footer but no index
    }

    ifs.seekg(static_cast<std::streamoff>(footer.index_offset), std::ios::beg);

    entries_.clear();
    entries_.reserve(static_cast<size_t>(footer.index_count));

    for (uint64_t i = 0; i < static_cast<uint64_t>(footer.index_count); i++) {
        IndexEntry entry;
        ifs.read(reinterpret_cast<char*>(&entry), sizeof(IndexEntry));

        if (!ifs.good()) {
            spdlog::error("Indexer::readFromFile - Failed to read index entry {}", i);
            entries_.clear();
            return BagFooter::createInvalid();
        }

        entries_.push_back(entry);
    }

    spdlog::debug("Indexer::readFromFile - Loaded {} index entries from {}",
                  entries_.size(), filepath);

    return footer;
}

IndexEntry Indexer::findByTimestamp(uint64_t timestamp_ns, bool& found) const {
    if (entries_.empty()) {
        found = false;
        return IndexEntry();
    }

    // 이진 탐색: timestamp_ns보다 작거나 같은 가장 최근 엔트리 찾기
    // lower_bound: 첫 번째로 >= timestamp_ns인 엔트리
    auto it = std::lower_bound(entries_.begin(), entries_.end(),
                                IndexEntry(timestamp_ns, 0));

    // 찾은 위치가 begin()이면, 모든 엔트리가 timestamp_ns보다 크므로
    // 첫 번째 엔트리 반환
    if (it == entries_.begin()) {
        found = true;
        return *it;
    }

    // 찾은 위치가 end()이거나 정확히 일치하지 않으면
    // 이전 엔트리 반환 (가장 가까운 과거 엔트리)
    if (it == entries_.end() || it->timestamp_ns != timestamp_ns) {
        --it;
    }

    found = true;
    return *it;
}

uint32_t Indexer::calculateChecksum(const std::string& filepath,
                                     uint64_t dataSize,
                                     uint64_t indexSize) {
    std::ifstream ifs(filepath, std::ios::binary);

    if (!ifs.is_open()) {
        spdlog::error("Indexer::calculateChecksum - Failed to open file: {}", filepath);
        return 0;
    }

    // 데이터 + 인덱스 블록 전체를 읽어서 CRC32 계산
    uint64_t totalSize = dataSize + indexSize;
    std::vector<char> buffer(totalSize);

    ifs.read(buffer.data(), totalSize);

    if (!ifs.good()) {
        spdlog::error("Indexer::calculateChecksum - Failed to read {} bytes", totalSize);
        return 0;
    }

    return crc32(buffer.data(), totalSize);
}

uint32_t Indexer::crc32(const char* data, size_t length) {
    // CRC32 다항식: 0xEDB88320 (IEEE 802.3)
    static const uint32_t poly = 0xEDB88320;

    // CRC32 테이블 생성 (최초 1회)
    static uint32_t table[256];
    static bool tableInitialized = false;

    if (!tableInitialized) {
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t crc = i;
            for (uint32_t j = 0; j < 8; j++) {
                if (crc & 1) {
                    crc = (crc >> 1) ^ poly;
                } else {
                    crc >>= 1;
                }
            }
            table[i] = crc;
        }
        tableInitialized = true;
    }

    // CRC32 계산
    uint32_t crc = 0xFFFFFFFF;

    for (size_t i = 0; i < length; i++) {
        uint8_t byte = static_cast<uint8_t>(data[i]);
        crc = (crc >> 8) ^ table[(crc ^ byte) & 0xFF];
    }

    return ~crc;
}

} // namespace mxrc::core::logging
