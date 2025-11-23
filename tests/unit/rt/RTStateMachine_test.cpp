#include <gtest/gtest.h>
#include "core/rt/RTStateMachine.h"

using namespace mxrc::core::rt;

class RTStateMachineTest : public ::testing::Test {
protected:
    RTStateMachine sm_;
};

// 초기 상태
TEST_F(RTStateMachineTest, InitialState) {
    EXPECT_EQ(RTState::INIT, sm_.getState());
}

// INIT -> READY 전환
TEST_F(RTStateMachineTest, InitToReady) {
    EXPECT_EQ(0, sm_.handleEvent(RTEvent::START));
    EXPECT_EQ(RTState::READY, sm_.getState());
}

// READY -> RUNNING 전환
TEST_F(RTStateMachineTest, ReadyToRunning) {
    sm_.handleEvent(RTEvent::START);  // INIT -> READY
    EXPECT_EQ(0, sm_.handleEvent(RTEvent::START));  // READY -> RUNNING
    EXPECT_EQ(RTState::RUNNING, sm_.getState());
}

// RUNNING -> PAUSED 전환
TEST_F(RTStateMachineTest, RunningToPaused) {
    sm_.handleEvent(RTEvent::START);  // INIT -> READY
    sm_.handleEvent(RTEvent::START);  // READY -> RUNNING
    EXPECT_EQ(0, sm_.handleEvent(RTEvent::PAUSE));
    EXPECT_EQ(RTState::PAUSED, sm_.getState());
}

// PAUSED -> RUNNING 재개
TEST_F(RTStateMachineTest, PausedToRunning) {
    sm_.handleEvent(RTEvent::START);  // INIT -> READY
    sm_.handleEvent(RTEvent::START);  // READY -> RUNNING
    sm_.handleEvent(RTEvent::PAUSE);  // RUNNING -> PAUSED
    EXPECT_EQ(0, sm_.handleEvent(RTEvent::RESUME));
    EXPECT_EQ(RTState::RUNNING, sm_.getState());
}

// RUNNING -> SHUTDOWN 전환
TEST_F(RTStateMachineTest, RunningToShutdown) {
    sm_.handleEvent(RTEvent::START);  // INIT -> READY
    sm_.handleEvent(RTEvent::START);  // READY -> RUNNING
    EXPECT_EQ(0, sm_.handleEvent(RTEvent::STOP));
    EXPECT_EQ(RTState::SHUTDOWN, sm_.getState());
}

// ERROR 상태로 전환
TEST_F(RTStateMachineTest, ToErrorState) {
    sm_.handleEvent(RTEvent::START);  // INIT -> READY
    sm_.handleEvent(RTEvent::START);  // READY -> RUNNING
    EXPECT_EQ(0, sm_.handleEvent(RTEvent::ERROR_OCCUR));
    EXPECT_EQ(RTState::ERROR, sm_.getState());
}

// ERROR -> INIT 리셋
TEST_F(RTStateMachineTest, ErrorToInitReset) {
    sm_.handleEvent(RTEvent::START);
    sm_.handleEvent(RTEvent::ERROR_OCCUR);
    EXPECT_EQ(RTState::ERROR, sm_.getState());

    EXPECT_EQ(0, sm_.handleEvent(RTEvent::RESET));
    EXPECT_EQ(RTState::INIT, sm_.getState());
}

// 잘못된 전환 - INIT에서 PAUSE
TEST_F(RTStateMachineTest, InvalidTransitionInitPause) {
    EXPECT_EQ(-1, sm_.handleEvent(RTEvent::PAUSE));
    EXPECT_EQ(RTState::INIT, sm_.getState());
}

// 잘못된 전환 - READY에서 RESUME
TEST_F(RTStateMachineTest, InvalidTransitionReadyResume) {
    sm_.handleEvent(RTEvent::START);  // INIT -> READY
    EXPECT_EQ(-1, sm_.handleEvent(RTEvent::RESUME));
    EXPECT_EQ(RTState::READY, sm_.getState());
}

// SHUTDOWN에서 전환 불가
TEST_F(RTStateMachineTest, ShutdownNoTransition) {
    sm_.handleEvent(RTEvent::START);  // INIT -> READY
    sm_.handleEvent(RTEvent::START);  // READY -> RUNNING
    sm_.handleEvent(RTEvent::STOP);   // RUNNING -> SHUTDOWN

    EXPECT_EQ(-1, sm_.handleEvent(RTEvent::START));
    EXPECT_EQ(-1, sm_.handleEvent(RTEvent::RESET));
    EXPECT_EQ(RTState::SHUTDOWN, sm_.getState());
}

// 상태 전환 콜백
TEST_F(RTStateMachineTest, TransitionCallback) {
    int callback_count = 0;
    RTState last_from = RTState::INIT;
    RTState last_to = RTState::INIT;
    RTEvent last_event = RTEvent::START;

    sm_.setTransitionCallback([&](RTState from, RTState to, RTEvent event) {
        callback_count++;
        last_from = from;
        last_to = to;
        last_event = event;
    });

    sm_.handleEvent(RTEvent::START);  // INIT -> READY

    EXPECT_EQ(1, callback_count);
    EXPECT_EQ(RTState::INIT, last_from);
    EXPECT_EQ(RTState::READY, last_to);
    EXPECT_EQ(RTEvent::START, last_event);
}

// 복잡한 상태 전환 시나리오
TEST_F(RTStateMachineTest, ComplexScenario) {
    // INIT -> READY -> RUNNING
    EXPECT_EQ(0, sm_.handleEvent(RTEvent::START));
    EXPECT_EQ(0, sm_.handleEvent(RTEvent::START));
    EXPECT_EQ(RTState::RUNNING, sm_.getState());

    // RUNNING -> PAUSED
    EXPECT_EQ(0, sm_.handleEvent(RTEvent::PAUSE));
    EXPECT_EQ(RTState::PAUSED, sm_.getState());

    // PAUSED -> RUNNING
    EXPECT_EQ(0, sm_.handleEvent(RTEvent::RESUME));
    EXPECT_EQ(RTState::RUNNING, sm_.getState());

    // RUNNING -> SHUTDOWN
    EXPECT_EQ(0, sm_.handleEvent(RTEvent::STOP));
    EXPECT_EQ(RTState::SHUTDOWN, sm_.getState());
}

// 문자열 변환
TEST_F(RTStateMachineTest, StateToString) {
    EXPECT_EQ("INIT", RTStateMachine::stateToString(RTState::INIT));
    EXPECT_EQ("READY", RTStateMachine::stateToString(RTState::READY));
    EXPECT_EQ("RUNNING", RTStateMachine::stateToString(RTState::RUNNING));
    EXPECT_EQ("PAUSED", RTStateMachine::stateToString(RTState::PAUSED));
    EXPECT_EQ("ERROR", RTStateMachine::stateToString(RTState::ERROR));
    EXPECT_EQ("SHUTDOWN", RTStateMachine::stateToString(RTState::SHUTDOWN));
}

TEST_F(RTStateMachineTest, EventToString) {
    EXPECT_EQ("START", RTStateMachine::eventToString(RTEvent::START));
    EXPECT_EQ("PAUSE", RTStateMachine::eventToString(RTEvent::PAUSE));
    EXPECT_EQ("RESUME", RTStateMachine::eventToString(RTEvent::RESUME));
    EXPECT_EQ("STOP", RTStateMachine::eventToString(RTEvent::STOP));
    EXPECT_EQ("ERROR_OCCUR", RTStateMachine::eventToString(RTEvent::ERROR_OCCUR));
    EXPECT_EQ("RESET", RTStateMachine::eventToString(RTEvent::RESET));
}
