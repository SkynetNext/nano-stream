#pragma once
// Template-friendly SequenceBarrier (no virtual dispatch).
//
// In the typed implementation, a "barrier" is just a type that provides:
//   int64_t waitFor(int64_t sequence);
//   int64_t getCursor() const;
//   bool isAlerted() const;
//   void alert();
//   void clearAlert();
//   void checkAlert();
//
// This file no longer defines a base class. It is kept as documentation and a
// compatibility placeholder while the codebase is migrated.

#include <cstdint>

namespace disruptor {} // namespace disruptor
