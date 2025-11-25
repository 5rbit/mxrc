#include "PalletTransportSequence.h"
#include <spdlog/spdlog.h>

namespace mxrc::robot::pallet_shuttle {

using namespace mxrc::core::sequence;

PalletTransportSequence::PalletTransportSequence()
    : definition_("pallet_transport", "Pallet Transport Sequence") {
    buildDefinition();
}

PalletTransportSequence::PalletTransportSequence(
    double pickup_x,
    double pickup_y,
    double place_x,
    double place_y,
    const std::string& pallet_id
) : pickup_x_(pickup_x),
    pickup_y_(pickup_y),
    place_x_(place_x),
    place_y_(place_y),
    pallet_id_(pallet_id),
    definition_("pallet_transport", "Pallet Transport Sequence") {
    buildDefinition();
}

void PalletTransportSequence::buildDefinition() {
    // 1. 픽업 위치로 이동
    ActionStep move_to_pickup("move_to_pickup", "MoveToPosition");
    move_to_pickup.addParameter("target_x", std::to_string(static_cast<int>(pickup_x_)));
    move_to_pickup.addParameter("target_y", std::to_string(static_cast<int>(pickup_y_)));
    move_to_pickup.addParameter("target_theta", "0.0");
    move_to_pickup.setTimeout(std::chrono::seconds(30));

    // 2. 팔렛 픽업
    ActionStep pick("pick_pallet", "PickPallet");
    pick.addParameter("pallet_id", pallet_id_);
    pick.setTimeout(std::chrono::seconds(10));

    // 3. 배치 위치로 이동
    ActionStep move_to_place("move_to_place", "MoveToPosition");
    move_to_place.addParameter("target_x", std::to_string(static_cast<int>(place_x_)));
    move_to_place.addParameter("target_y", std::to_string(static_cast<int>(place_y_)));
    move_to_place.addParameter("target_theta", "0.0");
    move_to_place.setTimeout(std::chrono::seconds(30));

    // 4. 팔렛 배치
    ActionStep place("place_pallet", "PlacePallet");
    place.setTimeout(std::chrono::seconds(10));

    // Sequence 구성
    definition_.addStep(move_to_pickup)
               .addStep(pick)
               .addStep(move_to_place)
               .addStep(place);

    // 전체 타임아웃 설정 (각 단계 합 + 여유)
    definition_.setTimeout(std::chrono::seconds(90));

    // 재시도 정책 설정
    RetryPolicy retry;
    retry.maxRetries = 2;
    retry.retryDelay = std::chrono::seconds(5);
    definition_.retryPolicy = retry;

    definition_.setDescription("Transport pallet from pickup to placement location");

    spdlog::debug("[PalletTransportSequence] Definition built: {} steps",
                  definition_.steps.size());
}

SequenceDefinition PalletTransportSequence::getDefinition() const {
    return definition_;
}

} // namespace mxrc::robot::pallet_shuttle
