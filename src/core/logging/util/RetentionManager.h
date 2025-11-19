#ifndef MXRC_CORE_LOGGING_UTIL_RETENTIONMANAGER_H
#define MXRC_CORE_LOGGING_UTIL_RETENTIONMANAGER_H

#include "dto/RetentionPolicy.h"
#include "util/FileUtils.h"
#include <string>
#include <vector>

namespace mxrc::core::logging {

/**
 * @brief Bag 파일 보존 정책 관리자
 *
 * RetentionPolicy에 따라 오래된 Bag 파일을 자동으로 삭제합니다.
 * 디스크 공간 부족 시 긴급 삭제 기능도 제공합니다.
 */
class RetentionManager {
public:
    /**
     * @brief RetentionManager 생성자
     * @param bagDirectory Bag 파일이 저장된 디렉토리
     * @param policy 보존 정책
     */
    explicit RetentionManager(const std::string& bagDirectory,
                              const RetentionPolicy& policy = RetentionPolicy());

    /**
     * @brief 보존 정책 설정
     * @param policy 새 보존 정책
     */
    void setPolicy(const RetentionPolicy& policy);

    /**
     * @brief 보존 정책에 따라 오래된 파일 삭제
     * @return 삭제된 파일 개수
     */
    size_t deleteOldFiles();

    /**
     * @brief 긴급 모드: 가장 오래된 파일 삭제 (디스크 공간 확보)
     * @param requiredBytes 필요한 디스크 공간 (바이트)
     * @return 삭제된 파일 개수
     *
     * 디스크 공간이 부족할 때 호출됩니다.
     * 보존 정책과 무관하게 가장 오래된 파일부터 삭제합니다.
     */
    size_t emergencyDeleteOldest(uint64_t requiredBytes);

    /**
     * @brief Bag 파일 목록 조회
     * @return Bag 파일 경로 목록 (오래된 순)
     */
    std::vector<std::string> listBagFiles() const;

    /**
     * @brief 총 Bag 파일 크기 조회
     * @return 총 크기 (바이트)
     */
    uint64_t getTotalSize() const;

    /**
     * @brief 디스크 공간 확인 및 자동 정리
     * @param requiredBytes 필요한 디스크 공간
     * @return 공간이 충분하면 true
     *
     * 공간이 부족하면 자동으로 emergencyDeleteOldest()를 호출합니다.
     */
    bool ensureDiskSpace(uint64_t requiredBytes);

private:
    std::string bagDirectory_;      ///< Bag 파일 디렉토리
    RetentionPolicy policy_;        ///< 보존 정책
};

} // namespace mxrc::core::logging

#endif // MXRC_CORE_LOGGING_UTIL_RETENTIONMANAGER_H
