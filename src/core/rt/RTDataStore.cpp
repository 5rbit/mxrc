#include "RTDataStore.h"
#include "util/TimeUtils.h"
#include <spdlog/spdlog.h>
#include <cstring>
#include <thread>

namespace mxrc {
namespace core {
namespace rt {

RTDataStore::RTDataStore() {
    // 모든 엔트리 초기화 (생성자에서 자동 호출됨)
}

bool RTDataStore::isValidKey(DataKey key) const {
    return static_cast<uint16_t>(key) < static_cast<uint16_t>(DataKey::MAX_KEYS);
}

int RTDataStore::setInt32(DataKey key, int32_t value) {
    if (!isValidKey(key)) {
        return -1;
    }

    auto idx = static_cast<size_t>(key);

    // Seqlock: 쓰기 시작 (seq를 홀수로)
    entries_[idx].seq.fetch_add(1, std::memory_order_release);

    // 데이터 쓰기
    entries_[idx].value.i32 = value;
    entries_[idx].type = DataType::INT32;
    entries_[idx].timestamp_ns = util::getMonotonicTimeNs();

    // Seqlock: 쓰기 완료 (seq를 짝수로)
    entries_[idx].seq.fetch_add(1, std::memory_order_release);

    return 0;
}

int RTDataStore::setFloat(DataKey key, float value) {
    if (!isValidKey(key)) {
        return -1;
    }

    auto idx = static_cast<size_t>(key);

    // Seqlock: 쓰기 시작 (seq를 홀수로)
    entries_[idx].seq.fetch_add(1, std::memory_order_release);

    // 데이터 쓰기
    entries_[idx].value.f32 = value;
    entries_[idx].type = DataType::FLOAT;
    entries_[idx].timestamp_ns = util::getMonotonicTimeNs();

    // Seqlock: 쓰기 완료 (seq를 짝수로)
    entries_[idx].seq.fetch_add(1, std::memory_order_release);

    return 0;
}

int RTDataStore::setDouble(DataKey key, double value) {
    if (!isValidKey(key)) {
        return -1;
    }

    auto idx = static_cast<size_t>(key);

    // Seqlock: 쓰기 시작 (seq를 홀수로)
    entries_[idx].seq.fetch_add(1, std::memory_order_release);

    // 데이터 쓰기
    entries_[idx].value.f64 = value;
    entries_[idx].type = DataType::DOUBLE;
    entries_[idx].timestamp_ns = util::getMonotonicTimeNs();

    // Seqlock: 쓰기 완료 (seq를 짝수로)
    entries_[idx].seq.fetch_add(1, std::memory_order_release);

    return 0;
}

int RTDataStore::setUint64(DataKey key, uint64_t value) {
    if (!isValidKey(key)) {
        return -1;
    }

    auto idx = static_cast<size_t>(key);

    // Seqlock: 쓰기 시작 (seq를 홀수로)
    entries_[idx].seq.fetch_add(1, std::memory_order_release);

    // 데이터 쓰기
    entries_[idx].value.u64 = value;
    entries_[idx].type = DataType::UINT64;
    entries_[idx].timestamp_ns = util::getMonotonicTimeNs();

    // Seqlock: 쓰기 완료 (seq를 짝수로)
    entries_[idx].seq.fetch_add(1, std::memory_order_release);

    return 0;
}

int RTDataStore::setString(DataKey key, const char* value, size_t len) {
    if (!isValidKey(key) || value == nullptr) {
        return -1;
    }

    auto idx = static_cast<size_t>(key);

    // Seqlock: 쓰기 시작 (seq를 홀수로)
    entries_[idx].seq.fetch_add(1, std::memory_order_release);

    // 데이터 쓰기
    // 최대 31바이트 복사 (null terminator 포함 32바이트)
    size_t copy_len = (len < 31) ? len : 31;
    std::memcpy(entries_[idx].value.str, value, copy_len);
    entries_[idx].value.str[copy_len] = '\0';
    entries_[idx].type = DataType::STRING;
    entries_[idx].timestamp_ns = util::getMonotonicTimeNs();

    // Seqlock: 쓰기 완료 (seq를 짝수로)
    entries_[idx].seq.fetch_add(1, std::memory_order_release);

    return 0;
}

int RTDataStore::getInt32(DataKey key, int32_t& out_value) const {
    if (!isValidKey(key)) {
        return -1;
    }

    auto idx = static_cast<size_t>(key);

    // Seqlock 읽기: 재시도 루프
    uint64_t seq1, seq2;
    int32_t temp_value;
    DataType temp_type;

    do {
        // seq 읽기 (쓰기 시작 전)
        seq1 = entries_[idx].seq.load(std::memory_order_acquire);

        // seq가 홀수이면 쓰기 진행 중 - 재시도
        if (seq1 & 1) {
            std::this_thread::yield();
            continue;
        }

        // 데이터 읽기
        temp_type = entries_[idx].type;
        temp_value = entries_[idx].value.i32;

        // seq 다시 읽기 (쓰기 완료 후)
        seq2 = entries_[idx].seq.load(std::memory_order_acquire);

        // seq1 == seq2이면 읽는 동안 쓰기가 없었음 - 성공
    } while (seq1 != seq2);

    // 타입 검증
    if (temp_type != DataType::INT32) {
        return -1;
    }

    out_value = temp_value;
    return 0;
}

int RTDataStore::getFloat(DataKey key, float& out_value) const {
    if (!isValidKey(key)) {
        return -1;
    }

    auto idx = static_cast<size_t>(key);

    // Seqlock 읽기: 재시도 루프
    uint64_t seq1, seq2;
    float temp_value;
    DataType temp_type;

    do {
        seq1 = entries_[idx].seq.load(std::memory_order_acquire);
        if (seq1 & 1) {
            std::this_thread::yield();
            continue;
        }

        temp_type = entries_[idx].type;
        temp_value = entries_[idx].value.f32;

        seq2 = entries_[idx].seq.load(std::memory_order_acquire);
    } while (seq1 != seq2);

    if (temp_type != DataType::FLOAT) {
        return -1;
    }

    out_value = temp_value;
    return 0;
}

int RTDataStore::getDouble(DataKey key, double& out_value) const {
    if (!isValidKey(key)) {
        return -1;
    }

    auto idx = static_cast<size_t>(key);

    // Seqlock 읽기: 재시도 루프
    uint64_t seq1, seq2;
    double temp_value;
    DataType temp_type;

    do {
        seq1 = entries_[idx].seq.load(std::memory_order_acquire);
        if (seq1 & 1) {
            std::this_thread::yield();
            continue;
        }

        temp_type = entries_[idx].type;
        temp_value = entries_[idx].value.f64;

        seq2 = entries_[idx].seq.load(std::memory_order_acquire);
    } while (seq1 != seq2);

    if (temp_type != DataType::DOUBLE) {
        return -1;
    }

    out_value = temp_value;
    return 0;
}

int RTDataStore::getUint64(DataKey key, uint64_t& out_value) const {
    if (!isValidKey(key)) {
        return -1;
    }

    auto idx = static_cast<size_t>(key);

    // Seqlock 읽기: 재시도 루프
    uint64_t seq1, seq2;
    uint64_t temp_value;
    DataType temp_type;

    do {
        seq1 = entries_[idx].seq.load(std::memory_order_acquire);
        if (seq1 & 1) {
            std::this_thread::yield();
            continue;
        }

        temp_type = entries_[idx].type;
        temp_value = entries_[idx].value.u64;

        seq2 = entries_[idx].seq.load(std::memory_order_acquire);
    } while (seq1 != seq2);

    if (temp_type != DataType::UINT64) {
        return -1;
    }

    out_value = temp_value;
    return 0;
}

int RTDataStore::getString(DataKey key, char* out_buffer, size_t buffer_size) const {
    if (!isValidKey(key) || out_buffer == nullptr || buffer_size == 0) {
        return -1;
    }

    auto idx = static_cast<size_t>(key);

    // Seqlock 읽기: 재시도 루프
    uint64_t seq1, seq2;
    char temp_str[32];
    DataType temp_type;

    do {
        seq1 = entries_[idx].seq.load(std::memory_order_acquire);
        if (seq1 & 1) {
            std::this_thread::yield();
            continue;
        }

        temp_type = entries_[idx].type;
        std::memcpy(temp_str, entries_[idx].value.str, 32);

        seq2 = entries_[idx].seq.load(std::memory_order_acquire);
    } while (seq1 != seq2);

    if (temp_type != DataType::STRING) {
        return -1;
    }

    // 안전하게 복사
    size_t copy_len = (buffer_size < 32) ? buffer_size - 1 : 31;
    std::memcpy(out_buffer, temp_str, copy_len);
    out_buffer[copy_len] = '\0';

    return 0;
}

uint64_t RTDataStore::incrementSeq(DataKey key) {
    if (!isValidKey(key)) {
        return 0;
    }

    auto idx = static_cast<size_t>(key);
    return entries_[idx].seq.fetch_add(1, std::memory_order_relaxed);
}

uint64_t RTDataStore::getSeq(DataKey key) const {
    if (!isValidKey(key)) {
        return 0;
    }

    auto idx = static_cast<size_t>(key);
    return entries_[idx].seq.load(std::memory_order_relaxed);
}

bool RTDataStore::isFresh(DataKey key, uint64_t max_age_ns) const {
    if (!isValidKey(key)) {
        return false;
    }

    auto idx = static_cast<size_t>(key);

    // 데이터가 없으면 fresh하지 않음
    if (entries_[idx].type == DataType::NONE) {
        return false;
    }

    uint64_t current_time = util::getMonotonicTimeNs();
    uint64_t age = current_time - entries_[idx].timestamp_ns;

    return age <= max_age_ns;
}

uint64_t RTDataStore::getTimestamp(DataKey key) const {
    if (!isValidKey(key)) {
        return 0;
    }

    auto idx = static_cast<size_t>(key);
    return entries_[idx].timestamp_ns;
}

} // namespace rt
} // namespace core
} // namespace mxrc
