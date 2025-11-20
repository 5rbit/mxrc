#pragma once

#include "RTDataStore.h"
#include "ipc/SharedMemory.h"
#include <string>
#include <memory>

namespace mxrc {
namespace core {
namespace rt {

// 공유 메모리 기반 RTDataStore
// RT 프로세스와 Non-RT 프로세스 간 데이터 공유
class RTDataStoreShared {
public:
    RTDataStoreShared();
    ~RTDataStoreShared();

    // 서버 측 (RT 프로세스): 공유 메모리 생성 및 RTDataStore 초기화
    // name: 공유 메모리 이름 (예: "/mxrc_rtdata")
    // 반환: 성공 0, 실패 -1
    int createShared(const std::string& name);

    // 클라이언트 측 (Non-RT 프로세스): 공유 메모리 열기
    // name: 공유 메모리 이름
    // 반환: 성공 0, 실패 -1
    int openShared(const std::string& name);

    // RTDataStore 인터페이스
    RTDataStore* getDataStore();
    const RTDataStore* getDataStore() const;

    // 유효 여부
    bool isValid() const { return store_ != nullptr; }

    // 공유 메모리 삭제 (정리 시)
    static int unlinkShared(const std::string& name);

private:
    std::unique_ptr<ipc::SharedMemoryRegion> shm_;
    RTDataStore* store_;  // 공유 메모리에 매핑된 포인터
};

} // namespace rt
} // namespace core
} // namespace mxrc
