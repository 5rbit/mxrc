#!/bin/bash
# MXRC WebAPI RT Impact Testing Script
#
# This script performs comprehensive testing to measure WebAPI's impact on RT performance

set -e

# 색상 정의
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() { echo -e "${GREEN}[INFO]${NC} $1"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }
log_section() { echo -e "\n${BLUE}=== $1 ===${NC}\n"; }

# Root 권한 확인
if [[ $EUID -ne 0 ]]; then
   log_error "This script must be run as root (use sudo)"
   exit 1
fi

# 변수 설정
RESULTS_DIR="/tmp/mxrc-rt-impact-$(date +%Y%m%d_%H%M%S)"
mkdir -p "$RESULTS_DIR"

log_info "Results will be saved to: $RESULTS_DIR"

# 1. Prerequisites Check
log_section "Checking Prerequisites"

check_command() {
    if command -v $1 &> /dev/null; then
        log_info "$1: Found"
        return 0
    else
        log_warn "$1: Not found"
        return 1
    fi
}

MISSING=0
check_command cyclictest || MISSING=$((MISSING+1))
check_command mpstat || MISSING=$((MISSING+1))
check_command perf || MISSING=$((MISSING+1))
check_command ab || MISSING=$((MISSING+1))

if [ $MISSING -gt 0 ]; then
    log_warn "Missing $MISSING required tools. Install with:"
    log_info "  sudo apt-get install rt-tests sysstat linux-tools-generic apache2-utils"
    read -p "Continue anyway? [y/N] " -n 1 -r
    echo
    [[ ! $REPLY =~ ^[Yy]$ ]] && exit 1
fi

# 2. Check RT Process
log_section "Checking RT Process"

RT_PID=$(pgrep mxrc-rt 2>/dev/null || true)
if [ -z "$RT_PID" ]; then
    log_error "mxrc-rt process not found. Start it first:"
    log_info "  sudo systemctl start mxrc-rt"
    exit 1
fi

log_info "RT Process PID: $RT_PID"

# Check scheduling policy
SCHED=$(chrt -p $RT_PID 2>/dev/null | grep policy | awk '{print $3}')
log_info "Scheduling Policy: $SCHED"
if [ "$SCHED" != "SCHED_FIFO" ]; then
    log_warn "RT process is not using SCHED_FIFO!"
fi

# Check CPU affinity
AFFINITY=$(taskset -cp $RT_PID 2>/dev/null | awk -F: '{print $2}' | tr -d ' ')
log_info "CPU Affinity: $AFFINITY"

# 3. Baseline Test (Without WebAPI)
log_section "Running Baseline Test (Without WebAPI)"

log_info "Stopping WebAPI if running..."
systemctl stop mxrc-webapi 2>/dev/null || true
sleep 2

log_info "Running cyclictest (baseline)... This will take ~2 minutes"
cyclictest -p 80 -t 2 -a 2,3 -n -m -l 100000 -q > "$RESULTS_DIR/cyclictest-baseline.txt" 2>&1 &
CYCLIC_PID=$!

# Monitor CPU usage during baseline
mpstat -P 2,3 1 60 > "$RESULTS_DIR/cpu-baseline.txt" &
MPSTAT_PID=$!

wait $CYCLIC_PID
kill $MPSTAT_PID 2>/dev/null || true

log_info "Baseline test completed"

# Extract baseline results
BASELINE_MAX=$(grep "Max Latencies" "$RESULTS_DIR/cyclictest-baseline.txt" | awk '{print $4}')
log_info "Baseline Max Latency: ${BASELINE_MAX}μs"

# 4. WebAPI Idle Test
log_section "Running WebAPI Idle Test"

log_info "Starting WebAPI..."
systemctl start mxrc-webapi
sleep 5

# Check if WebAPI started
if ! systemctl is-active --quiet mxrc-webapi; then
    log_error "Failed to start WebAPI"
    exit 1
fi

WEBAPI_PID=$(pgrep -f "mxrc.*webapi" | head -1)
log_info "WebAPI PID: $WEBAPI_PID"

# Check WebAPI CPU affinity
WEBAPI_AFFINITY=$(taskset -cp $WEBAPI_PID 2>/dev/null | awk -F: '{print $2}' | tr -d ' ')
log_info "WebAPI CPU Affinity: $WEBAPI_AFFINITY"

if [[ "$WEBAPI_AFFINITY" == *"2"* ]] || [[ "$WEBAPI_AFFINITY" == *"3"* ]]; then
    log_error "WebAPI is running on RT cores! This will cause RT interference!"
fi

log_info "Running cyclictest (WebAPI idle)... This will take ~2 minutes"
cyclictest -p 80 -t 2 -a 2,3 -n -m -l 100000 -q > "$RESULTS_DIR/cyclictest-idle.txt" 2>&1 &
CYCLIC_PID=$!

mpstat -P ALL 1 60 > "$RESULTS_DIR/cpu-idle.txt" &
MPSTAT_PID=$!

wait $CYCLIC_PID
kill $MPSTAT_PID 2>/dev/null || true

log_info "Idle test completed"

IDLE_MAX=$(grep "Max Latencies" "$RESULTS_DIR/cyclictest-idle.txt" | awk '{print $4}')
log_info "Idle Max Latency: ${IDLE_MAX}μs"

# 5. WebAPI Load Test
log_section "Running WebAPI Load Test"

log_info "Generating HTTP load (1000 req/s)..."

# Start load generators in background
ab -n 60000 -c 10 http://localhost:8080/api/health/live > "$RESULTS_DIR/ab-results.txt" 2>&1 &
AB_PID=$!

sleep 2

log_info "Running cyclictest (WebAPI under load)... This will take ~2 minutes"
cyclictest -p 80 -t 2 -a 2,3 -n -m -l 100000 -q > "$RESULTS_DIR/cyclictest-load.txt" 2>&1 &
CYCLIC_PID=$!

mpstat -P ALL 1 60 > "$RESULTS_DIR/cpu-load.txt" &
MPSTAT_PID=$!

# Monitor memory
while ps -p $CYCLIC_PID > /dev/null 2>&1; do
    RT_MEM=$(ps -p $RT_PID -o rss= 2>/dev/null || echo "0")
    WEBAPI_MEM=$(ps -p $WEBAPI_PID -o rss= 2>/dev/null || echo "0")
    echo "$(date +%s),$RT_MEM,$WEBAPI_MEM" >> "$RESULTS_DIR/memory-usage.csv"
    sleep 5
done

wait $CYCLIC_PID
wait $AB_PID 2>/dev/null || true
kill $MPSTAT_PID 2>/dev/null || true

log_info "Load test completed"

LOAD_MAX=$(grep "Max Latencies" "$RESULTS_DIR/cyclictest-load.txt" | awk '{print $4}')
log_info "Load Max Latency: ${LOAD_MAX}μs"

# 6. Cache Miss Analysis (if perf available)
if command -v perf &> /dev/null; then
    log_section "Analyzing Cache Performance"

    log_info "Measuring cache misses (10 seconds)..."
    perf stat -e cache-misses,cache-references -p $RT_PID sleep 10 > "$RESULTS_DIR/perf-cache.txt" 2>&1 || true

    if [ -f "$RESULTS_DIR/perf-cache.txt" ]; then
        CACHE_MISSES=$(grep cache-misses "$RESULTS_DIR/perf-cache.txt" | awk '{print $1}' | tr -d ',' || echo "N/A")
        log_info "Cache Misses: $CACHE_MISSES"
    fi
fi

# 7. Generate Report
log_section "Generating Report"

REPORT="$RESULTS_DIR/REPORT.txt"

cat > "$REPORT" <<EOF
MXRC WebAPI RT Impact Test Report
==================================
Date: $(date)
Hostname: $(hostname)
Kernel: $(uname -r)

System Configuration
--------------------
RT Process PID: $RT_PID
RT CPU Affinity: $AFFINITY
RT Scheduling: $SCHED

WebAPI PID: $WEBAPI_PID
WebAPI CPU Affinity: $WEBAPI_AFFINITY

Test Results
------------
Baseline (No WebAPI):
  Max Latency: ${BASELINE_MAX}μs

WebAPI Idle:
  Max Latency: ${IDLE_MAX}μs
  Increase: $((IDLE_MAX - BASELINE_MAX))μs ($((100 * (IDLE_MAX - BASELINE_MAX) / BASELINE_MAX))%)

WebAPI Under Load:
  Max Latency: ${LOAD_MAX}μs
  Increase: $((LOAD_MAX - BASELINE_MAX))μs ($((100 * (LOAD_MAX - BASELINE_MAX) / BASELINE_MAX))%)

Assessment
----------
EOF

# Determine pass/fail
if [ $((LOAD_MAX - BASELINE_MAX)) -lt $((BASELINE_MAX / 10)) ]; then
    echo "✅ PASS: RT impact < 10% - Excellent!" >> "$REPORT"
    STATUS="${GREEN}PASS${NC}"
elif [ $((LOAD_MAX - BASELINE_MAX)) -lt $((BASELINE_MAX / 5)) ]; then
    echo "✅ PASS: RT impact < 20% - Acceptable" >> "$REPORT"
    STATUS="${GREEN}PASS${NC}"
elif [ $LOAD_MAX -lt 200 ]; then
    echo "⚠️  MARGINAL: RT impact significant but latency < 200μs" >> "$REPORT"
    STATUS="${YELLOW}MARGINAL${NC}"
else
    echo "❌ FAIL: RT impact unacceptable" >> "$REPORT"
    STATUS="${RED}FAIL${NC}"
fi

cat "$REPORT"

log_section "Test Complete"
log_info "Full results saved to: $RESULTS_DIR"
log_info "Report: $REPORT"
echo -e "\nOverall Status: $STATUS\n"

# Offer to view detailed results
read -p "View detailed cyclictest results? [y/N] " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo -e "\n--- Baseline ---"
    tail -20 "$RESULTS_DIR/cyclictest-baseline.txt"
    echo -e "\n--- Load ---"
    tail -20 "$RESULTS_DIR/cyclictest-load.txt"
fi

exit 0
