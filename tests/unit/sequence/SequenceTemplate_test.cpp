// SequenceTemplate 템플릿 시스템 테스트

#include "gtest/gtest.h"
#include "core/sequence/core/SequenceRegistry.h"
#include "core/sequence/core/SequenceEngine.h"
#include "core/sequence/core/SequenceTemplate.h"
#include "MockActions.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <memory>

using namespace mxrc::core::sequence;
using namespace mxrc::core::sequence::testing;

class SequenceTemplateTest : public ::testing::Test {
public:
    static void SetUpTestSuite() {
        // spdlog 초기화
        try {
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            auto logger = std::make_shared<spdlog::logger>("mxrc", console_sink);
            logger->set_level(spdlog::level::debug);
            spdlog::register_logger(logger);
        } catch (const spdlog::spdlog_ex&) {
            // 로거가 이미 존재
        }
    }

protected:
    void SetUp() override {
        registry_ = std::make_shared<SequenceRegistry>();
        actionFactory_ = std::make_shared<MockActionFactory>();
        engine_ = std::make_shared<SequenceEngine>(registry_, actionFactory_);
    }

    std::shared_ptr<SequenceRegistry> registry_;
    std::shared_ptr<IActionFactory> actionFactory_;
    std::shared_ptr<SequenceEngine> engine_;
};

/**
 * @brief 기본 템플릿 등록
 *
 * 템플릿을 정의하고 레지스트리에 등록하는 기본 시나리오
 */
TEST_F(SequenceTemplateTest, RegisterBasicTemplate) {
    // 템플릿 정의
    SequenceTemplate pickAndPlace;
    pickAndPlace.id = "pick_and_place";
    pickAndPlace.name = "Pick and Place";
    pickAndPlace.version = "1.0.0";
    pickAndPlace.description = "Pick object at location and place at target";

    // 파라미터 정의
    TemplateParameter sourceParam;
    sourceParam.name = "source_x";
    sourceParam.type = "float";
    sourceParam.required = true;
    sourceParam.description = "Source X coordinate";

    TemplateParameter targetParam;
    targetParam.name = "target_x";
    targetParam.type = "float";
    targetParam.required = true;
    targetParam.description = "Target X coordinate";

    pickAndPlace.parameters = {sourceParam, targetParam};

    // 액션 정의 (파라미터 플레이스홀더 포함)
    pickAndPlace.actionIds = {
        "move_to_${source_x}",
        "gripper_open",
        "move_to_${target_x}",
        "gripper_close"
    };

    // 등록
    registry_->registerTemplate(pickAndPlace);

    // 검증
    EXPECT_TRUE(registry_->hasTemplate("pick_and_place"));
    auto retrieved = registry_->getTemplate("pick_and_place");
    EXPECT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->name, "Pick and Place");
    EXPECT_EQ(retrieved->actionIds.size(), 4);
    EXPECT_EQ(retrieved->parameters.size(), 2);
}

/**
 * @brief 템플릿 인스턴스화
 *
 * 템플릿에서 파라미터를 제공하여 구체적인 시퀀스 인스턴스 생성
 */
TEST_F(SequenceTemplateTest, BasicTemplateInstantiation) {
    // 템플릿 정의 및 등록
    SequenceTemplate template_;
    template_.id = "simple_move";
    template_.name = "Simple Move";
    template_.version = "1.0.0";

    TemplateParameter locParam;
    locParam.name = "location";
    locParam.type = "string";
    locParam.required = true;

    template_.parameters = {locParam};
    template_.actionIds = {"move_to_${location}", "wait"};

    registry_->registerTemplate(template_);

    // 인스턴스화
    std::map<std::string, std::any> params;
    params["location"] = std::string("home");

    auto result = engine_->instantiateTemplate("simple_move", params, "move_to_home");

    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.instanceId.empty());
    EXPECT_EQ(result.errorMessage, "");
}

/**
 * @brief 파라미터 치환 검증
 *
 * ${paramName} 형식의 플레이스홀더가 실제 값으로 치환되는지 검증
 */
TEST_F(SequenceTemplateTest, ParameterSubstitution) {
    // 템플릿
    SequenceTemplate template_;
    template_.id = "pick_from_location";
    template_.name = "Pick From Location";
    template_.version = "1.0.0";

    TemplateParameter x, y, z;
    x.name = "x"; x.type = "float"; x.required = true;
    y.name = "y"; y.type = "float"; y.required = true;
    z.name = "z"; z.type = "float"; z.required = true;

    template_.parameters = {x, y, z};
    template_.actionIds = {"move_to_${x}_${y}_${z}", "pick"};

    registry_->registerTemplate(template_);

    // 파라미터 제공
    std::map<std::string, std::any> params;
    params["x"] = 100.5f;
    params["y"] = 200.3f;
    params["z"] = 50.2f;

    auto result = engine_->instantiateTemplate("pick_from_location", params);

    EXPECT_TRUE(result.success);

    // 인스턴스 조회 및 액션 검증
    auto instance = registry_->getTemplateInstance(result.instanceId);
    EXPECT_NE(instance, nullptr);
    EXPECT_NE(instance->sequenceDefinition, nullptr);

    // 첫 액션이 실제로 파라미터가 치환되었는지 확인
    const auto& actionIds = instance->sequenceDefinition->actionIds;
    EXPECT_EQ(actionIds.size(), 2);
    // 정확한 치환값은 float to string 변환에 따라 다를 수 있으므로
    // 패턴 확인만 함 (숫자 포함 여부)
    EXPECT_NE(actionIds[0].find("100"), std::string::npos);  // x 값 확인
    EXPECT_EQ(actionIds[1], "pick");  // 두 번째 액션은 pick 그대로
}

/**
 * @brief 필수 파라미터 검증
 *
 * 필수 파라미터가 누락되면 인스턴스화 실패
 */
TEST_F(SequenceTemplateTest, MissingRequiredParameter) {
    // 템플릿
    SequenceTemplate template_;
    template_.id = "load_with_weight";
    template_.name = "Load With Weight";
    template_.version = "1.0.0";

    TemplateParameter weightParam;
    weightParam.name = "weight";
    weightParam.type = "int";
    weightParam.required = true;  // 필수 파라미터

    template_.parameters = {weightParam};
    template_.actionIds = {"lift_${weight}", "hold"};

    registry_->registerTemplate(template_);

    // 파라미터 없이 인스턴스화 시도
    std::map<std::string, std::any> emptyParams;
    auto result = engine_->instantiateTemplate("load_with_weight", emptyParams);

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorMessage, "Parameter validation failed");
    EXPECT_NE(result.validationErrors.size(), 0);
}

/**
 * @brief 기본값 파라미터
 *
 * 선택 파라미터는 누락되어도 됨
 */
TEST_F(SequenceTemplateTest, OptionalParameterWithDefault) {
    // 템플릿
    SequenceTemplate template_;
    template_.id = "move_with_speed";
    template_.name = "Move With Speed";
    template_.version = "1.0.0";

    TemplateParameter speedParam;
    speedParam.name = "speed";
    speedParam.type = "int";
    speedParam.required = false;  // 선택 파라미터
    speedParam.defaultValue = "100";

    template_.parameters = {speedParam};
    template_.actionIds = {"set_speed_${speed}", "move"};

    registry_->registerTemplate(template_);

    // 파라미터 없이 인스턴스화 (기본값으로 처리)
    std::map<std::string, std::any> params;
    auto result = engine_->instantiateTemplate("move_with_speed", params);

    EXPECT_TRUE(result.success);
}

/**
 * @brief 템플릿 인스턴스 추적
 *
 * 동일 템플릿에서 생성된 여러 인스턴스를 추적
 */
TEST_F(SequenceTemplateTest, TrackMultipleInstances) {
    // 템플릿
    SequenceTemplate template_;
    template_.id = "assemble";
    template_.name = "Assembly Task";
    template_.version = "1.0.0";

    TemplateParameter partParam;
    partParam.name = "part_id";
    partParam.type = "string";
    partParam.required = true;

    template_.parameters = {partParam};
    template_.actionIds = {"fetch_${part_id}", "assemble"};

    registry_->registerTemplate(template_);

    // 여러 인스턴스 생성
    std::vector<std::string> instanceIds;
    for (int i = 0; i < 3; ++i) {
        std::map<std::string, std::any> params;
        params["part_id"] = std::string("part_") + std::to_string(i);

        auto result = engine_->instantiateTemplate("assemble", params);
        EXPECT_TRUE(result.success);
        instanceIds.push_back(result.instanceId);
    }

    // 템플릿으로부터 생성된 모든 인스턴스 조회
    auto instances = registry_->getTemplateInstances("assemble");
    EXPECT_GE(instances.size(), 3);  // 최소 3개 이상

    // 각 인스턴스 검증
    for (const auto& instanceId : instanceIds) {
        auto instance = registry_->getTemplateInstance(instanceId);
        EXPECT_NE(instance, nullptr);
        EXPECT_EQ(instance->templateId, "assemble");
    }
}

/**
 * @brief 복합 파라미터 타입
 *
 * int, float, bool, string 등 다양한 타입 지원
 */
TEST_F(SequenceTemplateTest, MultipleParameterTypes) {
    // 템플릿
    SequenceTemplate template_;
    template_.id = "complex_task";
    template_.name = "Complex Task";
    template_.version = "1.0.0";

    TemplateParameter intParam, floatParam, boolParam, stringParam;
    intParam.name = "count"; intParam.type = "int"; intParam.required = true;
    floatParam.name = "temperature"; floatParam.type = "float"; floatParam.required = true;
    boolParam.name = "enabled"; boolParam.type = "bool"; boolParam.required = true;
    stringParam.name = "mode"; stringParam.type = "string"; stringParam.required = true;

    template_.parameters = {intParam, floatParam, boolParam, stringParam};
    template_.actionIds = {
        "configure_${count}_${temperature}_${enabled}_${mode}"
    };

    registry_->registerTemplate(template_);

    // 다양한 타입의 파라미터 제공
    std::map<std::string, std::any> params;
    params["count"] = 10;
    params["temperature"] = 95.5f;
    params["enabled"] = true;
    params["mode"] = std::string("fast");

    auto result = engine_->instantiateTemplate("complex_task", params);

    EXPECT_TRUE(result.success);

    auto instance = registry_->getTemplateInstance(result.instanceId);
    EXPECT_NE(instance, nullptr);
    EXPECT_EQ(instance->parameters.size(), 4);
}

/**
 * @brief 템플릿 인스턴스 실행
 *
 * 생성된 인스턴스를 직접 실행
 */
TEST_F(SequenceTemplateTest, ExecuteTemplateInstance) {
    // 템플릿
    SequenceTemplate template_;
    template_.id = "execute_test";
    template_.name = "Execute Test";
    template_.version = "1.0.0";

    TemplateParameter actionParam;
    actionParam.name = "action_type";
    actionParam.type = "string";
    actionParam.required = true;

    template_.parameters = {actionParam};
    template_.actionIds = {"${action_type}"};

    registry_->registerTemplate(template_);

    // 파라미터로 success 액션 지정
    std::map<std::string, std::any> params;
    params["action_type"] = std::string("success");

    // 템플릿 직접 실행
    std::string executionId = engine_->executeTemplate("execute_test", params);

    // 실행이 성공했는지 확인
    EXPECT_FALSE(executionId.empty());
}

/**
 * @brief 템플릿 삭제
 *
 * 템플릿 및 관련 인스턴스 삭제
 */
TEST_F(SequenceTemplateTest, DeleteTemplate) {
    // 템플릿 등록
    SequenceTemplate template_;
    template_.id = "temp_task";
    template_.name = "Temporary Task";
    template_.version = "1.0.0";
    template_.actionIds = {"dummy_action"};

    registry_->registerTemplate(template_);
    EXPECT_TRUE(registry_->hasTemplate("temp_task"));

    // 인스턴스 생성
    std::map<std::string, std::any> params;
    auto result = engine_->instantiateTemplate("temp_task", params);
    EXPECT_TRUE(result.success);

    // 템플릿 삭제
    bool deleted = registry_->removeTemplate("temp_task");
    EXPECT_TRUE(deleted);
    EXPECT_FALSE(registry_->hasTemplate("temp_task"));

    // 인스턴스도 함께 삭제됨
    auto instances = registry_->getTemplateInstances("temp_task");
    EXPECT_EQ(instances.size(), 0);
}

/**
 * @brief 동일 액션 ID에 여러 파라미터 치환
 *
 * 하나의 액션 ID에 여러 파라미터 플레이스홀더가 있는 경우
 */
TEST_F(SequenceTemplateTest, MultipleSubstitutionsInSingleAction) {
    // 템플릿
    SequenceTemplate template_;
    template_.id = "multi_param_action";
    template_.name = "Multi Param Action";
    template_.version = "1.0.0";

    TemplateParameter xParam, yParam;
    xParam.name = "x"; xParam.type = "float"; xParam.required = true;
    yParam.name = "y"; yParam.type = "float"; yParam.required = true;

    template_.parameters = {xParam, yParam};
    template_.actionIds = {"move_to_${x}_${y}_position"};

    registry_->registerTemplate(template_);

    // 파라미터 제공
    std::map<std::string, std::any> params;
    params["x"] = 10.5f;
    params["y"] = 20.5f;

    auto result = engine_->instantiateTemplate("multi_param_action", params);

    EXPECT_TRUE(result.success);

    auto instance = registry_->getTemplateInstance(result.instanceId);
    EXPECT_NE(instance, nullptr);

    const auto& actionIds = instance->sequenceDefinition->actionIds;
    EXPECT_EQ(actionIds.size(), 1);

    // 액션 ID에 파라미터 값이 포함되었는지 확인
    EXPECT_NE(actionIds[0].find("10"), std::string::npos);  // x 값
    EXPECT_NE(actionIds[0].find("20"), std::string::npos);  // y 값
    EXPECT_NE(actionIds[0].find("position"), std::string::npos);  // 리터럴
}

/**
 * @brief 템플릿 조회
 *
 * 등록된 모든 템플릿 ID 목록 조회
 */
TEST_F(SequenceTemplateTest, QueryAllTemplates) {
    // 여러 템플릿 등록
    for (int i = 0; i < 5; ++i) {
        SequenceTemplate template_;
        template_.id = "template_" + std::to_string(i);
        template_.name = "Template " + std::to_string(i);
        template_.version = "1.0.0";
        template_.actionIds = {"action_" + std::to_string(i)};

        registry_->registerTemplate(template_);
    }

    // 모든 템플릿 ID 조회
    auto templateIds = registry_->getAllTemplateIds();

    EXPECT_GE(templateIds.size(), 5);

    // 각 템플릿이 실제로 존재하는지 확인
    for (int i = 0; i < 5; ++i) {
        std::string id = "template_" + std::to_string(i);
        auto it = std::find(templateIds.begin(), templateIds.end(), id);
        EXPECT_NE(it, templateIds.end());
    }
}
