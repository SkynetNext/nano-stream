#!/bin/bash
# Performance comparison script for Nano-Stream (C++) vs Disruptor (Java)

set -e

echo "# Performance Comparison: Nano-Stream (C++) vs Disruptor (Java)"
echo ""
echo "Generated: $(date -u +"%Y-%m-%d %H:%M:%S UTC")"
echo ""

# Extract C++ benchmark results
if [ -f "benchmark_cpp.json" ]; then
  echo "## C++ Benchmarks (Nano-Stream)"
  echo ""
  echo "### Key Metrics"
  echo ""
  
  # Extract throughput metrics (simplified parsing)
  if command -v jq &> /dev/null; then
    echo "| Benchmark | Throughput (ops/sec) | Latency (ns) |"
    echo "|-----------|---------------------|--------------|"
    
    jq -r '.benchmarks[] | "| \(.name) | \(.iterations * 1000000000 / .real_time // "N/A") | \(.real_time // "N/A") |"' benchmark_cpp.json | head -20
  else
    echo "\`\`\`json"
    head -100 benchmark_cpp.json
    echo "\`\`\`"
  fi
  echo ""
fi

# Extract Java benchmark results
if [ -f "benchmark_java.json" ]; then
  echo "## Java Benchmarks (Disruptor)"
  echo ""
  echo "### Key Metrics"
  echo ""
  
  if command -v jq &> /dev/null; then
    echo "| Benchmark | Throughput (ops/sec) | Latency (ns) |"
    echo "|-----------|---------------------|--------------|"
    
    jq -r '.benchmarks[] | "| \(.benchmark) | \(.primaryMetric.score // "N/A") | \(.primaryMetric.scoreUnit // "N/A") |"' benchmark_java.json | head -20
  else
    echo "\`\`\`json"
    head -100 benchmark_java.json
    echo "\`\`\`"
  fi
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

