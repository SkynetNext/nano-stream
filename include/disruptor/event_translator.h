#pragma once

#include <cstdint>
#include <functional>
#include <utility>

namespace disruptor {

/**
 * Event translator interface for updating events in the ring buffer.
 * Inspired by LMAX Disruptor's EventTranslator.
 */
template <typename T> class EventTranslator {
public:
  virtual ~EventTranslator() = default;

  /**
   * Translate data into the given event.
   *
   * @param event The event to update
   * @param sequence The sequence number assigned to the event
   */
  virtual void translate_to(T &event, int64_t sequence) = 0;
};

/**
 * Event translator with one argument.
 */
template <typename T, typename A> class EventTranslatorOneArg {
public:
  virtual ~EventTranslatorOneArg() = default;

  /**
   * Translate data into the given event with one argument.
   *
   * @param event The event to update
   * @param sequence The sequence number assigned to the event
   * @param arg0 The first argument
   */
  virtual void translate_to(T &event, int64_t sequence, const A &arg0) = 0;
};

/**
 * Event translator with two arguments.
 */
template <typename T, typename A, typename B> class EventTranslatorTwoArg {
public:
  virtual ~EventTranslatorTwoArg() = default;

  /**
   * Translate data into the given event with two arguments.
   *
   * @param event The event to update
   * @param sequence The sequence number assigned to the event
   * @param arg0 The first argument
   * @param arg1 The second argument
   */
  virtual void translate_to(T &event, int64_t sequence, const A &arg0,
                            const B &arg1) = 0;
};

/**
 * Event translator with three arguments.
 */
template <typename T, typename A, typename B, typename C>
class EventTranslatorThreeArg {
public:
  virtual ~EventTranslatorThreeArg() = default;

  /**
   * Translate data into the given event with three arguments.
   *
   * @param event The event to update
   * @param sequence The sequence number assigned to the event
   * @param arg0 The first argument
   * @param arg1 The second argument
   * @param arg2 The third argument
   */
  virtual void translate_to(T &event, int64_t sequence, const A &arg0,
                            const B &arg1, const C &arg2) = 0;
};

/**
 * Lambda-based event translator for convenience.
 */
template <typename T> class LambdaEventTranslator : public EventTranslator<T> {
private:
  std::function<void(T &, int64_t)> translator_fn_;

public:
  explicit LambdaEventTranslator(
      std::function<void(T &, int64_t)> translator_fn)
      : translator_fn_(std::move(translator_fn)) {}

  void translate_to(T &event, int64_t sequence) override {
    translator_fn_(event, sequence);
  }
};

/**
 * Lambda-based event translator with one argument.
 */
template <typename T, typename A>
class LambdaEventTranslatorOneArg : public EventTranslatorOneArg<T, A> {
private:
  std::function<void(T &, int64_t, const A &)> translator_fn_;

public:
  explicit LambdaEventTranslatorOneArg(
      std::function<void(T &, int64_t, const A &)> translator_fn)
      : translator_fn_(std::move(translator_fn)) {}

  void translate_to(T &event, int64_t sequence, const A &arg0) override {
    translator_fn_(event, sequence, arg0);
  }
};

/**
 * Lambda-based event translator with two arguments.
 */
template <typename T, typename A, typename B>
class LambdaEventTranslatorTwoArg : public EventTranslatorTwoArg<T, A, B> {
private:
  std::function<void(T &, int64_t, const A &, const B &)> translator_fn_;

public:
  explicit LambdaEventTranslatorTwoArg(
      std::function<void(T &, int64_t, const A &, const B &)> translator_fn)
      : translator_fn_(std::move(translator_fn)) {}

  void translate_to(T &event, int64_t sequence, const A &arg0,
                    const B &arg1) override {
    translator_fn_(event, sequence, arg0, arg1);
  }
};

/**
 * Lambda-based event translator with three arguments.
 */
template <typename T, typename A, typename B, typename C>
class LambdaEventTranslatorThreeArg
    : public EventTranslatorThreeArg<T, A, B, C> {
private:
  std::function<void(T &, int64_t, const A &, const B &, const C &)>
      translator_fn_;

public:
  explicit LambdaEventTranslatorThreeArg(
      std::function<void(T &, int64_t, const A &, const B &, const C &)>
          translator_fn)
      : translator_fn_(std::move(translator_fn)) {}

  void translate_to(T &event, int64_t sequence, const A &arg0, const B &arg1,
                    const C &arg2) override {
    translator_fn_(event, sequence, arg0, arg1, arg2);
  }
};

} // namespace disruptor
