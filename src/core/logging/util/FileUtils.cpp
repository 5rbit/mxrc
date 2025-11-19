#include "util/FileUtils.h"
#include <spdlog/spdlog.h>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <regex>

namespace fs = std::filesystem;

namespace mxrc::core::logging {

bool FileUtils::fileExists(const std::string& filepath) {
    return fs::exists(filepath) && fs::is_regular_file(filepath);
}

bool FileUtils::directoryExists(const std::string& dirpath) {
    return fs::exists(dirpath) && fs::is_directory(dirpath);
}

bool FileUtils::createDirectories(const std::string& dirpath) {
    try {
        return fs::create_directories(dirpath);
    } catch (const fs::filesystem_error& e) {
        spdlog::error("Failed to create directories {}: {}", dirpath, e.what());
        return false;
    }
}

uint64_t FileUtils::getAvailableSpace(const std::string& path) {
    try {
        auto spaceInfo = fs::space(path);
        return spaceInfo.available;
    } catch (const fs::filesystem_error& e) {
        spdlog::error("Failed to get available space for {}: {}", path, e.what());
        return 0;
    }
}

uint64_t FileUtils::getFileSize(const std::string& filepath) {
    if (!fileExists(filepath)) {
        throw fs::filesystem_error("File does not exist",
                                   fs::path(filepath),
                                   std::make_error_code(std::errc::no_such_file_or_directory));
    }
    return fs::file_size(filepath);
}

std::vector<std::string> FileUtils::listFiles(const std::string& dirpath,
                                               const std::string& pattern) {
    std::vector<std::string> files;

    if (!directoryExists(dirpath)) {
        spdlog::warn("Directory does not exist: {}", dirpath);
        return files;
    }

    try {
        // 패턴을 정규식으로 변환 (간단한 와일드카드 지원)
        std::string regexPattern = pattern;
        std::replace(regexPattern.begin(), regexPattern.end(), '*', '.');
        regexPattern = ".*" + regexPattern;

        std::regex fileRegex(regexPattern);

        for (const auto& entry : fs::directory_iterator(dirpath)) {
            if (entry.is_regular_file()) {
                std::string filename = entry.path().filename().string();
                if (pattern == "*" || std::regex_match(filename, fileRegex)) {
                    files.push_back(entry.path().string());
                }
            }
        }

        // 생성 시간 기준 정렬 (오래된 파일이 앞에)
        std::sort(files.begin(), files.end(), [](const std::string& a, const std::string& b) {
            auto timeA = fs::last_write_time(a);
            auto timeB = fs::last_write_time(b);
            return timeA < timeB;
        });

    } catch (const fs::filesystem_error& e) {
        spdlog::error("Failed to list files in {}: {}", dirpath, e.what());
    }

    return files;
}

bool FileUtils::deleteFile(const std::string& filepath) {
    try {
        if (fileExists(filepath)) {
            return fs::remove(filepath);
        }
        return false;
    } catch (const fs::filesystem_error& e) {
        spdlog::error("Failed to delete file {}: {}", filepath, e.what());
        return false;
    }
}

uint64_t FileUtils::getLastModifiedTime(const std::string& filepath) {
    if (!fileExists(filepath)) {
        throw fs::filesystem_error("File does not exist",
                                   fs::path(filepath),
                                   std::make_error_code(std::errc::no_such_file_or_directory));
    }

    auto ftime = fs::last_write_time(filepath);
    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now()
    );
    return std::chrono::system_clock::to_time_t(sctp);
}

bool FileUtils::isDiskSpaceInsufficient(const std::string& path, uint64_t requiredBytes) {
    uint64_t available = getAvailableSpace(path);
    return available < requiredBytes;
}

std::string FileUtils::generateTimestampedFilename(const std::string& baseName,
                                                    const std::string& extension) {
    // 현재 시간 조회 (밀리초 포함)
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()
    ) % 1000;

    // 포맷팅: YYYY-MM-DD_HH-MM-SS-mmm (밀리초 3자리 추가)
    std::stringstream ss;
    ss << baseName << "_"
       << std::put_time(std::localtime(&time_t_now), "%Y-%m-%d_%H-%M-%S")
       << "-" << std::setfill('0') << std::setw(3) << ms.count()
       << "." << extension;

    return ss.str();
}

} // namespace mxrc::core::logging
