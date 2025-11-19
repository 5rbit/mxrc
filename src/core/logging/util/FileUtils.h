#ifndef MXRC_CORE_LOGGING_UTIL_FILEUTILS_H
#define MXRC_CORE_LOGGING_UTIL_FILEUTILS_H

#include <string>
#include <cstdint>
#include <vector>
#include <filesystem>

namespace mxrc::core::logging {

/**
 * @brief 파일 I/O 유틸리티 함수
 *
 * Bag 파일 관리를 위한 파일 시스템 조작 기능을 제공합니다.
 */
class FileUtils {
public:
    /**
     * @brief 파일 존재 여부 확인
     * @param filepath 파일 경로
     * @return 파일이 존재하면 true
     */
    static bool fileExists(const std::string& filepath);

    /**
     * @brief 디렉토리 존재 여부 확인
     * @param dirpath 디렉토리 경로
     * @return 디렉토리가 존재하면 true
     */
    static bool directoryExists(const std::string& dirpath);

    /**
     * @brief 디렉토리 생성 (부모 디렉토리도 함께 생성)
     * @param dirpath 디렉토리 경로
     * @return 성공하면 true
     */
    static bool createDirectories(const std::string& dirpath);

    /**
     * @brief 사용 가능한 디스크 공간 확인
     * @param path 파일 시스템 경로
     * @return 사용 가능한 바이트 수
     */
    static uint64_t getAvailableSpace(const std::string& path);

    /**
     * @brief 파일 크기 조회
     * @param filepath 파일 경로
     * @return 파일 크기 (바이트)
     * @throws std::filesystem::filesystem_error 파일이 없으면
     */
    static uint64_t getFileSize(const std::string& filepath);

    /**
     * @brief 디렉토리 내 파일 목록 조회 (패턴 매칭)
     * @param dirpath 디렉토리 경로
     * @param pattern 파일명 패턴 (예: "*.bag")
     * @return 파일 경로 목록 (생성 시간 오름차순 정렬)
     */
    static std::vector<std::string> listFiles(const std::string& dirpath,
                                               const std::string& pattern = "*");

    /**
     * @brief 파일 삭제
     * @param filepath 파일 경로
     * @return 성공하면 true
     */
    static bool deleteFile(const std::string& filepath);

    /**
     * @brief 파일 최종 수정 시간 조회
     * @param filepath 파일 경로
     * @return Unix 타임스탬프 (초)
     * @throws std::filesystem::filesystem_error 파일이 없으면
     */
    static uint64_t getLastModifiedTime(const std::string& filepath);

    /**
     * @brief 디스크 공간 부족 여부 확인
     * @param path 파일 시스템 경로
     * @param requiredBytes 필요한 바이트 수
     * @return 공간이 부족하면 true
     */
    static bool isDiskSpaceInsufficient(const std::string& path, uint64_t requiredBytes);

    /**
     * @brief 파일명에 타임스탬프 추가
     * @param baseName 기본 파일명 (예: "mission")
     * @param extension 확장자 (예: ".bag")
     * @return 타임스탬프가 추가된 파일명 (예: "mission_2025-11-19_14-30-00.bag")
     */
    static std::string generateTimestampedFilename(const std::string& baseName,
                                                    const std::string& extension = ".bag");
};

} // namespace mxrc::core::logging

#endif // MXRC_CORE_LOGGING_UTIL_FILEUTILS_H
