#!/bin/bash
# Run Java perf test: OneToOneSequencedThroughputTest

# Get script directory and navigate to project root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_ROOT/reference/disruptor" || exit 1

# Build perftest classes
./gradlew perftestClasses

# Get classpath (use : for Unix-style, Git Bash handles this correctly)
CLASSPATH="build/classes/java/perftest:build/classes/java/main:build/classes/java/test"

# Add dependencies (if any)
if [ -d "build/libs" ]; then
    for jar in build/libs/*.jar; do
        CLASSPATH="$CLASSPATH:$jar"
    done
fi

# Debug: print classpath
echo "Classpath: $CLASSPATH"
echo "Looking for class: com.lmax.disruptor.sequenced.OneToOneSequencedThroughputTest"

# Verify class file exists
if [ -f "build/classes/java/perftest/com/lmax/disruptor/sequenced/OneToOneSequencedThroughputTest.class" ]; then
    echo "Class file found, running..."
    java -cp "$CLASSPATH" com.lmax.disruptor.sequenced.OneToOneSequencedThroughputTest
else
    echo "Error: Class file not found!"
    echo "Expected: build/classes/java/perftest/com/lmax/disruptor/sequenced/OneToOneSequencedThroughputTest.class"
    exit 1
fi

