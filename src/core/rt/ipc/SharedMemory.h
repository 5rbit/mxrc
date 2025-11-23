#pragma once

#include <cstddef>
#include <string>

namespace mxrc {
namespace core {
namespace rt {
namespace ipc {

// 공유 메모리 영역
// POSIX shm_open/mmap 기반
class SharedMemoryRegion {
public:
    SharedMemoryRegion();
    ~SharedMemoryRegion();

    // 공유 메모리 생성 (서버 측)
    // name: 공유 메모리 이름 (예: "/mxrc_rtdata")
    // size: 크기 (바이트)
    // 반환: 성공 0, 실패 -1
    int create(const std::string& name, size_t size);

    // 공유 메모리 열기 (클라이언트 측)
    // name: 공유 메모리 이름
    // 반환: 성공 0, 실패 -1
    int open(const std::string& name);

    // 공유 메모리 해제
    void close();

    // 공유 메모리 삭제 (서버 측에서 cleanup 시)
    // name: 공유 메모리 이름
    // 반환: 성공 0, 실패 -1
    static int unlink(const std::string& name);

    // 매핑된 메모리 포인터
    void* getPtr() const { return ptr_; }

    // 크기
    size_t getSize() const { return size_; }

    // 유효 여부
    bool isValid() const { return ptr_ != nullptr; }

private:
    void* ptr_;
    size_t size_;
    int fd_;
    std::string name_;
};

} // namespace ipc
} // namespace rt
} // namespace core
} // namespace mxrc
