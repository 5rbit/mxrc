// MetricsCollector_test.cpp
// Copyright (C) 2025 MXRC Project

#include "core/monitoring/MetricsCollector.h"
#include <gtest/gtest.h>
#include <thread>
#include <chrono>

using namespace mxrc::core::monitoring;

class MonitoringMetricsCollectorTest : public ::testing::Test {
protected:
    MetricsCollector collector_;
};

// ============================================================================
// Counter Tests
// ============================================================================

TEST_F(MonitoringMetricsCollectorTest, CounterBasicOperations) {
    auto counter = collector_.getOrCreateCounter("test_counter");

    EXPECT_EQ(0, counter->get());

    counter->increment();
    EXPECT_EQ(1, counter->get());

    counter->increment(5);
    EXPECT_EQ(6, counter->get());

    counter->reset();
    EXPECT_EQ(0, counter->get());
}

TEST_F(MonitoringMetricsCollectorTest, CounterWithLabels) {
    auto counter1 = collector_.getOrCreateCounter("test_counter", {{"method", "GET"}});
    auto counter2 = collector_.getOrCreateCounter("test_counter", {{"method", "POST"}});

    counter1->increment(10);
    counter2->increment(20);

    EXPECT_EQ(10, counter1->get());
    EXPECT_EQ(20, counter2->get());
}

TEST_F(MonitoringMetricsCollectorTest, CounterThreadSafety) {
    auto counter = collector_.getOrCreateCounter("test_counter");

    const int num_threads = 10;
    const int increments_per_thread = 1000;

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&counter, increments_per_thread]() {
            for (int j = 0; j < increments_per_thread; ++j) {
                counter->increment();
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(num_threads * increments_per_thread, counter->get());
}

TEST_F(MonitoringMetricsCollectorTest, CounterConvenienceMethod) {
    collector_.incrementCounter("test_counter", {{"status", "success"}}, 5);

    auto counter = collector_.getOrCreateCounter("test_counter", {{"status", "success"}});
    EXPECT_EQ(5, counter->get());
}

// ============================================================================
// Gauge Tests
// ============================================================================

TEST_F(MonitoringMetricsCollectorTest, GaugeBasicOperations) {
    auto gauge = collector_.getOrCreateGauge("test_gauge");

    EXPECT_DOUBLE_EQ(0.0, gauge->get());

    gauge->set(42.5);
    EXPECT_DOUBLE_EQ(42.5, gauge->get());

    gauge->increment(10.5);
    EXPECT_DOUBLE_EQ(53.0, gauge->get());

    gauge->decrement(3.0);
    EXPECT_DOUBLE_EQ(50.0, gauge->get());
}

TEST_F(MonitoringMetricsCollectorTest, GaugeWithLabels) {
    auto gauge1 = collector_.getOrCreateGauge("test_gauge", {{"cpu", "0"}});
    auto gauge2 = collector_.getOrCreateGauge("test_gauge", {{"cpu", "1"}});

    gauge1->set(45.5);
    gauge2->set(67.8);

    EXPECT_DOUBLE_EQ(45.5, gauge1->get());
    EXPECT_DOUBLE_EQ(67.8, gauge2->get());
}

TEST_F(MonitoringMetricsCollectorTest, GaugeNegativeValues) {
    auto gauge = collector_.getOrCreateGauge("test_gauge");

    gauge->set(-10.5);
    EXPECT_DOUBLE_EQ(-10.5, gauge->get());

    gauge->increment(15.0);
    EXPECT_DOUBLE_EQ(4.5, gauge->get());

    gauge->decrement(10.0);
    EXPECT_DOUBLE_EQ(-5.5, gauge->get());
}

TEST_F(MonitoringMetricsCollectorTest, GaugeConvenienceMethod) {
    collector_.setGauge("test_gauge", 123.45, {{"instance", "1"}});

    auto gauge = collector_.getOrCreateGauge("test_gauge", {{"instance", "1"}});
    EXPECT_DOUBLE_EQ(123.45, gauge->get());
}

// ============================================================================
// Histogram Tests
// ============================================================================

TEST_F(MonitoringMetricsCollectorTest, HistogramBasicOperations) {
    std::vector<double> buckets = {0.1, 0.5, 1.0, 5.0};
    auto histogram = collector_.getOrCreateHistogram("test_histogram", {}, buckets);

    EXPECT_EQ(0, histogram->count());
    EXPECT_DOUBLE_EQ(0.0, histogram->sum());

    histogram->observe(0.05);  // bucket 0
    histogram->observe(0.3);   // bucket 1
    histogram->observe(0.8);   // bucket 2
    histogram->observe(2.0);   // bucket 3
    histogram->observe(10.0);  // bucket 4 (+Inf)

    EXPECT_EQ(5, histogram->count());
    EXPECT_DOUBLE_EQ(13.15, histogram->sum());
}

TEST_F(MonitoringMetricsCollectorTest, HistogramBucketCounts) {
    std::vector<double> buckets = {1.0, 5.0, 10.0};
    auto histogram = collector_.getOrCreateHistogram("test_histogram", {}, buckets);

    histogram->observe(0.5);   // bucket 0
    histogram->observe(2.0);   // bucket 1
    histogram->observe(7.0);   // bucket 2
    histogram->observe(15.0);  // bucket 3 (+Inf)

    auto counts = histogram->bucketCounts();
    ASSERT_EQ(4, counts.size());
    EXPECT_EQ(1, counts[0]);  // <= 1.0
    EXPECT_EQ(1, counts[1]);  // 1.0 < x <= 5.0
    EXPECT_EQ(1, counts[2]);  // 5.0 < x <= 10.0
    EXPECT_EQ(1, counts[3]);  // > 10.0
}

TEST_F(MonitoringMetricsCollectorTest, HistogramMultipleObservations) {
    auto histogram = collector_.getOrCreateHistogram("test_histogram");

    for (int i = 0; i < 100; ++i) {
        histogram->observe(0.001 * i);
    }

    EXPECT_EQ(100, histogram->count());
    EXPECT_NEAR(4.95, histogram->sum(), 0.01);
}

TEST_F(MonitoringMetricsCollectorTest, HistogramConvenienceMethod) {
    collector_.observeHistogram("test_histogram", 1.5, {{"operation", "query"}});

    auto histogram = collector_.getOrCreateHistogram("test_histogram", {{"operation", "query"}});
    EXPECT_EQ(1, histogram->count());
    EXPECT_DOUBLE_EQ(1.5, histogram->sum());
}

// ============================================================================
// Prometheus Export Tests
// ============================================================================

TEST_F(MonitoringMetricsCollectorTest, PrometheusExportCounter) {
    collector_.incrementCounter("http_requests_total", {{"method", "GET"}}, 10);
    collector_.incrementCounter("http_requests_total", {{"method", "POST"}}, 5);

    std::string output = collector_.exportPrometheus();

    EXPECT_NE(std::string::npos, output.find("# TYPE http_requests_total counter"));
    EXPECT_NE(std::string::npos, output.find("http_requests_total{method=\"GET\"} 10"));
    EXPECT_NE(std::string::npos, output.find("http_requests_total{method=\"POST\"} 5"));
}

TEST_F(MonitoringMetricsCollectorTest, PrometheusExportGauge) {
    collector_.setGauge("cpu_usage_percent", 45.5, {{"core", "0"}});
    collector_.setGauge("cpu_usage_percent", 67.8, {{"core", "1"}});

    std::string output = collector_.exportPrometheus();

    EXPECT_NE(std::string::npos, output.find("# TYPE cpu_usage_percent gauge"));
    EXPECT_NE(std::string::npos, output.find("cpu_usage_percent{core=\"0\"} 45.5"));
    EXPECT_NE(std::string::npos, output.find("cpu_usage_percent{core=\"1\"} 67.8"));
}

TEST_F(MonitoringMetricsCollectorTest, PrometheusExportHistogram) {
    std::vector<double> buckets = {0.1, 0.5, 1.0};
    auto histogram = collector_.getOrCreateHistogram("request_duration_seconds", {}, buckets);

    histogram->observe(0.05);
    histogram->observe(0.3);
    histogram->observe(0.8);
    histogram->observe(2.0);

    std::string output = collector_.exportPrometheus();

    EXPECT_NE(std::string::npos, output.find("# TYPE request_duration_seconds histogram"));
    EXPECT_NE(std::string::npos, output.find("request_duration_seconds_bucket{le=\"0.1\"}"));
    EXPECT_NE(std::string::npos, output.find("request_duration_seconds_bucket{le=\"0.5\"}"));
    EXPECT_NE(std::string::npos, output.find("request_duration_seconds_bucket{le=\"1\"}"));
    EXPECT_NE(std::string::npos, output.find("request_duration_seconds_bucket{le=\"+Inf\"}"));
    EXPECT_NE(std::string::npos, output.find("request_duration_seconds_sum"));
    EXPECT_NE(std::string::npos, output.find("request_duration_seconds_count"));
}

TEST_F(MonitoringMetricsCollectorTest, PrometheusExportWithHelp) {
    collector_.getOrCreateCounter("test_counter", {}, "Test counter help text");
    collector_.incrementCounter("test_counter");

    std::string output = collector_.exportPrometheus();

    EXPECT_NE(std::string::npos, output.find("# HELP test_counter Test counter help text"));
    EXPECT_NE(std::string::npos, output.find("# TYPE test_counter counter"));
}

TEST_F(MonitoringMetricsCollectorTest, PrometheusExportEmpty) {
    std::string output = collector_.exportPrometheus();
    EXPECT_TRUE(output.empty());
}

// ============================================================================
// Reset Tests
// ============================================================================

TEST_F(MonitoringMetricsCollectorTest, ResetCounters) {
    auto counter1 = collector_.getOrCreateCounter("counter1");
    auto counter2 = collector_.getOrCreateCounter("counter2");

    counter1->increment(10);
    counter2->increment(20);

    collector_.reset();

    EXPECT_EQ(0, counter1->get());
    EXPECT_EQ(0, counter2->get());
}

TEST_F(MonitoringMetricsCollectorTest, ResetClearsGaugesAndHistograms) {
    auto gauge = collector_.getOrCreateGauge("test_gauge");
    auto histogram = collector_.getOrCreateHistogram("test_histogram");

    gauge->set(42.0);
    histogram->observe(1.5);

    collector_.reset();

    // After reset, gauges and histograms are cleared
    std::string output = collector_.exportPrometheus();
    EXPECT_EQ(std::string::npos, output.find("test_gauge"));
    EXPECT_EQ(std::string::npos, output.find("test_histogram"));
}

// ============================================================================
// ScopedTimer Tests
// ============================================================================

TEST_F(MonitoringMetricsCollectorTest, ScopedTimerAutoRecords) {
    auto histogram = collector_.getOrCreateHistogram("operation_duration");

    {
        ScopedTimer timer(histogram);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    EXPECT_EQ(1, histogram->count());
    EXPECT_GT(histogram->sum(), 0.008);  // At least 8ms (allowing some variance)
    EXPECT_LT(histogram->sum(), 0.020);  // Less than 20ms
}

TEST_F(MonitoringMetricsCollectorTest, ScopedTimerMultipleCalls) {
    auto histogram = collector_.getOrCreateHistogram("operation_duration");

    for (int i = 0; i < 5; ++i) {
        ScopedTimer timer(histogram);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    EXPECT_EQ(5, histogram->count());
    EXPECT_GT(histogram->sum(), 0.020);  // At least 20ms total
}

// ============================================================================
// Edge Cases and Error Handling
// ============================================================================

TEST_F(MonitoringMetricsCollectorTest, EmptyLabels) {
    auto counter1 = collector_.getOrCreateCounter("test", {});
    auto counter2 = collector_.getOrCreateCounter("test");

    // Should return the same counter
    EXPECT_EQ(counter1, counter2);
}

TEST_F(MonitoringMetricsCollectorTest, MultipleLabels) {
    Labels labels = {
        {"method", "GET"},
        {"status", "200"},
        {"path", "/api/users"}
    };

    collector_.incrementCounter("http_requests", labels, 1);

    std::string output = collector_.exportPrometheus();
    EXPECT_NE(std::string::npos, output.find("method=\"GET\""));
    EXPECT_NE(std::string::npos, output.find("status=\"200\""));
    EXPECT_NE(std::string::npos, output.find("path=\"/api/users\""));
}

TEST_F(MonitoringMetricsCollectorTest, HistogramUnsortedBuckets) {
    std::vector<double> buckets = {5.0, 1.0, 10.0, 0.1};  // Unsorted
    auto histogram = collector_.getOrCreateHistogram("test", {}, buckets);

    histogram->observe(0.5);
    histogram->observe(3.0);
    histogram->observe(7.0);

    // Histogram should handle unsorted buckets internally
    EXPECT_EQ(3, histogram->count());
}

TEST_F(MonitoringMetricsCollectorTest, LargeNumberOfMetrics) {
    // Test with many metrics to ensure scalability
    for (int i = 0; i < 100; ++i) {
        std::string name = "metric_" + std::to_string(i);
        collector_.incrementCounter(name, {{"id", std::to_string(i)}}, i);
    }

    std::string output = collector_.exportPrometheus();

    // Verify some metrics are present
    EXPECT_NE(std::string::npos, output.find("metric_0"));
    EXPECT_NE(std::string::npos, output.find("metric_50"));
    EXPECT_NE(std::string::npos, output.find("metric_99"));
}
