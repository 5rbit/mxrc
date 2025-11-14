#pragma once

#include "core/sequence/interfaces/IAction.h"
#include "core/sequence/interfaces/IActionFactory.h"
#include "core/sequence/core/ExecutionContext.h"
#include <string>
#include <map>
#include <any>
#include <memory>

namespace mxrc::core::sequence::testing {

/**
 * @brief 성공하는 테스트용 동작
 */
class SuccessAction : public IAction {
public:
    explicit SuccessAction(const std::string& id)
        : id_(id), status_(ActionStatus::PENDING), progress_(0.0f) {}

    std::string getId() const override { return id_; }
    std::string getType() const override { return "SuccessAction"; }

    void execute(ExecutionContext& context) override {
        status_ = ActionStatus::RUNNING;
        progress_ = 0.5f;

        // 결과 저장
        context.setActionResult(id_, 42);

        progress_ = 1.0f;
        status_ = ActionStatus::COMPLETED;
    }

    void cancel() override {
        if (status_ == ActionStatus::RUNNING) {
            status_ = ActionStatus::CANCELLED;
        }
    }

    ActionStatus getStatus() const override { return status_; }
    float getProgress() const override { return progress_; }
    std::string getDescription() const override { return "Mock success action"; }

private:
    std::string id_;
    ActionStatus status_;
    float progress_;
};

/**
 * @brief 실패하는 테스트용 동작
 */
class FailureAction : public IAction {
public:
    explicit FailureAction(const std::string& id)
        : id_(id), status_(ActionStatus::PENDING), progress_(0.0f) {}

    std::string getId() const override { return id_; }
    std::string getType() const override { return "FailureAction"; }

    void execute(ExecutionContext& context) override {
        status_ = ActionStatus::RUNNING;
        progress_ = 0.5f;
        progress_ = 1.0f;
        status_ = ActionStatus::FAILED;
    }

    void cancel() override {
        if (status_ == ActionStatus::RUNNING) {
            status_ = ActionStatus::CANCELLED;
        }
    }

    ActionStatus getStatus() const override { return status_; }
    float getProgress() const override { return progress_; }
    std::string getDescription() const override { return "Mock failure action"; }

private:
    std::string id_;
    ActionStatus status_;
    float progress_;
};

/**
 * @brief 결과를 저장하는 테스트용 동작
 */
class ResultStoringAction : public IAction {
public:
    explicit ResultStoringAction(const std::string& id, const std::any& result)
        : id_(id), result_(result), status_(ActionStatus::PENDING), progress_(0.0f) {}

    std::string getId() const override { return id_; }
    std::string getType() const override { return "ResultStoringAction"; }

    void execute(ExecutionContext& context) override {
        status_ = ActionStatus::RUNNING;
        progress_ = 0.5f;

        // 지정된 결과 저장
        context.setActionResult(id_, result_);

        progress_ = 1.0f;
        status_ = ActionStatus::COMPLETED;
    }

    void cancel() override {
        if (status_ == ActionStatus::RUNNING) {
            status_ = ActionStatus::CANCELLED;
        }
    }

    ActionStatus getStatus() const override { return status_; }
    float getProgress() const override { return progress_; }
    std::string getDescription() const override { return "Mock result storing action"; }

private:
    std::string id_;
    std::any result_;
    ActionStatus status_;
    float progress_;
};

/**
 * @brief 컨텍스트 변수를 읽고 수정하는 테스트용 동작
 */
class ContextModifyingAction : public IAction {
public:
    explicit ContextModifyingAction(const std::string& id)
        : id_(id), status_(ActionStatus::PENDING), progress_(0.0f) {}

    std::string getId() const override { return id_; }
    std::string getType() const override { return "ContextModifyingAction"; }

    void execute(ExecutionContext& context) override {
        status_ = ActionStatus::RUNNING;
        progress_ = 0.5f;

        // 컨텍스트 변수 읽기
        auto varValue = context.getVariable("input_var");
        if (varValue.has_value()) {
            try {
                int inputInt = std::any_cast<int>(varValue);
                // 결과에 입력값 두 배 저장
                context.setActionResult(id_, inputInt * 2);
            } catch (...) {
                context.setActionResult(id_, 0);
            }
        }

        progress_ = 1.0f;
        status_ = ActionStatus::COMPLETED;
    }

    void cancel() override {
        if (status_ == ActionStatus::RUNNING) {
            status_ = ActionStatus::CANCELLED;
        }
    }

    ActionStatus getStatus() const override { return status_; }
    float getProgress() const override { return progress_; }
    std::string getDescription() const override { return "Mock context modifying action"; }

private:
    std::string id_;
    ActionStatus status_;
    float progress_;
};

/**
 * @brief 예외를 발생시키는 테스트용 동작
 */
class ExceptionThrowingAction : public IAction {
public:
    explicit ExceptionThrowingAction(const std::string& id)
        : id_(id), status_(ActionStatus::PENDING), progress_(0.0f) {}

    std::string getId() const override { return id_; }
    std::string getType() const override { return "ExceptionThrowingAction"; }

    void execute(ExecutionContext& context) override {
        status_ = ActionStatus::RUNNING;
        progress_ = 0.5f;

        // 예외 발생
        throw std::runtime_error("Mock action threw exception");
    }

    void cancel() override {
        if (status_ == ActionStatus::RUNNING) {
            status_ = ActionStatus::CANCELLED;
        }
    }

    ActionStatus getStatus() const override { return status_; }
    float getProgress() const override { return progress_; }
    std::string getDescription() const override { return "Mock exception throwing action"; }

private:
    std::string id_;
    ActionStatus status_;
    float progress_;
};

/**
 * @brief 테스트용 동작 팩토리
 */
class MockActionFactory : public IActionFactory {
public:
    std::shared_ptr<IAction> createAction(
        const std::string& type,
        const std::string& id,
        const std::map<std::string, std::string>& params) override {

        if (type == "success" || type.find("success") != std::string::npos) {
            return std::make_shared<SuccessAction>(id);
        } else if (type == "failure" || type.find("failure") != std::string::npos) {
            return std::make_shared<FailureAction>(id);
        } else if (type == "modify" || type.find("modify") != std::string::npos) {
            return std::make_shared<ContextModifyingAction>(id);
        } else if (type == "exception" || type.find("exception") != std::string::npos) {
            return std::make_shared<ExceptionThrowingAction>(id);
        } else {
            // 기본값: 성공 동작
            return std::make_shared<SuccessAction>(id);
        }
    }

    std::vector<std::string> getSupportedTypes() const override {
        return {"success", "failure", "modify", "exception"};
    }
};

} // namespace mxrc::core::sequence::testing

