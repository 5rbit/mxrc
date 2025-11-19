// DataStoreEventAdapter.h - DataStore와 EventBus 연동 어댑터
// Copyright (C) 2025 MXRC Project

#ifndef MXRC_CORE_EVENT_ADAPTERS_DATASTOREEVENTADAPTER_H
#define MXRC_CORE_EVENT_ADAPTERS_DATASTOREEVENTADAPTER_H

#include "core/datastore/DataStore.h"
#include "core/event/interfaces/IEventBus.h"
#include "core/event/dto/DataStoreEvents.h"
#include "core/event/dto/ActionEvents.h"
#include "core/event/util/EventFilter.h"
#include <memory>
#include <string>
#include <mutex>
#include <set>

namespace mxrc::core::event {

/**
 * @brief DataStore와 EventBus 간의 양방향 연동 어댑터
 *
 * - DataStore 변경 → EventBus 이벤트 발행
 * - EventBus 이벤트 → DataStore 업데이트
 * - 순환 업데이트 방지 기능 포함
 */
class DataStoreEventAdapter : public Observer, public std::enable_shared_from_this<DataStoreEventAdapter> {
public:
    /**
     * @brief 생성자
     *
     * @param dataStore DataStore 인스턴스
     * @param eventBus EventBus 인스턴스
     */
    DataStoreEventAdapter(
        std::shared_ptr<DataStore> dataStore,
        std::shared_ptr<IEventBus> eventBus);

    ~DataStoreEventAdapter();

    /**
     * @brief DataStore 변경 알림 처리 (Observer 인터페이스 구현)
     *
     * @param changed_data 변경된 데이터
     */
    void onDataChanged(const SharedData& changed_data) override;

    /**
     * @brief Action 결과를 DataStore에 자동 저장하도록 구독
     *
     * ACTION_COMPLETED 이벤트를 수신하여 결과를 DataStore에 저장합니다.
     * @param keyPrefix DataStore 키 접두사 (예: "action.results.")
     */
    void subscribeToActionResults(const std::string& keyPrefix = "action.results.");

    /**
     * @brief Sequence 결과를 DataStore에 자동 저장하도록 구독
     *
     * SEQUENCE_COMPLETED 이벤트를 수신하여 결과를 DataStore에 저장합니다.
     * @param keyPrefix DataStore 키 접두사 (예: "sequence.results.")
     */
    void subscribeToSequenceResults(const std::string& keyPrefix = "sequence.results.");

    /**
     * @brief DataStore 감시 시작
     *
     * @param keyPattern 감시할 키 패턴 (예: "robot.*")
     */
    void startWatching(const std::string& keyPattern);

    /**
     * @brief DataStore 감시 중지
     *
     * @param keyPattern 중지할 키 패턴
     */
    void stopWatching(const std::string& keyPattern);

private:
    std::shared_ptr<DataStore> dataStore_;
    std::shared_ptr<IEventBus> eventBus_;

    // 순환 업데이트 방지를 위한 플래그
    std::mutex updateMutex_;
    std::set<std::string> updatingKeys_;  // 현재 업데이트 중인 키 목록

    // 구독 ID 관리
    std::vector<SubscriptionId> subscriptionIds_;

    /**
     * @brief DataStore 값을 문자열로 변환
     *
     * @param data SharedData 객체
     * @return 문자열로 변환된 값
     */
    std::string valueToString(const SharedData& data);

    /**
     * @brief DataType을 문자열로 변환
     *
     * @param type DataType
     * @return 타입 문자열
     */
    std::string dataTypeToString(DataType type);

    /**
     * @brief 순환 업데이트인지 확인
     *
     * @param key 확인할 키
     * @return 순환 업데이트이면 true
     */
    bool isCircularUpdate(const std::string& key);

    /**
     * @brief 업데이트 시작 표시
     *
     * @param key 업데이트할 키
     */
    void markUpdating(const std::string& key);

    /**
     * @brief 업데이트 완료 표시
     *
     * @param key 업데이트 완료한 키
     */
    void unmarkUpdating(const std::string& key);
};

} // namespace mxrc::core::event

#endif // MXRC_CORE_EVENT_ADAPTERS_DATASTOREEVENTADAPTER_H
