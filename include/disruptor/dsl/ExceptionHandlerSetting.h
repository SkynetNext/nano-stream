#pragma once
// 1:1 port of com.lmax.disruptor.dsl.ExceptionHandlerSetting
// Source: reference/disruptor/src/main/java/com/lmax/disruptor/dsl/ExceptionHandlerSetting.java

#include "../BatchEventProcessor.h"
#include "../EventHandlerIdentity.h"
#include "../EventProcessor.h"
#include "../ExceptionHandler.h"
#include "ConsumerRepository.h"

#include <stdexcept>

namespace disruptor::dsl {

template <typename T, typename BarrierPtrT, typename BarrierT>
class ExceptionHandlerSetting final {
public:
  ExceptionHandlerSetting(EventHandlerIdentity& handlerIdentity,
                          ConsumerRepository<BarrierPtrT>& consumerRepository)
      : handlerIdentity_(&handlerIdentity), consumerRepository_(&consumerRepository) {}

  void with(ExceptionHandler<T>& exceptionHandler) {
    EventProcessor& eventProcessor = consumerRepository_->getEventProcessorFor(*handlerIdentity_);
    auto* batch = dynamic_cast<BatchEventProcessor<T, BarrierT>*>(&eventProcessor);
    if (batch != nullptr) {
      batch->setExceptionHandler(exceptionHandler);
      auto barrier = consumerRepository_->getBarrierFor(*handlerIdentity_);
      if (barrier) barrier->alert();
    } else {
      throw std::runtime_error("EventProcessor is not a BatchEventProcessor and does not support exception handlers");
    }
  }

private:
  EventHandlerIdentity* handlerIdentity_;
  ConsumerRepository<BarrierPtrT>* consumerRepository_;
};

} // namespace disruptor::dsl


