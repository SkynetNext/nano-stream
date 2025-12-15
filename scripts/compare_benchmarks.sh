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
      (.iterations * 1000000000 / .real_time) as $throughput |
      "| \(.name) | \($throughput) | \(.real_time) |"' \
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
    echo "| Benchmark | Throughput (ops/sec) | Score |"
    echo "|-----------|---------------------|-------|"
    
    # JMH JSON format: .benchmarks[].benchmark and .benchmarks[].primaryMetric.score
    # Filter to only throughput tests and core benchmarks matching C++ tests
    jq -r '.benchmarks[] | 
      select(.mode == "thrpt") |
      select(.benchmark | test("SequenceBenchmark|RingBufferBenchmark|SingleProducerSingleConsumer")) |
      (.primaryMetric.score * (
        if .primaryMetric.scoreUnit == "ops/us" then 1000000
        elif .primaryMetric.scoreUnit == "ops/ms" then 1000
        elif .primaryMetric.scoreUnit == "ops/s" then 1
        else 1 end
      )) as $ops_per_sec |
      "| \(.benchmark) | \($ops_per_sec) | \(.primaryMetric.score) \(.primaryMetric.scoreUnit) |"' \
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

# Summary comparison
echo "## Summary"
echo ""
echo "### Performance Highlights"
echo ""
echo "- **C++ (Nano-Stream)**: Header-only, zero-allocation, sub-nanosecond latency"
echo "- **Java (Disruptor)**: Mature library, JVM-optimized, battle-tested"
echo ""
echo "> Note: Direct comparison requires matching test scenarios. Results may vary based on hardware and JVM tuning."

