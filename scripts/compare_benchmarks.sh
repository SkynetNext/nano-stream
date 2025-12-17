#!/bin/bash
# Performance comparison script for Nano-Stream (C++) vs Disruptor (Java)

set -e

echo "# Performance Comparison: Nano-Stream (C++) vs Disruptor (Java)"
echo ""
echo "Generated: $(date -u +"%Y-%m-%d %H:%M:%S UTC")"
echo ""

# Check if files exist
CPP_FILE="benchmark_cpp.json"
JAVA_FILE="benchmark_java.json"

# Extract hardware information from C++ benchmark JSON
if [ -f "$CPP_FILE" ] && command -v jq &> /dev/null; then
  echo "## Test Environment"
  echo ""
  if jq -e '.context' "$CPP_FILE" > /dev/null 2>&1; then
    echo "### Hardware Information"
    echo ""
    jq -r '.context | 
      "| Property | Value |
|----------|-------|
| Host | \(.host_name // "N/A") |
| CPU Cores | \(.num_cpus // "N/A") |
| CPU Frequency | \(.mhz_per_cpu // "N/A") MHz |
| CPU Scaling | \(if .cpu_scaling_enabled then "Enabled" else "Disabled" end) |
| ASLR | \(if .aslr_enabled then "Enabled" else "Disabled" end) |"' "$CPP_FILE" 2>/dev/null || echo "Hardware information not available"
    
    echo ""
    echo "### Cache Information"
    echo ""
    jq -r '.context.caches[]? | 
      "| Level \(.level) \(.type) Cache | \(.size) bytes (shared by \(.num_sharing) cores) |"' "$CPP_FILE" 2>/dev/null | head -10 || echo "Cache information not available"
    echo ""
    
    echo "### NUMA Information"
    echo ""
    # Check if numactl is available and get NUMA info
    if command -v numactl &> /dev/null; then
      echo "| Property | Value |"
      echo "|----------|-------|"
      numactl --hardware 2>/dev/null | grep -E "^available:|^node" | while IFS= read -r line; do
        if [[ $line =~ ^available: ]]; then
          echo "| NUMA Nodes Available | $(echo "$line" | cut -d: -f2 | xargs) |"
        elif [[ $line =~ ^node ]]; then
          node_info=$(echo "$line" | sed 's/node \([0-9]*\) cpus: \(.*\) size: \([0-9]*\) MB/node \1: \2 CPUs, \3 MB/')
          echo "| $node_info |"
        fi
      done || echo "| NUMA information | Not available |"
    else
      # Fallback: try to get from /proc
      if [ -f /proc/cpuinfo ]; then
        numa_nodes=$(ls -d /sys/devices/system/node/node* 2>/dev/null | wc -l)
        if [ "$numa_nodes" -gt 0 ]; then
          echo "| NUMA Nodes | $numa_nodes |"
          echo "| CPU to Node mapping | See /proc/cpuinfo |"
        else
          echo "| NUMA | Not available or single node |"
        fi
      else
        echo "| NUMA | Information not available |"
      fi
    fi
    echo ""
  fi
fi

# Also check in reference/disruptor/build/reports/jmh/ if Java file not found
if [ ! -f "$JAVA_FILE" ] && [ -f "reference/disruptor/build/reports/jmh/result.json" ]; then
  cp reference/disruptor/build/reports/jmh/result.json "$JAVA_FILE"
  echo "Copied Java benchmark file from reference/disruptor/build/reports/jmh/result.json"
fi

# Extract C++ benchmark results
if [ -f "$CPP_FILE" ]; then
  echo "## C++ Benchmarks (Nano-Stream)"
  echo ""
  echo "### Key Metrics"
  echo ""
  
  # Extract throughput metrics (simplified parsing)
  if command -v jq &> /dev/null; then
    echo "| Benchmark | Throughput (ops/sec) | Latency (ns) |"
    echo "|-----------|---------------------|--------------|"
    
    jq -r '.benchmarks[] | 
      (.items_per_second // 0) as $throughput |
      (.time_unit // "ns") as $unit |
      (.real_time // 0) as $latency_raw |
      (if $unit == "ns" then $latency_raw
       elif $unit == "us" then ($latency_raw * 1000)
       elif $unit == "ms" then ($latency_raw * 1000000)
       elif $unit == "s" then ($latency_raw * 1000000000)
       else $latency_raw end) as $latency_ns |
      "| \(.name) | \($throughput) | \($latency_ns) ns |"' \
      "$CPP_FILE" | head -30
  else
    echo "\`\`\`json"
    head -100 "$CPP_FILE"
    echo "\`\`\`"
  fi
  echo ""
else
  echo "> C++ benchmark file not found: $CPP_FILE"
  echo ""
fi

# Extract Java benchmark results
if [ -f "$JAVA_FILE" ]; then
  echo "## Java Benchmarks (Disruptor)"
  echo ""
  echo "### Key Metrics"
  echo ""
  
  if command -v jq &> /dev/null; then
    echo "| Benchmark | Mode | Score (raw) | Score (normalized) |"
    echo "|-----------|------|-------------|--------------------|"
    
    # JMH JSON format: array of objects, each has .benchmark and .primaryMetric.score
    # Filter to only throughput tests and core benchmarks matching C++ tests
    jq -r '.[] |
      select(.benchmark | test("SingleProducerSingleConsumer|SequenceBenchmark|RingBufferBenchmark")) |
      .primaryMetric.score as $score |
      .primaryMetric.scoreUnit as $unit |
      (if $unit == "ops/us" then ($score * 1000000)
       elif $unit == "ops/ms" then ($score * 1000)
       elif $unit == "ops/s" then $score
       elif $unit == "ns/op" then (1000000000 / $score)
       elif $unit == "us/op" then (1000000 / $score)
       elif $unit == "ms/op" then (1000 / $score)
       else null end) as $normalized |
      (if $unit == "ns/op" then "ops/s (derived)"
       elif $unit == "us/op" then "ops/s (derived)"
       elif $unit == "ms/op" then "ops/s (derived)"
       elif $unit == "ops/us" then "ops/s"
       elif $unit == "ops/ms" then "ops/s"
       elif $unit == "ops/s" then "ops/s"
       else "n/a" end) as $normalized_unit |
      "| \(.benchmark) | \(.mode) | \($score) \($unit) | \(if $normalized == null then "n/a" else ($normalized|tostring) end) \($normalized_unit) |"' \
      "$JAVA_FILE" | head -30
  else
    echo "\`\`\`json"
    head -100 "$JAVA_FILE"
    echo "\`\`\`"
  fi
  echo ""
else
  echo "> Java benchmark file not found: $JAVA_FILE"
  echo ""
fi

# Java perf test results
if [ -f "benchmark_java_perf.txt" ]; then
  echo "## Java Perf Test Results (Disruptor)"
  echo ""
  echo "\`\`\`"
  cat benchmark_java_perf.txt
  echo "\`\`\`"
  echo ""
fi

# Performance Analysis and Comparison
if [ -f "$CPP_FILE" ] && [ -f "$JAVA_FILE" ] && command -v jq &> /dev/null; then
  echo "## Performance Analysis & Comparison"
  echo ""
  
  # Extract key metrics for comparison
  echo "### Key Performance Metrics"
  echo ""
  echo "| Operation | C++ (Nano-Stream) | Java (Disruptor) | C++/Java Ratio | Winner |"
  echo "|-----------|-------------------|------------------|----------------|--------|"
  
  # Sequence Get/Read - C++ uses BM_SequenceGet, Java uses SequenceUnsafe read1/read2 average
  CPP_SEQ_GET=$(jq -r '.benchmarks[] | select(.name == "BM_SequenceGet") | .items_per_second' "$CPP_FILE" 2>/dev/null | head -1)
  JAVA_SEQ_READ=$(jq -r '.[] | select(.benchmark | test("SequenceBenchmark\\.SequenceUnsafe")) | select(.benchmark | test("\\.read1$|\\.read2$")) | (.primaryMetric.score * (if .primaryMetric.scoreUnit == "ops/us" then 1000000 else 1 end))' "$JAVA_FILE" 2>/dev/null | awk '{sum+=$1; count++} END {if(count>0) print sum/count; else print 0}')
  
  if [ ! -z "$CPP_SEQ_GET" ] && [ ! -z "$JAVA_SEQ_READ" ] && [ "$(echo "$JAVA_SEQ_READ > 0" | bc -l 2>/dev/null)" = "1" ] && [ "$(echo "$CPP_SEQ_GET > 0" | bc -l 2>/dev/null)" = "1" ]; then
    RATIO=$(echo "scale=2; $CPP_SEQ_GET / $JAVA_SEQ_READ" | bc -l 2>/dev/null)
    if [ ! -z "$RATIO" ] && [ "$(echo "$RATIO > 0" | bc -l 2>/dev/null)" = "1" ]; then
      WINNER=$([ "$(echo "$RATIO > 1" | bc -l 2>/dev/null)" = "1" ] && echo "C++" || echo "Java")
      printf "| Sequence Read | %.2e ops/s | %.2e ops/s | %.2fx | **%s** |\n" "$CPP_SEQ_GET" "$JAVA_SEQ_READ" "$RATIO" "$WINNER"
    fi
  fi
  
  # Sequence Set - C++ uses BM_SequenceSet, Java uses SequenceUnsafe setValue1
  CPP_SEQ_SET=$(jq -r '.benchmarks[] | select(.name == "BM_SequenceSet") | .items_per_second' "$CPP_FILE" 2>/dev/null | head -1)
  JAVA_SEQ_SET=$(jq -r '.[] | select(.benchmark | test("SequenceBenchmark\\.SequenceUnsafe")) | select(.benchmark | test("setValue1")) | (.primaryMetric.score * (if .primaryMetric.scoreUnit == "ops/us" then 1000000 else 1 end))' "$JAVA_FILE" 2>/dev/null | head -1)
  
  if [ ! -z "$CPP_SEQ_SET" ] && [ ! -z "$JAVA_SEQ_SET" ] && [ "$(echo "$JAVA_SEQ_SET > 0" | bc -l 2>/dev/null)" = "1" ] && [ "$(echo "$CPP_SEQ_SET > 0" | bc -l 2>/dev/null)" = "1" ]; then
    RATIO=$(echo "scale=2; $CPP_SEQ_SET / $JAVA_SEQ_SET" | bc -l 2>/dev/null)
    if [ ! -z "$RATIO" ] && [ "$(echo "$RATIO > 0" | bc -l 2>/dev/null)" = "1" ]; then
      WINNER=$([ "$(echo "$RATIO > 1" | bc -l 2>/dev/null)" = "1" ] && echo "C++" || echo "Java")
      printf "| Sequence Set | %.2e ops/s | %.2e ops/s | %.2fx | **%s** |\n" "$CPP_SEQ_SET" "$JAVA_SEQ_SET" "$RATIO" "$WINNER"
    fi
  fi
  
  # Sequence Increment - C++ uses BM_SequenceIncrementAndGet, Java uses SequenceUnsafe incrementValue2
  CPP_SEQ_INC=$(jq -r '.benchmarks[] | select(.name == "BM_SequenceIncrementAndGet") | .items_per_second' "$CPP_FILE" 2>/dev/null | head -1)
  JAVA_SEQ_INC=$(jq -r '.[] | select(.benchmark | test("SequenceBenchmark\\.SequenceUnsafe")) | select(.benchmark | test("incrementValue2")) | (.primaryMetric.score * (if .primaryMetric.scoreUnit == "ops/us" then 1000000 else 1 end))' "$JAVA_FILE" 2>/dev/null | head -1)
  
  if [ ! -z "$CPP_SEQ_INC" ] && [ ! -z "$JAVA_SEQ_INC" ] && [ "$(echo "$JAVA_SEQ_INC > 0" | bc -l 2>/dev/null)" = "1" ] && [ "$(echo "$CPP_SEQ_INC > 0" | bc -l 2>/dev/null)" = "1" ]; then
    RATIO=$(echo "scale=2; $CPP_SEQ_INC / $JAVA_SEQ_INC" | bc -l 2>/dev/null)
    if [ ! -z "$RATIO" ] && [ "$(echo "$RATIO > 0" | bc -l 2>/dev/null)" = "1" ]; then
      WINNER=$([ "$(echo "$RATIO > 1" | bc -l 2>/dev/null)" = "1" ] && echo "C++" || echo "Java")
      printf "| Sequence Increment | %.2e ops/s | %.2e ops/s | %.2fx | **%s** |\n" "$CPP_SEQ_INC" "$JAVA_SEQ_INC" "$RATIO" "$WINNER"
    fi
  fi
  
  # Typical production scenarios (aligned):
  #
  # - SingleProducerSingleConsumer.producing (JMH avgt, ns/op) <-> BM_Typical_SingleProducerSingleConsumer (Google Benchmark)
  # - MultiProducerSingleConsumer.producing (JMH thrpt, ops/ms) <-> BM_Typical_MultiProducerSingleConsumer (Google Benchmark, 4 producers + 1 consumer thread)
  # - MultiProducerSingleConsumer.producingBatch (JMH thrpt, ops/ms, batch=100) <-> BM_Typical_MultiProducerSingleConsumerBatch

  CPP_1P1C=$(jq -r '.benchmarks[] | select(.name == "BM_Typical_SingleProducerSingleConsumer") | .items_per_second' "$CPP_FILE" 2>/dev/null | head -1)
  JAVA_1P1C=$(jq -r '.[] | select(.benchmark | test("SingleProducerSingleConsumer\\.producing$")) |
      (.primaryMetric.score) as $s |
      (.primaryMetric.scoreUnit) as $u |
      (if $u == "ns/op" then (1000000000 / $s)
       elif $u == "us/op" then (1000000 / $s)
       elif $u == "ms/op" then (1000 / $s)
       elif $u == "ops/us" then ($s * 1000000)
       elif $u == "ops/ms" then ($s * 1000)
       elif $u == "ops/s" then $s
       else 0 end)' "$JAVA_FILE" 2>/dev/null | head -1)

  if [ ! -z "$CPP_1P1C" ] && [ ! -z "$JAVA_1P1C" ] && [ "$(echo "$JAVA_1P1C > 0" | bc -l 2>/dev/null)" = "1" ] && [ "$(echo "$CPP_1P1C > 0" | bc -l 2>/dev/null)" = "1" ]; then
    RATIO=$(echo "scale=2; $CPP_1P1C / $JAVA_1P1C" | bc -l 2>/dev/null)
    WINNER=$([ "$(echo "$RATIO > 1" | bc -l 2>/dev/null)" = "1" ] && echo "C++" || echo "Java")
    printf "| 1P1C publish (ring=1<<20) | %.2e ops/s | %.2e ops/s | %.2fx | **%s** |\n" "$CPP_1P1C" "$JAVA_1P1C" "$RATIO" "$WINNER"
  fi

  CPP_MP1C=$(jq -r '.benchmarks[] | select(.name == "BM_Typical_MultiProducerSingleConsumer") | .items_per_second' "$CPP_FILE" 2>/dev/null | head -1)
  JAVA_MP1C=$(jq -r '.[] | select(.benchmark | test("MultiProducerSingleConsumer\\.producing$")) |
      (.primaryMetric.score) as $s |
      (.primaryMetric.scoreUnit) as $u |
      (if $u == "ops/ms" then ($s * 1000)
       elif $u == "ops/us" then ($s * 1000000)
       elif $u == "ops/s" then $s
       else 0 end)' "$JAVA_FILE" 2>/dev/null | head -1)

  if [ ! -z "$CPP_MP1C" ] && [ ! -z "$JAVA_MP1C" ] && [ "$(echo "$JAVA_MP1C > 0" | bc -l 2>/dev/null)" = "1" ] && [ "$(echo "$CPP_MP1C > 0" | bc -l 2>/dev/null)" = "1" ]; then
    RATIO=$(echo "scale=2; $CPP_MP1C / $JAVA_MP1C" | bc -l 2>/dev/null)
    WINNER=$([ "$(echo "$RATIO > 1" | bc -l 2>/dev/null)" = "1" ] && echo "C++" || echo "Java")
    printf "| MP1C publish (P=4, ring=1<<22) | %.2e ops/s | %.2e ops/s | %.2fx | **%s** |\n" "$CPP_MP1C" "$JAVA_MP1C" "$RATIO" "$WINNER"
  fi

  CPP_MP1C_BATCH=$(jq -r '.benchmarks[] | select(.name == "BM_Typical_MultiProducerSingleConsumerBatch") | .items_per_second' "$CPP_FILE" 2>/dev/null | head -1)
  JAVA_MP1C_BATCH=$(jq -r '.[] | select(.benchmark | test("MultiProducerSingleConsumer\\.producingBatch$")) |
      (.primaryMetric.score) as $s |
      (.primaryMetric.scoreUnit) as $u |
      (if $u == "ops/ms" then ($s * 1000)
       elif $u == "ops/us" then ($s * 1000000)
       elif $u == "ops/s" then $s
       else 0 end)' "$JAVA_FILE" 2>/dev/null | head -1)

  if [ ! -z "$CPP_MP1C_BATCH" ] && [ ! -z "$JAVA_MP1C_BATCH" ] && [ "$(echo "$JAVA_MP1C_BATCH > 0" | bc -l 2>/dev/null)" = "1" ] && [ "$(echo "$CPP_MP1C_BATCH > 0" | bc -l 2>/dev/null)" = "1" ]; then
    RATIO=$(echo "scale=2; $CPP_MP1C_BATCH / $JAVA_MP1C_BATCH" | bc -l 2>/dev/null)
    WINNER=$([ "$(echo "$RATIO > 1" | bc -l 2>/dev/null)" = "1" ] && echo "C++" || echo "Java")
    printf "| MP1C batch publish (P=4, batch=100) | %.2e ops/s | %.2e ops/s | %.2fx | **%s** |\n" "$CPP_MP1C_BATCH" "$JAVA_MP1C_BATCH" "$RATIO" "$WINNER"
  fi
  
  echo ""
  echo "### Analysis Notes"
  echo ""
  echo "- **Latency vs Throughput**: JMH may report either throughput (ops/...) or average time (ns/op). This report prints both raw and a normalized ops/s when possible."
  echo "- **Throughput**: Higher is better. Both implementations show excellent performance."
  echo "- **Ratio > 1**: C++ is faster. **Ratio < 1**: Java is faster."
  echo ""
fi

# Summary comparison
echo "## Summary"
echo ""
echo "### Performance Highlights"
echo ""
echo "- **C++ (Nano-Stream)**: Header-only, zero-allocation, sub-nanosecond latency"
echo "- **Java (Disruptor)**: Mature library, JVM-optimized, battle-tested"
echo ""
echo "> Note: Direct comparison requires matching test scenarios. Results may vary based on hardware and JVM tuning."

