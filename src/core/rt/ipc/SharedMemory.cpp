#include "SharedMemory.h"
#include <spdlog/spdlog.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>

namespace mxrc {
namespace core {
namespace rt {
namespace ipc {

SharedMemoryRegion::SharedMemoryRegion()
    : ptr_(nullptr), size_(0), fd_(-1) {}

SharedMemoryRegion::~SharedMemoryRegion() {
    close();
}

int SharedMemoryRegion::create(const std::string& name, size_t size) {
    if (ptr_ != nullptr) {
        spdlog::warn("SharedMemory already created");
        return -1;
    }

    name_ = name;
    size_ = size;

    // 공유 메모리 객체 생성
    fd_ = shm_open(name.c_str(), O_CREAT | O_RDWR, 0666);
    if (fd_ == -1) {
        spdlog::error("shm_open failed: {}", strerror(errno));
        return -1;
    }

    // 크기 설정
    if (ftruncate(fd_, size) == -1) {
        spdlog::error("ftruncate failed: {}", strerror(errno));
        ::close(fd_);
        fd_ = -1;
        return -1;
    }

    // 메모리 매핑
    ptr_ = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
    if (ptr_ == MAP_FAILED) {
        spdlog::error("mmap failed: {}", strerror(errno));
        ptr_ = nullptr;
        ::close(fd_);
        fd_ = -1;
        return -1;
    }

    // 메모리 초기화
    std::memset(ptr_, 0, size);

    spdlog::info("SharedMemory created: name={}, size={} bytes", name, size);
    return 0;
}

int SharedMemoryRegion::open(const std::string& name) {
    if (ptr_ != nullptr) {
        spdlog::warn("SharedMemory already opened");
        return -1;
    }

    name_ = name;

    // 공유 메모리 객체 열기
    fd_ = shm_open(name.c_str(), O_RDWR, 0666);
    if (fd_ == -1) {
        spdlog::error("shm_open failed: {}", strerror(errno));
        return -1;
    }

    // 크기 확인
    struct stat sb;
    if (fstat(fd_, &sb) == -1) {
        spdlog::error("fstat failed: {}", strerror(errno));
        ::close(fd_);
        fd_ = -1;
        return -1;
    }
    size_ = sb.st_size;

    // 메모리 매핑
    ptr_ = mmap(nullptr, size_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
    if (ptr_ == MAP_FAILED) {
        spdlog::error("mmap failed: {}", strerror(errno));
        ptr_ = nullptr;
        ::close(fd_);
        fd_ = -1;
        return -1;
    }

    spdlog::info("SharedMemory opened: name={}, size={} bytes", name, size_);
    return 0;
}

void SharedMemoryRegion::close() {
    if (ptr_ != nullptr) {
        munmap(ptr_, size_);
        ptr_ = nullptr;
    }

    if (fd_ != -1) {
        ::close(fd_);
        fd_ = -1;
    }

    size_ = 0;
}

int SharedMemoryRegion::unlink(const std::string& name) {
    if (shm_unlink(name.c_str()) == -1) {
        spdlog::error("shm_unlink failed for {}: {}", name, strerror(errno));
        return -1;
    }

    spdlog::info("SharedMemory unlinked: {}", name);
    return 0;
}

} // namespace ipc
} // namespace rt
} // namespace core
} // namespace mxrc
