#pragma once

#include "core/sequence/dto/SequenceDefinition.h"
#include <string>

namespace mxrc::robot::pallet_shuttle {

/**
 * @brief 팔렛 운반 Sequence
 *
 * 픽업 위치로 이동 → 팔렛 픽업 → 배치 위치로 이동 → 팔렛 배치
 *
 * Feature 016: Pallet Shuttle Control System
 * Phase 5: User Story 1 - 팔렛 픽업 및 배치
 */
class PalletTransportSequence {
public:
    /**
     * @brief 기본 생성자 (테스트용)
     */
    PalletTransportSequence();

    /**
     * @brief 생성자
     *
     * @param pickup_x 픽업 위치 X
     * @param pickup_y 픽업 위치 Y
     * @param place_x 배치 위치 X
     * @param place_y 배치 위치 Y
     * @param pallet_id 팔렛 ID
     */
    PalletTransportSequence(
        double pickup_x,
        double pickup_y,
        double place_x,
        double place_y,
        const std::string& pallet_id
    );

    /**
     * @brief Sequence 정의 조회
     */
    mxrc::core::sequence::SequenceDefinition getDefinition() const;

private:
    void buildDefinition();

    double pickup_x_{100.0};
    double pickup_y_{200.0};
    double place_x_{300.0};
    double place_y_{400.0};
    std::string pallet_id_{"PALLET_DEFAULT"};
    mxrc::core::sequence::SequenceDefinition definition_;
};

} // namespace mxrc::robot::pallet_shuttle
