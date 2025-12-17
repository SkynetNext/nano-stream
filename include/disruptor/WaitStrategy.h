#pragma once
// Template-friendly WaitStrategy (no virtual dispatch).
//
// In the Java reference, WaitStrategy is an interface and the JVM typically
// devirtualizes + inlines implementations (especially BusySpinWaitStrategy).
// In C++, we eliminate virtual dispatch by treating WaitStrategy as a concept:
//
// Required API for a WaitStrategy type `WS`:
//   int64_t WS::waitFor(int64_t sequence,
//                       const Sequence& cursor,
//                       const Sequence& dependentSequence,
//                       Barrier& barrier);
//   void WS::signalAllWhenBlocking();
//   static constexpr bool WS::kIsBlockingStrategy;

#include <cstdint>

namespace disruptor {

class Sequence;
class SequenceBarrier; // Legacy type; will be removed after full template
                       // migration.

// This file intentionally does NOT define a base class.

} // namespace disruptor
