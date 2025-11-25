#!/bin/bash

TEST_LIST_FILE="test_list.txt"

# Run gtest_list_tests and save to a temporary file
build/run_tests --gtest_list_tests > "$TEST_LIST_FILE"

# Initialize associative arrays for counts
declare -A UNIT_MODULE_COUNTS
declare -A INTEGRATION_MODULE_COUNTS
declare -A BENCHMARK_MODULE_COUNTS # New for benchmarks

# Map test suite prefixes to modules and implicitly to type (unit/integration/benchmark)
declare -A SUITE_TO_MODULE_TYPE_MAP

# Unit Tests (default type if no suffix)
SUITE_TO_MODULE_TYPE_MAP["TaskRegistryTest"]="Task_Unit" # 12
SUITE_TO_MODULE_TYPE_MAP["TaskExecutorOnceTest"]="Task_Unit" # 25
SUITE_TO_MODULE_TYPE_MAP["PeriodicSchedulerTest"]="Task_Unit" # 9
SUITE_TO_MODULE_TYPE_MAP["TriggerManagerTest"]="Task_Unit" # 12
SUITE_TO_MODULE_TYPE_MAP["TaskMonitorTest"]="Task_Unit" # 14

SUITE_TO_MODULE_TYPE_MAP["ActionExecutorTest"]="Action_Unit" # 12
SUITE_TO_MODULE_TYPE_MAP["ActionFactoryTest"]="Action_Unit" # 5
SUITE_TO_MODULE_TYPE_MAP["ActionRegistryTest"]="Action_Unit" # 6

SUITE_TO_MODULE_TYPE_MAP["SequenceRegistryTest"]="Sequence_Unit" # 7
SUITE_TO_MODULE_TYPE_MAP["SequenceEngineTest"]="Sequence_Unit" # 14
SUITE_TO_MODULE_TYPE_MAP["ConditionEvaluatorTest"]="Sequence_Unit" # 5
SUITE_TO_MODULE_TYPE_MAP["RetryHandlerTest"]="Sequence_Unit" # 7

SUITE_TO_MODULE_TYPE_MAP["LockFreeQueueTest"]="Event_Unit" # 6
SUITE_TO_MODULE_TYPE_MAP["MPSCLockFreeQueueTest"]="Event_Unit" # 3
SUITE_TO_MODULE_TYPE_MAP["SubscriptionManagerTest"]="Event_Unit" # 9
SUITE_TO_MODULE_TYPE_MAP["EventBusTest"]="Event_Unit" # 17
SUITE_TO_MODULE_TYPE_MAP["DataStoreEventAdapterTest"]="Event_Unit" # 26
SUITE_TO_MODULE_TYPE_MAP["PrioritizedEventTest"]="Event_Unit" # 15
SUITE_TO_MODULE_TYPE_MAP["PriorityQueueTest"]="Event_Unit" # 20
SUITE_TO_MODULE_TYPE_MAP["ThrottlingPolicyTest"]="Event_Unit" # 19
SUITE_TO_MODULE_TYPE_MAP["CoalescingPolicyTest"]="Event_Unit" # 20

SUITE_TO_MODULE_TYPE_MAP["DataStoreTest"]="DataStore_Unit" # 34
SUITE_TO_MODULE_TYPE_MAP["TBBIntegrationTest"]="DataStore_Unit" # 1
SUITE_TO_MODULE_TYPE_MAP["ExpirationManagerTest"]="DataStore_Unit" # 21
SUITE_TO_MODULE_TYPE_MAP["MetricsCollectorTest"]="DataStore_Unit" # 11 (this is MetricsCollector in core/datastore/managers)
SUITE_TO_MODULE_TYPE_MAP["AccessControlManagerTest"]="DataStore_Unit" # 14
SUITE_TO_MODULE_TYPE_MAP["LogManagerTest"]="DataStore_Unit" # 19
SUITE_TO_MODULE_TYPE_MAP["VersionedDataTest"]="DataStore_Unit" # 23

SUITE_TO_MODULE_TYPE_MAP["BagMessageTest"]="Logging_Unit" # 7
SUITE_TO_MODULE_TYPE_MAP["SerializerTest"]="Logging_Unit" # 15
SUITE_TO_MODULE_TYPE_MAP["FileUtilsTest"]="Logging_Unit" # 13
SUITE_TO_MODULE_TYPE_MAP["AsyncWriterTest"]="Logging_Unit" # 8
SUITE_TO_MODULE_TYPE_MAP["RetentionManagerTest"]="Logging_Unit" # 8
SUITE_TO_MODULE_TYPE_MAP["SimpleBagWriterTest"]="Logging_Unit" # 8
SUITE_TO_MODULE_TYPE_MAP["DataStoreBagLoggerTest"]="Logging_Unit" # 8
SUITE_TO_MODULE_TYPE_MAP["IndexerTest"]="Logging_Unit" # 9
SUITE_TO_MODULE_TYPE_MAP["BagReaderTest"]="Logging_Unit" # 11
SUITE_TO_MODULE_TYPE_MAP["BagReplayerTest"]="Logging_Unit" # 11
SUITE_TO_MODULE_TYPE_MAP["AsyncLoggerTest"]="Logging_Unit" # 6
SUITE_TO_MODULE_TYPE_MAP["LogPerformanceTest"]="Logging_Unit" # 4
SUITE_TO_MODULE_TYPE_MAP["SignalHandlerTest"]="Logging_Unit" # 4

SUITE_TO_MODULE_TYPE_MAP["RTDataStoreTest"]="RT_Unit" # 16
SUITE_TO_MODULE_TYPE_MAP["RTDataStoreConcurrencyTest"]="RT_Unit" # 1
SUITE_TO_MODULE_TYPE_MAP["SharedMemoryTest"]="RT_Unit" # 7
SUITE_TO_MODULE_TYPE_MAP["RTExecutiveTest"]="RT_Unit" # 20
SUITE_TO_MODULE_TYPE_MAP["RTStateMachineTest"]="RT_Unit" # 16

SUITE_TO_MODULE_TYPE_MAP["MonitoringMetricsCollectorTest"]="Monitoring_Unit" # 24 (this is Monitoring's MetricsCollectorTest)
SUITE_TO_MODULE_TYPE_MAP["MetricsServerTest"]="Monitoring_Unit" # 18
SUITE_TO_MODULE_TYPE_MAP["RTMetricsTest"]="Monitoring_Unit" # 21
SUITE_TO_MODULE_TYPE_MAP["StructuredLoggerTest"]="Monitoring_Unit" # 15

SUITE_TO_MODULE_TYPE_MAP["CPUAffinityManagerTest"]="Perf_Unit" # 10
SUITE_TO_MODULE_TYPE_MAP["NUMABindingTest"]="Perf_Unit" # 8
SUITE_TO_MODULE_TYPE_MAP["PerfMonitorTest"]="Perf_Unit" # 14

SUITE_TO_MODULE_TYPE_MAP["ProcessMonitorTest"]="HA_Unit" # 11
SUITE_TO_MODULE_TYPE_MAP["FailoverManagerTest"]="HA_Unit" # 16
SUITE_TO_MODULE_TYPE_MAP["StateCheckpointTest"]="HA_Unit" # 15

SUITE_TO_MODULE_TYPE_MAP["TracerProviderTest"]="Tracing_Unit" # 14
SUITE_TO_MODULE_TYPE_MAP["SpanContextTest"]="Tracing_Unit" # 21
SUITE_TO_MODULE_TYPE_MAP["EventBusTracerTest"]="Tracing_Unit" # 9
SUITE_TO_MODULE_TYPE_MAP["RTCycleTracerTest"]="Tracing_Unit" # 15

SUITE_TO_MODULE_TYPE_MAP["SystemdUtilTest"]="Systemd_Unit" # 11
SUITE_TO_MODULE_TYPE_MAP["WatchdogNotifierTest"]="Systemd_Unit" # 9
SUITE_TO_MODULE_TYPE_MAP["WatchdogTimerTest"]="Systemd_Unit" # 10

SUITE_TO_MODULE_TYPE_MAP["YAMLConfigParserTest"]="EtherCAT_Unit" # 7
SUITE_TO_MODULE_TYPE_MAP["SensorDataManagerTest"]="EtherCAT_Unit" # 19
SUITE_TO_MODULE_TYPE_MAP["MotorCommandManagerTest"]="EtherCAT_Unit" # 10
SUITE_TO_MODULE_TYPE_MAP["RTEtherCATCycleTest"]="EtherCAT_Unit" # 14


# Integration Tests
SUITE_TO_MODULE_TYPE_MAP["ActionIntegrationTest"]="Action_Integration" # 4
SUITE_TO_MODULE_TYPE_MAP["SequenceIntegrationTest"]="Sequence_Integration" # 1
SUITE_TO_MODULE_TYPE_MAP["SequenceTaskManagerIntegration"]="Task_Integration" # 1
SUITE_TO_MODULE_TYPE_MAP["DataStoreLoggingIntegration"]="DataStore_Integration" # 3
SUITE_TO_MODULE_TYPE_MAP["EventFlowTest"]="Event_Integration" # 5
SUITE_TO_MODULE_TYPE_MAP["PriorityIntegrationTest"]="Event_Integration" # 8
SUITE_TO_MODULE_TYPE_MAP["EventStormTest"]="Event_Integration" # 3
SUITE_TO_MODULE_TYPE_MAP["MonitoringExtensionTest"]="Event_Integration" # 6
SUITE_TO_MODULE_TYPE_MAP["RTIntegrationTest"]="RT_Integration" # 3
SUITE_TO_MODULE_TYPE_MAP["RTExecutiveEventBusTest"]="RT_Integration" # 5
SUITE_TO_MODULE_TYPE_MAP["MonitoringIntegrationTest"]="Monitoring_Integration" # 8
SUITE_TO_MODULE_TYPE_MAP["CPUIsolationTest"]="Perf_Integration" # 3
SUITE_TO_MODULE_TYPE_MAP["NUMAOptimizationTest"]="Perf_Integration" # 4
SUITE_TO_MODULE_TYPE_MAP["StartupOrderTest"]="Systemd_Integration" # 5
SUITE_TO_MODULE_TYPE_MAP["RetryLogicTest"]="Systemd_Integration" # 6
SUITE_TO_MODULE_TYPE_MAP["CpuQuotaTest"]="Systemd_Integration" # 4
SUITE_TO_MODULE_TYPE_MAP["MemoryLimitTest"]="Systemd_Integration" # 7
SUITE_TO_MODULE_TYPE_MAP["IoWeightTest"]="Systemd_Integration" # 8
SUITE_TO_MODULE_TYPE_MAP["ServiceOrderTest"]="Systemd_Integration" # 5
SUITE_TO_MODULE_TYPE_MAP["DependencyChainTest"]="Systemd_Integration" # 7
SUITE_TO_MODULE_TYPE_MAP["TargetTest"]="Systemd_Integration" # 8
SUITE_TO_MODULE_TYPE_MAP["MetricsCollectionTest"]="Systemd_Integration" # 7
SUITE_TO_MODULE_TYPE_MAP["PrometheusFormatTest"]="Systemd_Integration" # 9
SUITE_TO_MODULE_TYPE_MAP["MetricsEndpointTest"]="Systemd_Integration" # 8
SUITE_TO_MODULE_TYPE_MAP["JournaldLoggingTest"]="Systemd_Integration" # 9
SUITE_TO_MODULE_TYPE_MAP["StructuredLoggingTest"]="Systemd_Integration" # 7
SUITE_TO_MODULE_TYPE_MAP["LogQueryTest"]="Systemd_Integration" # 7
SUITE_TO_MODULE_TYPE_MAP["SecurityHardeningTest"]="Systemd_Integration" # 8
SUITE_TO_MODULE_TYPE_MAP["CapabilityTest"]="Systemd_Integration" # 5
SUITE_TO_MODULE_TYPE_MAP["FilesystemRestrictionTest"]="Systemd_Integration" # 6
SUITE_TO_MODULE_TYPE_MAP["BootTimeTest"]="Systemd_Integration" # 4
SUITE_TO_MODULE_TYPE_MAP["TracingIntegrationTest"]="Tracing_Integration" # 9
SUITE_TO_MODULE_TYPE_MAP["RTEtherCATCycleIntegrationTest"]="EtherCAT_Integration" # 14 (from tests/integration/ethercat/RTEtherCATCycle_test.cpp)
SUITE_TO_MODULE_TYPE_MAP["BagLoggingIntegrationTest"]="Logging_Integration" # 7
SUITE_TO_MODULE_TYPE_MAP["CrashSafetyTest"]="Logging_Integration" # 3


# Benchmarks
SUITE_TO_MODULE_TYPE_MAP["AccessorBenchmark"]="DataStore_Benchmark" # 13
SUITE_TO_MODULE_TYPE_MAP["DataStoreLoggingBenchmark"]="Logging_Benchmark" # 4
SUITE_TO_MODULE_TYPE_MAP["BagReplayBenchmark"]="Logging_Benchmark" # 4


# Initialize all counts to 0
for suite_name in "${!SUITE_TO_MODULE_TYPE_MAP[@]}"; do
    module_type="${SUITE_TO_MODULE_TYPE_MAP[$suite_name]}"
    module_name="${module_type%_*}" # Extract module name before underscore
    test_type="${module_type#*_}"  # Extract type after underscore

    UNIT_MODULE_COUNTS["$module_name"]=0
    INTEGRATION_MODULE_COUNTS["$module_name"]=0
    BENCHMARK_MODULE_COUNTS["$module_name"]=0
done


CURRENT_SUITE=""
while IFS= read -r line; do
    if [[ "$line" =~ ^([A-Za-z0-9_]+)\.$ ]]; then # Match TestSuiteName.
        CURRENT_SUITE="${BASH_REMATCH[1]}"
    elif [[ "$line" =~ ^[[:space:]]+([A-Za-z0-9_]+)$ ]]; then # Match indented TestCaseName
        if [[ -n "$CURRENT_SUITE" ]]; then
            MODULE_TYPE="${SUITE_TO_MODULE_TYPE_MAP[$CURRENT_SUITE]}"
            if [[ -n "$MODULE_TYPE" ]]; then # Only count if it's a recognized module
                module_name="${MODULE_TYPE%_*}"
                test_type="${MODULE_TYPE#*_}"
                
                if [[ "$test_type" == "Unit" ]]; then
                    UNIT_MODULE_COUNTS["$module_name"]=$((UNIT_MODULE_COUNTS["$module_name"] + 1))
                elif [[ "$test_type" == "Integration" ]]; then
                    INTEGRATION_MODULE_COUNTS["$module_name"]=$((INTEGRATION_MODULE_COUNTS["$module_name"] + 1))
                elif [[ "$test_type" == "Benchmark" ]]; then
                    BENCHMARK_MODULE_COUNTS["$module_name"]=$((BENCHMARK_MODULE_COUNTS["$module_name"] + 1))
                fi
            fi
        fi
    fi
done < "$TEST_LIST_FILE"


echo "Test Counts by Module:"
echo "----------------------------------------------------------------"
printf "% -15s % -10s % -10s % -10s % -10s\n" "Module" "Unit" "Integration" "Benchmark" "Total"
echo "----------------------------------------------------------------"

TOTAL_UNIT=0
TOTAL_INTEGRATION=0
TOTAL_BENCHMARK=0
declare -A ALL_MODULES

# Collect all unique module names
for suite_name in "${!SUITE_TO_MODULE_TYPE_MAP[@]}"; do
    module_type="${SUITE_TO_MODULE_TYPE_MAP[$suite_name]}"
    module_name="${module_type%_*}"
    ALL_MODULES["$module_name"]=1
done


for module in "${!ALL_MODULES[@]}"; do
    UNIT=${UNIT_MODULE_COUNTS["$module"]:-0}
    INTEGRATION=${INTEGRATION_MODULE_COUNTS["$module"]:-0}
    BENCHMARK=${BENCHMARK_MODULE_COUNTS["$module"]:-0}
    TOTAL=$((UNIT + INTEGRATION + BENCHMARK))
    
    TOTAL_UNIT=$((TOTAL_UNIT + UNIT))
    TOTAL_INTEGRATION=$((TOTAL_INTEGRATION + INTEGRATION))
    TOTAL_BENCHMARK=$((TOTAL_BENCHMARK + BENCHMARK))
    
    printf "% -15s % -10s % -10s % -10s % -10s\n" "$module" "$UNIT" "$INTEGRATION" "$BENCHMARK" "$TOTAL"
done

echo "----------------------------------------------------------------"
printf "% -15s % -10s % -10s % -10s % -10s\n" "Grand Total" "$TOTAL_UNIT" "$TOTAL_INTEGRATION" "$TOTAL_BENCHMARK" "$((TOTAL_UNIT + TOTAL_INTEGRATION + TOTAL_BENCHMARK))"
echo "----------------------------------------------------------------"

# Clean up temporary file
rm "$TEST_LIST_FILE"
