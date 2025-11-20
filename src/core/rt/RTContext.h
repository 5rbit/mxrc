#pragma once

#include <cstdint>

namespace mxrc {
namespace core {
namespace rt {

// Forward declaration
class RTDataStore;

// RT 실행 컨텍스트
// Action 콜백에 전달되는 컨텍스트 정보
struct RTContext {
    RTDataStore* data_store;  // 공유 데이터 저장소
    uint32_t current_slot;    // 현재 슬롯 번호
    uint64_t cycle_count;     // 총 실행된 사이클 수
    uint64_t timestamp_ns;    // 현재 사이클 시작 시간 (나노초)
};

} // namespace rt
} // namespace core
} // namespace mxrc
