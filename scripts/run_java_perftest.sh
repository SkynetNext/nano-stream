#!/bin/bash
# Run Java perf test: OneToOneSequencedThroughputTest

# Get script directory and navigate to project root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
DISRUPTOR_DIR="$PROJECT_ROOT/reference/disruptor"
cd "$DISRUPTOR_DIR" || exit 1

# Build perftest classes
./gradlew perftestClasses

# Construct classpath using absolute paths, only including directories that exist
CLASSPATH=""

# Add perftest classes
if [ -d "$DISRUPTOR_DIR/build/classes/java/perftest" ]; then
    CLASSPATH="$DISRUPTOR_DIR/build/classes/java/perftest"
fi

# Add perftest resources (if exists)
if [ -d "$DISRUPTOR_DIR/build/resources/perftest" ]; then
    [ -n "$CLASSPATH" ] && CLASSPATH="$CLASSPATH:"
    CLASSPATH="$CLASSPATH$DISRUPTOR_DIR/build/resources/perftest"
fi

# Add main classes
if [ -d "$DISRUPTOR_DIR/build/classes/java/main" ]; then
    [ -n "$CLASSPATH" ] && CLASSPATH="$CLASSPATH:"
    CLASSPATH="$CLASSPATH$DISRUPTOR_DIR/build/classes/java/main"
fi

# Add main resources (if exists)
if [ -d "$DISRUPTOR_DIR/build/resources/main" ]; then
    [ -n "$CLASSPATH" ] && CLASSPATH="$CLASSPATH:"
    CLASSPATH="$CLASSPATH$DISRUPTOR_DIR/build/resources/main"
fi

# Add test classes
if [ -d "$DISRUPTOR_DIR/build/classes/java/test" ]; then
    [ -n "$CLASSPATH" ] && CLASSPATH="$CLASSPATH:"
    CLASSPATH="$CLASSPATH$DISRUPTOR_DIR/build/classes/java/test"
fi

# Add test resources (if exists)
if [ -d "$DISRUPTOR_DIR/build/resources/test" ]; then
    [ -n "$CLASSPATH" ] && CLASSPATH="$CLASSPATH:"
    CLASSPATH="$CLASSPATH$DISRUPTOR_DIR/build/resources/test"
fi

# Add dependencies (if any)
if [ -d "$DISRUPTOR_DIR/build/libs" ]; then
    for jar in "$DISRUPTOR_DIR/build/libs"/*.jar; do
        if [ -f "$jar" ]; then
            [ -n "$CLASSPATH" ] && CLASSPATH="$CLASSPATH:"
            CLASSPATH="$CLASSPATH$jar"
        fi
    done
fi

# Debug: print classpath
echo "Classpath: $CLASSPATH"
echo "Looking for class: com.lmax.disruptor.sequenced.OneToOneSequencedThroughputTest"

# Verify class file exists
CLASS_FILE="$DISRUPTOR_DIR/build/classes/java/perftest/com/lmax/disruptor/sequenced/OneToOneSequencedThroughputTest.class"
if [ -f "$CLASS_FILE" ]; then
    echo "Class file found, running..."
    java -cp "$CLASSPATH" com.lmax.disruptor.sequenced.OneToOneSequencedThroughputTest
else
    echo "Error: Class file not found!"
    echo "Expected: $CLASS_FILE"
    exit 1
fi

