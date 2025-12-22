#!/bin/bash
# Performance comparison script for Disruptor-CPP (C++) vs Disruptor (Java)

set -e

echo "# Performance Comparison: Disruptor-CPP (C++) vs Disruptor (Java)"
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

HAVE_JQ=0
if command -v jq &> /dev/null; then
  HAVE_JQ=1
fi

# Extra system info (best-effort). These complement benchmark_cpp.json context which often lacks CPU model / RAM size.
OS_INFO="$(uname -srm 2>/dev/null || true)"
KERNEL_INFO="$(uname -r 2>/dev/null || true)"
CPU_MODEL=""
MEM_TOTAL=""
if [ -f /proc/cpuinfo ]; then
  CPU_MODEL="$(grep -m1 -E 'model name\\s*:' /proc/cpuinfo | sed 's/.*model name\\s*:\\s*//' || true)"
fi
if [ -z "$CPU_MODEL" ] && command -v lscpu >/dev/null 2>&1; then
  CPU_MODEL="$(lscpu 2>/dev/null | grep -m1 -E '^Model name:' | sed 's/^Model name:\\s*//' || true)"
fi
if [ -f /proc/meminfo ]; then
  # MemTotal is in kB
  MEM_TOTAL="$(awk '/^MemTotal:/ { printf \"%.1f GB\", $2/1024/1024 }' /proc/meminfo 2>/dev/null || true)"
fi

# Extract hardware information from C++ benchmark JSON.
if [ -f "$CPP_FILE" ] && [ "$HAVE_JQ" -eq 1 ] && jq -e '.context' "$CPP_FILE" > /dev/null 2>&1; then
  echo "### Hardware Information"
  echo ""
  jq -r '.context |
    "| Property | Value |
|----------|-------|
| Host | \(.host_name // "N/A") |
| OS | "'"${OS_INFO:-N/A}"'" |
| Kernel | "'"${KERNEL_INFO:-N/A}"'" |
| CPU Model | "'"${CPU_MODEL:-N/A}"'" |
| CPU Cores | \(.num_cpus // "N/A") |
| CPU Frequency | \(.mhz_per_cpu // "N/A") MHz |
| Memory | "'"${MEM_TOTAL:-N/A}"'" |
| CPU Scaling | \(if .cpu_scaling_enabled then "Enabled" else "Disabled" end) |
| ASLR | \(if .aslr_enabled then "Enabled" else "Disabled" end) |"' "$CPP_FILE" 2>/dev/null || true

  echo ""
  echo "### Cache Information"
  echo ""
  jq -r '.context.caches[]? |
    "| Level \(.level) \(.type) Cache | \(.size) bytes (shared by \(.num_sharing) cores) |"' "$CPP_FILE" 2>/dev/null | head -10 || true
  echo ""
elif [ -f "$CPP_FILE" ]; then
  # Fallback without jq (Windows/Git Bash friendly): use python to print the same tables.
  python - "$CPP_FILE" <<'PY' || true
import json, sys, os, platform, re
path = sys.argv[1]
with open(path, "r", encoding="utf-8") as f:
    d = json.load(f)
ctx = d.get("context", {}) or {}
os_info = platform.platform()
kernel = platform.release()
cpu_model = None
mem_total = None
try:
    if os.path.exists("/proc/cpuinfo"):
        with open("/proc/cpuinfo", "r", encoding="utf-8", errors="ignore") as cf:
            for line in cf:
                if line.lower().startswith("model name"):
                    cpu_model = line.split(":", 1)[1].strip()
                    break
    if os.path.exists("/proc/meminfo"):
        with open("/proc/meminfo", "r", encoding="utf-8", errors="ignore") as mf:
            for line in mf:
                if line.startswith("MemTotal:"):
                    kb = int(re.findall(r"\d+", line)[0])
                    mem_total = f"{kb/1024/1024:.1f} GB"
                    break
except Exception:
    pass
print("### Hardware Information\n")
print("| Property | Value |")
print("|----------|-------|")
print(f"| Host | {ctx.get('host_name','N/A')} |")
print(f"| OS | {os_info or 'N/A'} |")
print(f"| Kernel | {kernel or 'N/A'} |")
print(f"| CPU Model | {cpu_model or 'N/A'} |")
print(f"| CPU Cores | {ctx.get('num_cpus','N/A')} |")
mhz = ctx.get("mhz_per_cpu","N/A")
print(f"| CPU Frequency | {mhz} MHz |")
print(f"| Memory | {mem_total or 'N/A'} |")
cpu_scaling = ctx.get("cpu_scaling_enabled", None)
aslr = ctx.get("aslr_enabled", None)
if cpu_scaling is None: cpu_scaling = ctx.get("cpu_scaling", None)
if aslr is None: aslr = ctx.get("aslr", None)
def yn(v):
    if v is None: return "N/A"
    return "Enabled" if bool(v) else "Disabled"
print(f"| CPU Scaling | {yn(cpu_scaling)} |")
print(f"| ASLR | {yn(aslr)} |")
print("\n### Cache Information\n")
caches = ctx.get("caches", []) or []
for c in caches[:10]:
    lvl = c.get("level","?")
    typ = c.get("type","?")
    size = c.get("size","?")
    share = c.get("num_sharing","?")
    print(f"| Level {lvl} {typ} Cache | {size} bytes (shared by {share} cores) |")
print("")
PY
else
  echo "> C++ benchmark context not available (missing \`$CPP_FILE\`)."
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
if [ -f "$CPP_FILE" ] && [ -f "$JAVA_FILE" ] && [ "$HAVE_JQ" -eq 1 ]; then
  echo "## Performance Analysis & Comparison"
  echo ""
  
  # Extract key metrics for comparison
  echo "### Key Performance Metrics"
  echo ""
  echo "| Operation | C++ (Disruptor-CPP) | Java (Disruptor) | C++/Java Ratio | Winner |"
  echo "|-----------|-------------------|------------------|----------------|--------|"

  # Typical production scenarios (aligned):
  #
  # - SingleProducerSingleConsumer.producing (JMH avgt, ns/op) <-> Typical_SingleProducerSingleConsumer (Google Benchmark)
  # - MultiProducerSingleConsumer.producing (JMH thrpt, ops/ms) <-> Typical_MultiProducerSingleConsumer (Google Benchmark, 4 producer benchmark threads; consumer is background)
  # - MultiProducerSingleConsumer.producingBatch (JMH thrpt, ops/ms, batch=100) <-> Typical_MultiProducerSingleConsumerBatch (same measurement scope as above)

  # C++ Google Benchmark output is aggregate entries:
  #   run_name = base benchmark name
  #   aggregate_name = mean/median/stddev/cv
  # For avgt-style benchmarks we convert mean ns/op to ops/s.
  cpp_mean_ops_per_sec_from_ns() {
    # Usage: cpp_mean_ops_per_sec_from_ns <run_name> [threads]
    local run_name="$1"
    local threads="${2:-}"
    if [ -n "$threads" ]; then
      jq -r --arg n "$run_name" --argjson t "$threads" '
        .benchmarks[]
        | select(.run_type=="aggregate" and .run_name==$n and .aggregate_name=="mean" and .threads==$t)
        | (if .time_unit=="ns" then (1000000000.0 / .real_time)
           elif .time_unit=="us" then (1000000.0 / .real_time)
           elif .time_unit=="ms" then (1000.0 / .real_time)
           elif .time_unit=="s"  then (1.0 / .real_time)
           else empty end)
      ' "$CPP_FILE" 2>/dev/null | head -1
    else
      jq -r --arg n "$run_name" '
        .benchmarks[]
        | select(.run_type=="aggregate" and .run_name==$n and .aggregate_name=="mean")
        | (if .time_unit=="ns" then (1000000000.0 / .real_time)
           elif .time_unit=="us" then (1000000.0 / .real_time)
           elif .time_unit=="ms" then (1000.0 / .real_time)
           elif .time_unit=="s"  then (1.0 / .real_time)
           else empty end)
      ' "$CPP_FILE" 2>/dev/null | head -1
    fi
  }

  cpp_mean_items_per_second() {
    # Usage: cpp_mean_items_per_second <run_name> [threads]
    # Prefer items_per_second if present (Google Benchmark emits it when SetItemsProcessed is used).
    local run_name="$1"
    local threads="${2:-}"
    if [ -n "$threads" ]; then
      jq -r --arg n "$run_name" --argjson t "$threads" '
        .benchmarks[]
        | select(.run_type=="aggregate" and .run_name==$n and .aggregate_name=="mean" and .threads==$t)
        | ( .items_per_second? // .counters.items_per_second? // empty )
      ' "$CPP_FILE" 2>/dev/null | head -1
    else
      jq -r --arg n "$run_name" '
        .benchmarks[]
        | select(.run_type=="aggregate" and .run_name==$n and .aggregate_name=="mean")
        | ( .items_per_second? // .counters.items_per_second? // empty )
      ' "$CPP_FILE" 2>/dev/null | head -1
    fi
  }

  CPP_1P1C=$(cpp_mean_ops_per_sec_from_ns "JMH_SingleProducerSingleConsumer_producing")
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

  # TSF4G-tbus-inspired baseline (C++ only) vs Java Disruptor SPSC.
  CPP_TBUS_SPSC=$(cpp_mean_ops_per_sec_from_ns "JMH_TBusSingleProducerSingleConsumer_producing")
  if [ ! -z "$CPP_TBUS_SPSC" ] && [ ! -z "$JAVA_1P1C" ]; then
    RATIO=$(awk_ratio "$CPP_TBUS_SPSC" "$JAVA_1P1C")
    WINNER=$(winner_from_ratio "$RATIO" 2>/dev/null || true)
    printf "| 1P1C publish (TSF4G tbus-style, SPSC) | %.2e ops/s | %.2e ops/s | %.2fx | **%s** |\n" "$CPP_TBUS_SPSC" "$JAVA_1P1C" "$RATIO" "$WINNER"
  fi

  # C++ MP1C producing is avgt ns/op (with Threads=4); convert to ops/s.
  CPP_MP1C=$(cpp_mean_ops_per_sec_from_ns "JMH_MultiProducerSingleConsumer_producing" 4)
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

  CPP_MP1C_BATCH=$(cpp_mean_items_per_second "JMH_MultiProducerSingleConsumer_producingBatch" 4)
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

# Python fallback when jq is not available (common on Windows Git Bash).
if [ -f "$CPP_FILE" ] && [ -f "$JAVA_FILE" ] && [ "$HAVE_JQ" -eq 0 ]; then
  python - "$CPP_FILE" "$JAVA_FILE" <<'PY'
import json, sys, math
cpp_path, java_path = sys.argv[1], sys.argv[2]
cpp = json.load(open(cpp_path, "r", encoding="utf-8"))
java = json.load(open(java_path, "r", encoding="utf-8"))

def java_to_ops_per_sec(entry):
    s = float(entry["primaryMetric"]["score"])
    u = entry["primaryMetric"]["scoreUnit"]
    if u == "ns/op": return 1e9 / s
    if u == "us/op": return 1e6 / s
    if u == "ms/op": return 1e3 / s
    if u == "ops/us": return s * 1e6
    if u == "ops/ms": return s * 1e3
    if u == "ops/s": return s
    return None

def find_java(endswith):
    for e in java:
        if e.get("benchmark","").endswith(endswith):
            return java_to_ops_per_sec(e)
    return None

def cpp_mean_ns(run_name, threads=None):
    for b in cpp.get("benchmarks", []):
        if b.get("run_type") != "aggregate": continue
        if b.get("run_name") != run_name: continue
        if b.get("aggregate_name") != "mean": continue
        if threads is not None and int(b.get("threads",0)) != int(threads): continue
        t = float(b.get("real_time", 0.0))
        unit = b.get("time_unit","ns")
        if unit == "ns": return t
        if unit == "us": return t * 1e3
        if unit == "ms": return t * 1e6
        if unit == "s":  return t * 1e9
    return None

def cpp_mean_ops_per_sec(run_name, threads=None):
    ns = cpp_mean_ns(run_name, threads)
    if ns is None or ns <= 0: return None
    return 1e9 / ns

def cpp_mean_items_per_sec(run_name, threads=None):
    for b in cpp.get("benchmarks", []):
        if b.get("run_type") != "aggregate": continue
        if b.get("run_name") != run_name: continue
        if b.get("aggregate_name") != "mean": continue
        if threads is not None and int(b.get("threads",0)) != int(threads): continue
        if "items_per_second" in b and b["items_per_second"] is not None:
            return float(b["items_per_second"])
        counters = b.get("counters") or {}
        v = counters.get("items_per_second")
        if v is not None: return float(v)
    return None

def ratio(cpp_v, java_v):
    if not cpp_v or not java_v: return None
    return cpp_v / java_v

def winner(r):
    if r is None: return ""
    return "C++" if r > 1.0 else "Java"

print("## Performance Analysis & Comparison\n")
print("### Key Performance Metrics\n")
print("| Operation | C++ (Disruptor-CPP) | Java (Disruptor) | C++/Java Ratio | Winner |")
print("|-----------|-------------------|------------------|----------------|--------|")

java_spsc = find_java("SingleProducerSingleConsumer.producing")
cpp_spsc = cpp_mean_ops_per_sec("JMH_SingleProducerSingleConsumer_producing")
if cpp_spsc and java_spsc:
    r = ratio(cpp_spsc, java_spsc)
    print(f"| 1P1C publish (ring=1<<20) | {cpp_spsc:.2e} ops/s | {java_spsc:.2e} ops/s | {r:.2f}x | **{winner(r)}** |")

cpp_tbus = cpp_mean_ops_per_sec("JMH_TBusSingleProducerSingleConsumer_producing")
if cpp_tbus and java_spsc:
    r = ratio(cpp_tbus, java_spsc)
    print(f"| 1P1C publish (TSF4G tbus-style, SPSC) | {cpp_tbus:.2e} ops/s | {java_spsc:.2e} ops/s | {r:.2f}x | **{winner(r)}** |")

java_mp1c = find_java("MultiProducerSingleConsumer.producing")
cpp_mp1c = cpp_mean_ops_per_sec("JMH_MultiProducerSingleConsumer_producing", threads=4)
if cpp_mp1c and java_mp1c:
    r = ratio(cpp_mp1c, java_mp1c)
    print(f"| MP1C publish (P=4, ring=1<<22) | {cpp_mp1c:.2e} ops/s | {java_mp1c:.2e} ops/s | {r:.2f}x | **{winner(r)}** |")

java_mp1c_b = find_java("MultiProducerSingleConsumer.producingBatch")
cpp_mp1c_b = cpp_mean_items_per_sec("JMH_MultiProducerSingleConsumer_producingBatch", threads=4)
if cpp_mp1c_b and java_mp1c_b:
    r = ratio(cpp_mp1c_b, java_mp1c_b)
    print(f"| MP1C batch publish (P=4, batch=100) | {cpp_mp1c_b:.2e} ops/s | {java_mp1c_b:.2e} ops/s | {r:.2f}x | **{winner(r)}** |")

print("\n### Analysis Notes\n")
print("- **Scope**: These comparisons are limited to aligned scenarios (SPSC, MP1C, MP1C batch).")
print("- **Units**: Java avgt (ns/op) is converted to ops/s; Java thrpt is converted to ops/s.")
print("- **Ratio**: >1 means C++ higher ops/s; <1 means Java higher ops/s.\n")
PY
fi

echo "## Summary"
echo ""
echo "- This report intentionally focuses on the minimum comparable set."

