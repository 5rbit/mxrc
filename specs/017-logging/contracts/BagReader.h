// BagReader.h
// Bag 파일 읽기 클래스 (FR-012 ~ FR-013, FR-024)
//
// 이 클래스는 MXRC Bag 파일을 읽고, 타임스탬프 기반 탐색 및
// 손상된 파일 복구 기능을 제공합니다.

#pragma once

#include <string>
#include <optional>
#include <vector>
#include <cstdint>
#include "../data-model.md"  // BagMessage, IndexEntry, BagFooter

namespace mxrc::core::logging {

/// @brief Bag 파일 읽기 클래스
///
/// 이 클래스는 다음 기능 요구사항을 충족합니다:
/// - FR-012: Bag 파일 순차 읽기
/// - FR-013: 타임스탬프 기반 탐색 (<10초 for 1GB+ files)
/// - FR-024: 파일 손상 시 복구 가능한 메시지만 읽기
class BagReader {
public:
    /// @brief 생성자
    BagReader();

    /// @brief 소멸자
    ~BagReader();

    /// @brief Bag 파일 열기 (FR-012)
    ///
    /// 파일을 열고, footer와 index를 읽어서 메모리에 로드합니다.
    /// CRC32 체크섬 검증을 수행하여 파일 무결성을 확인합니다.
    ///
    /// @param filepath Bag 파일 경로
    /// @throws std::runtime_error 파일 열기 실패 또는 포맷 오류
    void open(const std::string& filepath);

    /// @brief Bag 파일 닫기
    ///
    /// 열려있는 파일 디스크립터를 닫고 메모리를 해제합니다.
    void close();

    /// @brief 특정 타임스탬프로 이동 (FR-013)
    ///
    /// 파일 내 인덱스 블록을 이진 탐색하여 지정된 타임스탬프 이후의
    /// 첫 번째 메시지 위치로 파일 포인터를 이동합니다.
    ///
    /// @param timestamp_ns 나노초 단위 타임스탬프
    /// @throws std::runtime_error 인덱스가 손상된 경우
    /// @note 성능 목표: 1GB 파일에서 ~10ms 탐색 시간
    void seekTime(int64_t timestamp_ns);

    /// @brief 다음 메시지 읽기 (FR-012)
    ///
    /// 현재 파일 위치에서 다음 JSONL 메시지를 읽어서 BagMessage로 파싱합니다.
    /// 파일 끝에 도달하거나 파싱 오류 시 std::nullopt를 반환합니다.
    ///
    /// @return 읽은 메시지 (optional), 파일 끝이면 std::nullopt
    /// @note 손상된 라인은 복구 모드에 따라 스킵하거나 예외 발생
    std::optional<BagMessage> next();

    /// @brief 타임스탬프 범위 내 모든 메시지 읽기 (FR-016)
    ///
    /// 지정된 시간 범위 내의 모든 메시지를 벡터로 반환합니다.
    /// 내부적으로 seekTime + next 반복을 수행합니다.
    ///
    /// @param start_ns 시작 타임스탬프 (나노초)
    /// @param end_ns 종료 타임스탬프 (나노초)
    /// @return 메시지 벡터
    std::vector<BagMessage> getMessagesInRange(int64_t start_ns, int64_t end_ns);

    /// @brief 파일 손상 복구 모드 설정 (FR-024)
    ///
    /// 복구 모드가 활성화되면, 손상된 JSONL 라인을 스킵하고
    /// 다음 유효한 메시지부터 읽기를 계속합니다.
    ///
    /// @param enabled true: 복구 모드, false: 예외 발생
    /// @note 기본값은 true (복구 모드 활성화)
    void setRecoveryMode(bool enabled);

    /// @brief 총 메시지 개수 조회
    ///
    /// footer의 index_count 값을 반환합니다.
    ///
    /// @return 총 메시지 개수
    /// @throws std::runtime_error 파일이 열리지 않은 경우
    uint32_t getMessageCount() const;

    /// @brief 파일 시간 범위 조회
    ///
    /// 인덱스의 첫 번째와 마지막 타임스탬프를 반환합니다.
    ///
    /// @return pair<start_ns, end_ns>
    /// @throws std::runtime_error 파일이 열리지 않은 경우
    std::pair<int64_t, int64_t> getTimeRange() const;

    /// @brief 현재 읽기 진행률 조회 (0.0 ~ 1.0)
    ///
    /// 현재 파일 포인터 위치를 data_size로 나눈 값을 반환합니다.
    ///
    /// @return 진행률 (0.0 = 시작, 1.0 = 끝)
    double getProgress() const;

    /// @brief 인덱스 무결성 검증
    ///
    /// 인덱스 블록의 타임스탬프가 단조 증가하는지 확인합니다.
    /// CRC32 체크섬도 검증합니다.
    ///
    /// @return true if valid, false otherwise
    bool validateIndex() const;

    /// @brief 파일이 열려있는지 확인
    ///
    /// @return true if open, false otherwise
    bool isOpen() const;

private:
    std::string filepath_;
    FILE* file_ = nullptr;
    BagFooter footer_;
    std::vector<IndexEntry> index_;
    bool recoveryMode_ = true;
    uint64_t currentOffset_ = 0;

    /// @brief 인덱스 블록 로드
    void loadIndex();

    /// @brief Footer 읽기 및 검증
    void readFooter();

    /// @brief JSONL 라인 파싱
    std::optional<BagMessage> parseJsonLine(const std::string& line);

    /// @brief 이진 탐색으로 타임스탬프 위치 찾기
    size_t binarySearchTimestamp(int64_t timestamp_ns) const;
};

} // namespace mxrc::core::logging
