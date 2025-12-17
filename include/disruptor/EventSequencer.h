#pragma once
// 1:1 port of com.lmax.disruptor.EventSequencer
// Source: reference/disruptor/src/main/java/com/lmax/disruptor/EventSequencer.java

#include "DataProvider.h"
#include "Sequenced.h"

namespace disruptor {

template <typename T>
class EventSequencer : public DataProvider<T>, public Sequenced {
public:
  ~EventSequencer() override = default;
};

} // namespace disruptor
