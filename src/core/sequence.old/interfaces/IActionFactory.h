#pragma once

#include <string>
#include <memory>
#include <map>

namespace mxrc::core::sequence {

class IAction;

/**
 * @brief 동작 생성 팩토리 인터페이스
 * 
 * 시퀀스에 필요한 동작들을 생성합니다.
 * 새로운 동작 타입을 추가할 때 이 팩토리를 구현합니다.
 */
class IActionFactory {
public:
    virtual ~IActionFactory() = default;
    
    /**
     * @brief 동작 생성
     * @param type 동작 타입
     * @param id 동작 고유 ID
     * @param params 동작 파라미터 맵
     * @return 생성된 동작 객체
     * @throws std::exception 미지원 타입 또는 파라미터 오류 시
     */
    virtual std::shared_ptr<IAction> createAction(
        const std::string& type,
        const std::string& id,
        const std::map<std::string, std::string>& params) = 0;
    
    /**
     * @brief 지원하는 동작 타입 조회
     * @return 지원되는 타입 목록
     */
    virtual std::vector<std::string> getSupportedTypes() const = 0;
};

} // namespace mxrc::core::sequence

