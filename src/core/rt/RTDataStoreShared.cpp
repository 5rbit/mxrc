#include "RTDataStoreShared.h"
#include <spdlog/spdlog.h>
#include <new>

namespace mxrc {
namespace core {
namespace rt {

RTDataStoreShared::RTDataStoreShared()
    : shm_(std::make_unique<ipc::SharedMemoryRegion>()), store_(nullptr) {}

RTDataStoreShared::~RTDataStoreShared() {
    // store_는 placement new로 생성했으므로 명시적 소멸자 호출
    if (store_ != nullptr) {
        store_->~RTDataStore();
        store_ = nullptr;
    }
}

int RTDataStoreShared::createShared(const std::string& name) {
    if (store_ != nullptr) {
        spdlog::warn("RTDataStoreShared already created");
        return -1;
    }

    // RTDataStore 크기만큼 공유 메모리 생성
    size_t size = sizeof(RTDataStore);
    if (shm_->create(name, size) != 0) {
        return -1;
    }

    // placement new로 공유 메모리에 RTDataStore 생성
    void* ptr = shm_->getPtr();
    store_ = new (ptr) RTDataStore();

    spdlog::info("RTDataStoreShared created in shared memory: {}", name);
    return 0;
}

int RTDataStoreShared::openShared(const std::string& name) {
    if (store_ != nullptr) {
        spdlog::warn("RTDataStoreShared already opened");
        return -1;
    }

    // 공유 메모리 열기
    if (shm_->open(name) != 0) {
        return -1;
    }

    // 공유 메모리 크기 확인
    size_t expected_size = sizeof(RTDataStore);
    if (shm_->getSize() < expected_size) {
        spdlog::error("Shared memory size mismatch: expected={}, actual={}",
                      expected_size, shm_->getSize());
        return -1;
    }

    // 공유 메모리에 이미 존재하는 RTDataStore 포인터 설정
    store_ = reinterpret_cast<RTDataStore*>(shm_->getPtr());

    spdlog::info("RTDataStoreShared opened from shared memory: {}", name);
    return 0;
}

RTDataStore* RTDataStoreShared::getDataStore() {
    return store_;
}

const RTDataStore* RTDataStoreShared::getDataStore() const {
    return store_;
}

int RTDataStoreShared::unlinkShared(const std::string& name) {
    return ipc::SharedMemoryRegion::unlink(name);
}

} // namespace rt
} // namespace core
} // namespace mxrc
