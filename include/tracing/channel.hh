#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "tracing/sink.hh"

namespace EasyLocal
{

namespace Trace
{

enum class CaptureSolution
{
  None,
  Best,
  All
};

struct Options
{
  EventMask events = Event::RunStarted | Event::RunFinished | Event::BestUpdated;
  std::uint64_t every_n_iterations = 1;
  bool capture_costs = true;
  CaptureSolution capture_solution = CaptureSolution::None;
};

class Channel
{
public:
  void SetSink(SinkPtr new_sink)
  {
    std::atomic_store_explicit(&sink, std::move(new_sink), std::memory_order_release);
  }

  void ClearSink()
  {
    std::atomic_store_explicit(&sink, SinkPtr{}, std::memory_order_release);
  }

  bool HasSink() const
  {
    return static_cast<bool>(std::atomic_load_explicit(&sink, std::memory_order_acquire));
  }

  void SetOptions(Options new_options)
  {
    std::atomic_store_explicit(
        &options, std::make_shared<const Options>(std::move(new_options)), std::memory_order_release);
  }

  Options GetOptions() const
  {
    return *std::atomic_load_explicit(&options, std::memory_order_acquire);
  }

  void BeginRun()
  {
#ifndef EASYLOCAL_DISABLE_TRACING
    if (!std::atomic_load_explicit(&sink, std::memory_order_acquire))
    {
      std::atomic_store_explicit(&context, std::shared_ptr<RunContext>{}, std::memory_order_release);
      return;
    }

    std::atomic_store_explicit(&context, MakeRunContext(), std::memory_order_release);
#endif
  }

  template <typename Collector>
  void Emit(Event event, const std::string &runner, std::uint64_t iteration,
            std::uint64_t evaluations, Collector &&collector)
  {
#ifndef EASYLOCAL_DISABLE_TRACING
    SinkPtr active_sink = std::atomic_load_explicit(&sink, std::memory_order_acquire);
    const std::shared_ptr<const Options> active_options =
      std::atomic_load_explicit(&options, std::memory_order_acquire);
    if (!active_sink || !ShouldEmit(*active_options, event, iteration) || !active_sink->Accepts(event))
      return;

    std::shared_ptr<RunContext> active_context =
      std::atomic_load_explicit(&context, std::memory_order_acquire);
    if (!active_context)
    {
      std::shared_ptr<RunContext> new_context = MakeRunContext();
      if (std::atomic_compare_exchange_strong_explicit(
          &context, &active_context, new_context,
          std::memory_order_acq_rel, std::memory_order_acquire))
      active_context = std::move(new_context);
    }

    Record record;
    record.event = event;
    record.run_id = active_context->run_id;
    record.runner = runner;
    record.sequence = active_context->sequence.fetch_add(1, std::memory_order_relaxed);
    record.elapsed_ns = static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - active_context->start).count());
    record.iteration = iteration;
    record.evaluations = evaluations;

    EventBuilder builder(record);
    std::forward<Collector>(collector)(builder);
    active_sink->Write(std::move(record));
#else
    (void)event;
    (void)runner;
    (void)iteration;
    (void)evaluations;
    (void)collector;
#endif
  }

  void Emit(Event event, const std::string &runner, std::uint64_t iteration,
            std::uint64_t evaluations)
  {
    Emit(event, runner, iteration, evaluations, [](EventBuilder &) {});
  }

private:
  using Clock = std::chrono::steady_clock;

  struct RunContext
  {
    RunContext(Clock::time_point start, std::string run_id)
        : start(start), run_id(std::move(run_id)) {}

    const Clock::time_point start;
    const std::string run_id;
    std::atomic<std::uint64_t> sequence{0};
  };

  static bool ShouldEmit(const Options &options, Event event, std::uint64_t iteration)
  {
    if (!Contains(options.events, event))
      return false;
    if (event != Event::Progress || options.every_n_iterations <= 1)
      return true;
    return iteration % options.every_n_iterations == 0;
  }

  static std::string MakeRunId()
  {
    static std::atomic<std::uint64_t> counter{0};
    const auto timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
                               std::chrono::system_clock::now().time_since_epoch())
                               .count();
    return std::to_string(timestamp) + "-" + std::to_string(counter.fetch_add(1, std::memory_order_relaxed));
  }

  static std::shared_ptr<RunContext> MakeRunContext()
  {
    return std::make_shared<RunContext>(Clock::now(), MakeRunId());
  }

  SinkPtr sink{};
  std::shared_ptr<const Options> options = std::make_shared<const Options>();
  std::shared_ptr<RunContext> context{};
};

} // namespace Trace
} // namespace EasyLocal