#pragma once

#include "core/ethercat/interfaces/IEtherCATMaster.h"
#include <vector>
#include <cstring>
#include <memory>

namespace mxrc {
namespace ethercat {
namespace test {

// 테스트용 가상 EtherCAT Master
// 실제 하드웨어 없이 PDO 데이터 읽기/쓰기 테스트 가능
class MockEtherCATMaster : public IEtherCATMaster {
public:
    MockEtherCATMaster()
        : active_(false)
        , error_count_(0)
        , domain_data_(1024, 0)  // 1KB PDO domain
        , initialize_called_(false)
        , activate_called_(false)
        , send_called_(false)
        , receive_called_(false) {
    }

    ~MockEtherCATMaster() override = default;

    // IEtherCATMaster 인터페이스 구현
    int initialize() override {
        initialize_called_ = true;
        return 0;
    }

    int activate() override {
        activate_called_ = true;
        active_ = true;
        return 0;
    }

    int deactivate() override {
        active_ = false;
        return 0;
    }

    int send() override {
        send_called_ = true;
        if (!active_) return -1;
        return 0;
    }

    int receive() override {
        receive_called_ = true;
        if (!active_) return -1;
        return 0;
    }

    bool isActive() const override {
        return active_;
    }

    uint32_t getErrorCount() const override {
        return error_count_;
    }

    // 테스트 헬퍼: PDO domain 데이터 설정
    void setDomainData(uint32_t offset, const void* data, size_t size) {
        if (offset + size <= domain_data_.size()) {
            std::memcpy(domain_data_.data() + offset, data, size);
        }
    }

    // 테스트 헬퍼: PDO domain 데이터 읽기
    void getDomainData(uint32_t offset, void* data, size_t size) const {
        if (offset + size <= domain_data_.size()) {
            std::memcpy(data, domain_data_.data() + offset, size);
        }
    }

    // 테스트 헬퍼: PDO domain 포인터 반환
    uint8_t* getDomainPtr() {
        return domain_data_.data();
    }

    const uint8_t* getDomainPtr() const {
        return domain_data_.data();
    }

    // 테스트 검증: 메서드 호출 여부 확인
    bool wasInitializeCalled() const { return initialize_called_; }
    bool wasActivateCalled() const { return activate_called_; }
    bool wasSendCalled() const { return send_called_; }
    bool wasReceiveCalled() const { return receive_called_; }

    // 테스트 헬퍼: 에러 카운트 시뮬레이션
    void setErrorCount(uint32_t count) {
        error_count_ = count;
    }

    // 테스트 헬퍼: 호출 플래그 리셋
    void resetCallFlags() {
        initialize_called_ = false;
        activate_called_ = false;
        send_called_ = false;
        receive_called_ = false;
    }

private:
    bool active_;
    uint32_t error_count_;
    std::vector<uint8_t> domain_data_;

    // 메서드 호출 추적
    bool initialize_called_;
    bool activate_called_;
    bool send_called_;
    bool receive_called_;
};

} // namespace test
} // namespace ethercat
} // namespace mxrc
