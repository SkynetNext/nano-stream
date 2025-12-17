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

# Helpers
awk_ratio() {
  # Usage: awk_ratio <num> <den>
  # Prints ratio with 2 decimal places, or empty if invalid.
  awk -v n="$1" -v d="$2" 'BEGIN { if (d>0 && n>0) printf "%.2f", (n/d); }'
}

winner_from_ratio() {
  # Usage: winner_from_ratio <ratio>
  awk -v r="$1" 'BEGIN { if (r=="") exit 1; if (r>1.0) printf "C++"; else printf "Java"; }'
}

echo "## Test Environment"
echo ""

# Extract hardware information from C++ benchmark JSON (preferred) and OS tools (fallback).
if [ -f "$CPP_FILE" ] && command -v jq &> /dev/null && jq -e '.context' "$CPP_FILE" > /dev/null 2>&1; then
  echo "### Hardware Information"
  echo ""
  jq -r '.context |
    "| Property | Value |
|----------|-------|
| Host | \(.host_name // "N/A") |
| CPU Cores | \(.num_cpus // "N/A") |
| CPU Frequency | \(.mhz_per_cpu // "N/A") MHz |
| CPU Scaling | \(if .cpu_scaling_enabled then "Enabled" else "Disabled" end) |
| ASLR | \(if .aslr_enabled then "Enabled" else "Disabled" end) |"' "$CPP_FILE" 2>/dev/null || true

  echo ""
  echo "### Cache Information"
  echo ""
  jq -r '.context.caches[]? |
    "| Level \(.level) \(.type) Cache | \(.size) bytes (shared by \(.num_sharing) cores) |"' "$CPP_FILE" 2>/dev/null | head -10 || true
  echo ""
else
  echo "> C++ benchmark context not available (missing \`$CPP_FILE\` or \`jq\`)."
  echo ""
fi

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

# Also check in reference/disruptor/build/reports/jmh/ if Java file not found
if [ ! -f "$JAVA_FILE" ] && [ -f "reference/disruptor/build/reports/jmh/result.json" ]; then
  cp reference/disruptor/build/reports/jmh/result.json "$JAVA_FILE"
  echo "Copied Java benchmark file from reference/disruptor/build/reports/jmh/result.json"
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

  # Typical production scenarios (aligned):
  #
  # - SingleProducerSingleConsumer.producing (JMH avgt, ns/op) <-> Typical_SingleProducerSingleConsumer (Google Benchmark)
  # - MultiProducerSingleConsumer.producing (JMH thrpt, ops/ms) <-> Typical_MultiProducerSingleConsumer (Google Benchmark, 4 producer benchmark threads; consumer is background)
  # - MultiProducerSingleConsumer.producingBatch (JMH thrpt, ops/ms, batch=100) <-> Typical_MultiProducerSingleConsumerBatch (same measurement scope as above)

  CPP_1P1C=$(jq -r '.benchmarks[] | select(.name == "JMH_SingleProducerSingleConsumer_producing") | .items_per_second' "$CPP_FILE" 2>/dev/null | head -1)
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

  if [ ! -z "$CPP_1P1C" ] && [ ! -z "$JAVA_1P1C" ]; then
    RATIO=$(awk_ratio "$CPP_1P1C" "$JAVA_1P1C")
    WINNER=$(winner_from_ratio "$RATIO" 2>/dev/null || true)
    printf "| 1P1C publish (ring=1<<20) | %.2e ops/s | %.2e ops/s | %.2fx | **%s** |\n" "$CPP_1P1C" "$JAVA_1P1C" "$RATIO" "$WINNER"
  fi

  CPP_MP1C=$(jq -r '.benchmarks[] | select(.name == "JMH_MultiProducerSingleConsumer_producing") | .items_per_second' "$CPP_FILE" 2>/dev/null | head -1)
  JAVA_MP1C=$(jq -r '.[] | select(.benchmark | test("MultiProducerSingleConsumer\\.producing$")) |
      (.primaryMetric.score) as $s |
      (.primaryMetric.scoreUnit) as $u |
      (if $u == "ops/ms" then ($s * 1000)
       elif $u == "ops/us" then ($s * 1000000)
       elif $u == "ops/s" then $s
       else 0 end)' "$JAVA_FILE" 2>/dev/null | head -1)

  if [ ! -z "$CPP_MP1C" ] && [ ! -z "$JAVA_MP1C" ]; then
    RATIO=$(awk_ratio "$CPP_MP1C" "$JAVA_MP1C")
    WINNER=$(winner_from_ratio "$RATIO" 2>/dev/null || true)
    printf "| MP1C publish (P=4, ring=1<<22) | %.2e ops/s | %.2e ops/s | %.2fx | **%s** |\n" "$CPP_MP1C" "$JAVA_MP1C" "$RATIO" "$WINNER"
  fi

  CPP_MP1C_BATCH=$(jq -r '.benchmarks[] | select(.name == "JMH_MultiProducerSingleConsumer_producingBatch") | .items_per_second' "$CPP_FILE" 2>/dev/null | head -1)
  JAVA_MP1C_BATCH=$(jq -r '.[] | select(.benchmark | test("MultiProducerSingleConsumer\\.producingBatch$")) |
      (.primaryMetric.score) as $s |
      (.primaryMetric.scoreUnit) as $u |
      (if $u == "ops/ms" then ($s * 1000)
       elif $u == "ops/us" then ($s * 1000000)
       elif $u == "ops/s" then $s
       else 0 end)' "$JAVA_FILE" 2>/dev/null | head -1)

  if [ ! -z "$CPP_MP1C_BATCH" ] && [ ! -z "$JAVA_MP1C_BATCH" ]; then
    RATIO=$(awk_ratio "$CPP_MP1C_BATCH" "$JAVA_MP1C_BATCH")
    WINNER=$(winner_from_ratio "$RATIO" 2>/dev/null || true)
    printf "| MP1C batch publish (P=4, batch=100) | %.2e ops/s | %.2e ops/s | %.2fx | **%s** |\n" "$CPP_MP1C_BATCH" "$JAVA_MP1C_BATCH" "$RATIO" "$WINNER"
  fi
  
  echo ""
  echo "### Analysis Notes"
  echo ""
  echo "- **Scope**: These comparisons are limited to aligned, production-style scenarios (1P1C, MP1C, MP1C batch)."
  echo "- **Units**: Java avgt (ns/op) is converted to ops/s; Java thrpt is converted to ops/s."
  echo "- **Ratio**: >1 means C++ higher ops/s; <1 means Java higher ops/s."
  echo ""
fi

echo "## Summary"
echo ""
echo "- This report intentionally focuses on the minimum comparable set."

