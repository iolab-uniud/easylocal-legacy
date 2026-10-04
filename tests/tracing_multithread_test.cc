#include <algorithm>
#include <atomic>
#include <cassert>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

#include "tracing.hh"

namespace
{

class FailingSink : public EasyLocal::Trace::Sink
{
public:
  void Write(EasyLocal::Trace::Record) override
  {
    throw std::runtime_error("backend failed");
  }
};

} // namespace

int main()
{
  using namespace EasyLocal::Trace;

  constexpr int producers = 8;
  constexpr int records_per_producer = 500;
  std::mutex sequences_mutex;
  std::vector<std::uint64_t> sequences;
  sequences.reserve(producers * records_per_producer);

  auto collector = std::make_shared<CallbackSink>([&](const Record &record) {
    std::lock_guard<std::mutex> lock(sequences_mutex);
    sequences.push_back(record.sequence);
  });
  auto async = std::make_shared<AsyncSink>(collector, 64, OverflowPolicy::Block);

  Channel channel;
  channel.SetOptions({Mask(Event::All), 1});
  channel.SetSink(async);
  channel.BeginRun();

  std::vector<std::thread> threads;
  for (int producer = 0; producer < producers; ++producer)
  {
    threads.emplace_back([&, producer] {
      for (int index = 0; index < records_per_producer; ++index)
      {
        channel.Emit(Event::Progress, "parallel_runner", index, index,
                     [&](EventBuilder &event) { event.Field("producer", producer); });
      }
    });
  }
  for (std::thread &thread : threads)
    thread.join();
  async->Flush();

  assert(sequences.size() == producers * records_per_producer);
  std::sort(sequences.begin(), sequences.end());
  for (std::size_t index = 0; index < sequences.size(); ++index)
    assert(sequences[index] == index);
  assert(async->Dropped() == 0);

  std::atomic<int> dynamically_collected{0};
  std::atomic<int> invalid_run_ids{0};
  auto dynamic_collector = std::make_shared<CallbackSink>([&](const Record &record) {
    dynamically_collected.fetch_add(1, std::memory_order_relaxed);
    if (record.run_id.empty())
      invalid_run_ids.fetch_add(1, std::memory_order_relaxed);
  });
  Channel dynamic_channel;
  dynamic_channel.SetSink(dynamic_collector);
  dynamic_channel.BeginRun();

  std::thread controller([&] {
    for (int index = 0; index < 500; ++index)
    {
      dynamic_channel.SetOptions({Mask(Event::All), static_cast<std::uint64_t>(index % 3 + 1)});
      if (index % 10 == 0)
      {
        dynamic_channel.ClearSink();
        dynamic_channel.SetSink(dynamic_collector);
        dynamic_channel.BeginRun();
      }
    }
  });
  threads.clear();
  for (int producer = 0; producer < 4; ++producer)
  {
    threads.emplace_back([&] {
      for (int index = 0; index < 1000; ++index)
        dynamic_channel.Emit(Event::Progress, "dynamic_runner", index, index);
    });
  }
  controller.join();
  for (std::thread &thread : threads)
    thread.join();

  assert(dynamically_collected.load(std::memory_order_relaxed) > 0);
  assert(invalid_run_ids.load(std::memory_order_relaxed) == 0);

  std::mutex gate_mutex;
  std::condition_variable gate;
  bool worker_entered = false;
  bool release_worker = false;
  auto blocked_collector = std::make_shared<CallbackSink>([&](const Record &) {
    std::unique_lock<std::mutex> lock(gate_mutex);
    worker_entered = true;
    gate.notify_all();
    gate.wait(lock, [&] { return release_worker; });
  });
  AsyncSink dropping(blocked_collector, 1, OverflowPolicy::DropNewest);

  dropping.Write(Record{});
  {
    std::unique_lock<std::mutex> lock(gate_mutex);
    gate.wait(lock, [&] { return worker_entered; });
  }
  dropping.Write(Record{});
  dropping.Write(Record{});
  assert(dropping.Dropped() == 1);
  {
    std::lock_guard<std::mutex> lock(gate_mutex);
    release_worker = true;
  }
  gate.notify_all();
  dropping.Flush();

  AsyncSink failing(std::make_shared<FailingSink>(), 1);
  failing.Write(Record{});
  bool failure_observed = false;
  try
  {
    failing.Flush();
  }
  catch (const std::runtime_error &)
  {
    failure_observed = true;
  }
  assert(failure_observed);
}