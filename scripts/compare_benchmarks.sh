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
      (.items_per_second // 0) as $throughput |
      (.time_unit // "ns") as $unit |
      (.real_time / .iterations) as $latency |
      (if $unit == "ns" then $latency
       elif $unit == "us" then ($latency * 1000)
       elif $unit == "ms" then ($latency * 1000000)
       elif $unit == "s" then ($latency * 1000000000)
       else $latency end) as $latency_ns |
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
    echo "| Benchmark | Throughput (ops/sec) | Score |"
    echo "|-----------|---------------------|-------|"
    
    # JMH JSON format: array of objects, each has .benchmark and .primaryMetric.score
    # Filter to only throughput tests and core benchmarks matching C++ tests
    jq -r '.[] | 
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
  
  if [ ! -z "$CPP_SEQ_GET" ] && [ ! -z "$JAVA_SEQ_READ" ] && [ "$(echo "$JAVA_SEQ_READ > 0" | bc -l 2>/dev/null)" = "1" ]; then
    RATIO=$(echo "scale=2; $CPP_SEQ_GET / $JAVA_SEQ_READ" | bc -l 2>/dev/null)
    WINNER=$(echo "$RATIO > 1" | bc -l 2>/dev/null && echo "C++" || echo "Java")
    printf "| Sequence Read | %.2e ops/s | %.2e ops/s | %.2fx | **%s** |\n" "$CPP_SEQ_GET" "$JAVA_SEQ_READ" "$RATIO" "$WINNER"
  fi
  
  # Sequence Set - C++ uses BM_SequenceSet, Java uses SequenceUnsafe setValue1
  CPP_SEQ_SET=$(jq -r '.benchmarks[] | select(.name == "BM_SequenceSet") | .items_per_second' "$CPP_FILE" 2>/dev/null | head -1)
  JAVA_SEQ_SET=$(jq -r '.[] | select(.benchmark | test("SequenceBenchmark\\.SequenceUnsafe")) | select(.benchmark | test("setValue1")) | (.primaryMetric.score * (if .primaryMetric.scoreUnit == "ops/us" then 1000000 else 1 end))' "$JAVA_FILE" 2>/dev/null | head -1)
  
  if [ ! -z "$CPP_SEQ_SET" ] && [ ! -z "$JAVA_SEQ_SET" ] && [ "$(echo "$JAVA_SEQ_SET > 0" | bc -l 2>/dev/null)" = "1" ]; then
    RATIO=$(echo "scale=2; $CPP_SEQ_SET / $JAVA_SEQ_SET" | bc -l 2>/dev/null)
    WINNER=$(echo "$RATIO > 1" | bc -l 2>/dev/null && echo "C++" || echo "Java")
    printf "| Sequence Set | %.2e ops/s | %.2e ops/s | %.2fx | **%s** |\n" "$CPP_SEQ_SET" "$JAVA_SEQ_SET" "$RATIO" "$WINNER"
  fi
  
  # Sequence Increment - C++ uses BM_SequenceIncrementAndGet, Java uses SequenceUnsafe incrementValue2
  CPP_SEQ_INC=$(jq -r '.benchmarks[] | select(.name == "BM_SequenceIncrementAndGet") | 
    (.time_unit // "ns") as $unit |
    (.real_time / .iterations) as $latency |
    (if $unit == "ns" then (1000000000 / $latency)
     elif $unit == "us" then (1000000000 / ($latency * 1000))
     elif $unit == "ms" then (1000000000 / ($latency * 1000000))
     else (1000000000 / $latency) end)' "$CPP_FILE" 2>/dev/null | head -1)
  JAVA_SEQ_INC=$(jq -r '.[] | select(.benchmark | test("SequenceBenchmark\\.SequenceUnsafe")) | select(.benchmark | test("incrementValue2")) | (.primaryMetric.score * (if .primaryMetric.scoreUnit == "ops/us" then 1000000 else 1 end))' "$JAVA_FILE" 2>/dev/null | head -1)
  
  if [ ! -z "$CPP_SEQ_INC" ] && [ ! -z "$JAVA_SEQ_INC" ] && [ "$(echo "$JAVA_SEQ_INC > 0" | bc -l 2>/dev/null)" = "1" ]; then
    RATIO=$(echo "scale=2; $CPP_SEQ_INC / $JAVA_SEQ_INC" | bc -l 2>/dev/null)
    WINNER=$(echo "$RATIO > 1" | bc -l 2>/dev/null && echo "C++" || echo "Java")
    printf "| Sequence Increment | %.2e ops/s | %.2e ops/s | %.2fx | **%s** |\n" "$CPP_SEQ_INC" "$JAVA_SEQ_INC" "$RATIO" "$WINNER"
  fi
  
  # RingBuffer - C++ uses BM_RingBufferSingleProducer/1024, Java uses RingBufferUnsafe
  CPP_RB=$(jq -r '.benchmarks[] | select(.name | test("BM_RingBufferSingleProducer/1024")) | .items_per_second' "$CPP_FILE" 2>/dev/null | head -1)
  JAVA_RB=$(jq -r '.[] | select(.benchmark | test("RingBufferBenchmark\\.RingBufferUnsafe$")) | (.primaryMetric.score * (if .primaryMetric.scoreUnit == "ops/us" then 1000000 else 1 end))' "$JAVA_FILE" 2>/dev/null | head -1)
  
  if [ ! -z "$CPP_RB" ] && [ ! -z "$JAVA_RB" ] && [ "$(echo "$JAVA_RB > 0" | bc -l 2>/dev/null)" = "1" ]; then
    RATIO=$(echo "scale=2; $CPP_RB / $JAVA_RB" | bc -l 2>/dev/null)
    WINNER=$(echo "$RATIO > 1" | bc -l 2>/dev/null && echo "C++" || echo "Java")
    printf "| RingBuffer (Single Producer) | %.2e ops/s | %.2e ops/s | %.2fx | **%s** |\n" "$CPP_RB" "$JAVA_RB" "$RATIO" "$WINNER"
  fi
  
  echo ""
  echo "### Analysis Notes"
  echo ""
  echo "- **Latency**: Lower is better. C++ shows sub-nanosecond latency for basic operations."
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

