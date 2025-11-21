#include <gtest/gtest.h>
#include <time.h>
#include <unistd.h>
#include <sched.h>
#include <vector>
#include <algorithm>
#include <numeric>
#include <cmath>

// RT jitter 측정 통합 테스트
class JitterTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 테스트 전 설정
    }

    void TearDown() override {
        // 테스트 후 정리
    }

    // 나노초 단위 시간 측정
    uint64_t getTimeNs() {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return ts.tv_sec * 1000000000ULL + ts.tv_nsec;
    }

    // 주기적 작업 수행 및 jitter 측정
    std::vector<uint64_t> measureJitter(int iterations, uint64_t periodNs) {
        std::vector<uint64_t> jitters;
        uint64_t nextWakeup = getTimeNs() + periodNs;

        for (int i = 0; i < iterations; i++) {
            // 다음 깨어날 시간까지 대기
            struct timespec ts;
            ts.tv_sec = nextWakeup / 1000000000ULL;
            ts.tv_nsec = nextWakeup % 1000000000ULL;
            clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, nullptr);

            // 실제 깨어난 시간 측정
            uint64_t wakeupTime = getTimeNs();

            // jitter 계산 (실제 깨어난 시간 - 예상 시간)
            int64_t jitter = wakeupTime - nextWakeup;
            jitters.push_back(std::abs(jitter));

            // 다음 주기 설정
            nextWakeup += periodNs;
        }

        return jitters;
    }

    // 최대 jitter 계산
    uint64_t getMaxJitter(const std::vector<uint64_t>& jitters) {
        if (jitters.empty()) return 0;
        return *std::max_element(jitters.begin(), jitters.end());
    }

    // 평균 jitter 계산
    double getAverageJitter(const std::vector<uint64_t>& jitters) {
        if (jitters.empty()) return 0.0;
        uint64_t sum = std::accumulate(jitters.begin(), jitters.end(), 0ULL);
        return static_cast<double>(sum) / jitters.size();
    }
};

// RT 프로세스의 jitter가 50μs 이하인지 테스트 (간단한 측정)
TEST_F(JitterTest, RTProcessJitterUnder50Microseconds) {
    // FIFO 스케줄링 정책인 경우에만 테스트
    int policy = sched_getscheduler(0);
    if (policy != SCHED_FIFO) {
        GTEST_SKIP() << "Not running with SCHED_FIFO policy";
    }

    // 1ms 주기로 100회 측정
    const int iterations = 100;
    const uint64_t periodNs = 1000000;  // 1ms

    auto jitters = measureJitter(iterations, periodNs);
    uint64_t maxJitter = getMaxJitter(jitters);

    // 최대 jitter가 50μs(50,000ns) 이하인지 확인
    EXPECT_LE(maxJitter, 50000)
        << "Maximum jitter: " << maxJitter / 1000 << "μs (should be ≤ 50μs)";
}

// RT 프로세스의 평균 jitter 측정
TEST_F(JitterTest, RTProcessAverageJitter) {
    int policy = sched_getscheduler(0);
    if (policy != SCHED_FIFO) {
        GTEST_SKIP() << "Not running with SCHED_FIFO policy";
    }

    // 1ms 주기로 1000회 측정 (더 정확한 통계)
    const int iterations = 1000;
    const uint64_t periodNs = 1000000;  // 1ms

    auto jitters = measureJitter(iterations, periodNs);
    double avgJitter = getAverageJitter(jitters);

    // 평균 jitter 정보 출력
    std::cout << "Average jitter: " << avgJitter / 1000 << "μs" << std::endl;
    std::cout << "Max jitter: " << getMaxJitter(jitters) / 1000 << "μs" << std::endl;

    // 평균 jitter가 25μs 이하인지 확인 (더 엄격한 기준)
    EXPECT_LE(avgJitter, 25000)
        << "Average jitter: " << avgJitter / 1000 << "μs (should be ≤ 25μs)";
}

// 짧은 주기(250μs)에서의 jitter 테스트
TEST_F(JitterTest, ShortPeriodJitterTest) {
    int policy = sched_getscheduler(0);
    if (policy != SCHED_FIFO) {
        GTEST_SKIP() << "Not running with SCHED_FIFO policy";
    }

    // 250μs 주기로 500회 측정
    const int iterations = 500;
    const uint64_t periodNs = 250000;  // 250μs

    auto jitters = measureJitter(iterations, periodNs);
    uint64_t maxJitter = getMaxJitter(jitters);

    // 짧은 주기에서도 50μs 이하 유지
    EXPECT_LE(maxJitter, 50000)
        << "Maximum jitter at 250μs period: " << maxJitter / 1000 << "μs";
}

// jitter 분포 분석
TEST_F(JitterTest, JitterDistributionAnalysis) {
    int policy = sched_getscheduler(0);
    if (policy != SCHED_FIFO) {
        GTEST_SKIP() << "Not running with SCHED_FIFO policy";
    }

    const int iterations = 1000;
    const uint64_t periodNs = 1000000;  // 1ms

    auto jitters = measureJitter(iterations, periodNs);

    // 99 percentile 계산
    std::vector<uint64_t> sortedJitters = jitters;
    std::sort(sortedJitters.begin(), sortedJitters.end());
    size_t p99Index = (sortedJitters.size() * 99) / 100;
    uint64_t p99Jitter = sortedJitters[p99Index];

    std::cout << "99th percentile jitter: " << p99Jitter / 1000 << "μs" << std::endl;

    // 99 percentile도 50μs 이하인지 확인
    EXPECT_LE(p99Jitter, 50000)
        << "99th percentile jitter: " << p99Jitter / 1000 << "μs (should be ≤ 50μs)";
}

// CPU 부하 상태에서의 jitter 테스트
TEST_F(JitterTest, JitterUnderCPULoad) {
    int policy = sched_getscheduler(0);
    if (policy != SCHED_FIFO) {
        GTEST_SKIP() << "Not running with SCHED_FIFO policy";
    }

    // CPU 부하를 주는 작업 포함
    const int iterations = 500;
    const uint64_t periodNs = 1000000;  // 1ms

    std::vector<uint64_t> jitters;
    uint64_t nextWakeup = getTimeNs() + periodNs;

    for (int i = 0; i < iterations; i++) {
        // 약간의 CPU 부하 추가 (작은 계산)
        volatile int dummy = 0;
        for (int j = 0; j < 100; j++) {
            dummy += j;
        }

        struct timespec ts;
        ts.tv_sec = nextWakeup / 1000000000ULL;
        ts.tv_nsec = nextWakeup % 1000000000ULL;
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, nullptr);

        uint64_t wakeupTime = getTimeNs();
        int64_t jitter = wakeupTime - nextWakeup;
        jitters.push_back(std::abs(jitter));

        nextWakeup += periodNs;
    }

    uint64_t maxJitter = getMaxJitter(jitters);

    // 부하 상태에서도 50μs 이하 유지
    EXPECT_LE(maxJitter, 50000)
        << "Maximum jitter under load: " << maxJitter / 1000 << "μs";
}
