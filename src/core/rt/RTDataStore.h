#pragma once

#include <cstdint>
#include <cstring>
#include <atomic>

namespace mxrc {
namespace core {
namespace rt {

// 데이터 타입
enum class DataType : uint8_t {
    NONE = 0,
    INT32,
    FLOAT,
    DOUBLE,
    UINT64,
    STRING  // 최대 32바이트
};

// 데이터 값 (union)
union DataValue {
    int32_t i32;
    float f32;
    double f64;
    uint64_t u64;
    char str[32];

    DataValue() { std::memset(this, 0, sizeof(DataValue)); }
};

// 데이터 엔트리
struct DataEntry {
    DataValue value;
    DataType type;
    uint64_t timestamp_ns;     // 마지막 업데이트 시간
    std::atomic<uint64_t> seq; // 시퀀스 번호 (atomic)

    DataEntry() : type(DataType::NONE), timestamp_ns(0), seq(0) {}
};

// 키 정의 (타입 안전성)
enum class DataKey : uint16_t {
    // 예제 키들
    ROBOT_X = 0,
    ROBOT_Y = 1,
    ROBOT_Z = 2,
    ROBOT_SPEED = 3,
    ROBOT_STATUS = 4,

    // EtherCAT 센서 데이터 (100-199)
    ETHERCAT_SENSOR_POSITION_0 = 100,
    ETHERCAT_SENSOR_POSITION_1 = 101,
    ETHERCAT_SENSOR_POSITION_2 = 102,
    ETHERCAT_SENSOR_POSITION_3 = 103,
    ETHERCAT_SENSOR_VELOCITY_0 = 110,
    ETHERCAT_SENSOR_VELOCITY_1 = 111,
    ETHERCAT_SENSOR_VELOCITY_2 = 112,
    ETHERCAT_SENSOR_VELOCITY_3 = 113,
    ETHERCAT_SENSOR_TORQUE_0 = 120,
    ETHERCAT_SENSOR_TORQUE_1 = 121,
    ETHERCAT_SENSOR_TORQUE_2 = 122,
    ETHERCAT_SENSOR_TORQUE_3 = 123,
    ETHERCAT_SENSOR_DI_0 = 130,
    ETHERCAT_SENSOR_DI_1 = 131,
    ETHERCAT_SENSOR_DI_2 = 132,
    ETHERCAT_SENSOR_DI_3 = 133,
    ETHERCAT_SENSOR_AI_0 = 140,
    ETHERCAT_SENSOR_AI_1 = 141,
    ETHERCAT_SENSOR_AI_2 = 142,
    ETHERCAT_SENSOR_AI_3 = 143,

    // EtherCAT 모터 명령 (200-299)
    ETHERCAT_MOTOR_CMD_0 = 200,
    ETHERCAT_MOTOR_CMD_1 = 201,
    ETHERCAT_MOTOR_CMD_2 = 202,
    ETHERCAT_MOTOR_CMD_3 = 203,
    ETHERCAT_MOTOR_CMD_4 = 204,
    ETHERCAT_MOTOR_CMD_5 = 205,
    ETHERCAT_MOTOR_CMD_6 = 206,
    ETHERCAT_MOTOR_CMD_7 = 207,

    // EtherCAT 상태 (300-319)
    ETHERCAT_MASTER_STATUS = 300,
    ETHERCAT_CYCLE_LATENCY = 301,
    ETHERCAT_ERROR_COUNT = 302,
    ETHERCAT_FRAME_COUNT = 303,

    // 최대 512개 키 지원
    MAX_KEYS = 512
};

// RT용 고정 크기 데이터 저장소
// - 동적 할당 없음
// - 예외 없음 (에러 코드 반환)
// - Lock-free 읽기/쓰기
class RTDataStore {
public:
    RTDataStore();
    ~RTDataStore() = default;

    // 타입별 set 메서드
    int setInt32(DataKey key, int32_t value);
    int setFloat(DataKey key, float value);
    int setDouble(DataKey key, double value);
    int setUint64(DataKey key, uint64_t value);
    int setString(DataKey key, const char* value, size_t len);

    // 타입별 get 메서드
    // 반환: 성공 0, 실패 -1
    int getInt32(DataKey key, int32_t& out_value) const;
    int getFloat(DataKey key, float& out_value) const;
    int getDouble(DataKey key, double& out_value) const;
    int getUint64(DataKey key, uint64_t& out_value) const;
    int getString(DataKey key, char* out_buffer, size_t buffer_size) const;

    // Atomic 시퀀스 번호 증가 및 가져오기
    uint64_t incrementSeq(DataKey key);
    uint64_t getSeq(DataKey key) const;

    // 데이터 신선도 확인
    // max_age_ns: 최대 허용 age (나노초)
    // 반환: 신선하면 true, 오래되었거나 데이터 없으면 false
    bool isFresh(DataKey key, uint64_t max_age_ns) const;

    // 타임스탬프 가져오기
    uint64_t getTimestamp(DataKey key) const;

private:
    // 키 유효성 검증
    bool isValidKey(DataKey key) const;

    // 고정 크기 배열
    DataEntry entries_[static_cast<size_t>(DataKey::MAX_KEYS)];
};

} // namespace rt
} // namespace core
} // namespace mxrc
