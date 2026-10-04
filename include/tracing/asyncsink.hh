#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <exception>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>

#include "tracing/sink.hh"

namespace EasyLocal
{

namespace Trace
{

enum class OverflowPolicy
{
  Block,
  DropNewest
};

class AsyncSink : public Sink
{
public:
  explicit AsyncSink(SinkPtr sink, std::size_t capacity = 4096,
                     OverflowPolicy overflow_policy = OverflowPolicy::Block)
    : sink(std::move(sink)), capacity(capacity), overflow_policy(overflow_policy)
  {
    if (!this->sink)
      throw std::invalid_argument("AsyncSink requires a sink");
    if (capacity == 0)
      throw std::invalid_argument("AsyncSink capacity must be greater than zero");
    worker = std::thread(&AsyncSink::Consume, this);
  }

  ~AsyncSink() override
  {
    Shutdown();
  }

  AsyncSink(const AsyncSink &) = delete;
  AsyncSink &operator=(const AsyncSink &) = delete;

  bool Accepts(Event event) const override
  {
    return sink->Accepts(event);
  }

  void Write(Record record) override
  {
    std::unique_lock<std::mutex> lock(mutex);
    RethrowFailure();
    if (stopping)
      throw std::logic_error("Cannot write to a stopped AsyncSink");

    if (overflow_policy == OverflowPolicy::Block)
    {
      space_available.wait(lock, [&] { return queue.size() < capacity || stopping || failure; });
      RethrowFailure();
      if (stopping)
        throw std::logic_error("Cannot write to a stopped AsyncSink");
    }
    else if (queue.size() >= capacity)
    {
      dropped.fetch_add(1, std::memory_order_relaxed);
      return;
    }

    queue.push_back(std::move(record));
    records_available.notify_one();
  }

  void Flush()
  {
    std::unique_lock<std::mutex> lock(mutex);
    drained.wait(lock, [&] { return (queue.empty() && !writing) || failure; });
    RethrowFailure();
  }

  std::uint64_t Dropped() const
  {
    return dropped.load(std::memory_order_relaxed);
  }

private:
  void Consume()
  {
    while (true)
    {
      Record record;
      {
        std::unique_lock<std::mutex> lock(mutex);
        records_available.wait(lock, [&] { return !queue.empty() || stopping; });
        if (queue.empty() && stopping)
          break;

        record = std::move(queue.front());
        queue.pop_front();
        writing = true;
        space_available.notify_one();
      }

      try
      {
        sink->Write(std::move(record));
      }
      catch (...)
      {
        std::lock_guard<std::mutex> lock(mutex);
        failure = std::current_exception();
        queue.clear();
        writing = false;
        stopping = true;
        space_available.notify_all();
        drained.notify_all();
        return;
      }

      {
        std::lock_guard<std::mutex> lock(mutex);
        writing = false;
        if (queue.empty())
          drained.notify_all();
      }
    }

    std::lock_guard<std::mutex> lock(mutex);
    drained.notify_all();
  }

  void Shutdown() noexcept
  {
    {
      std::lock_guard<std::mutex> lock(mutex);
      stopping = true;
    }
    records_available.notify_all();
    space_available.notify_all();
    if (worker.joinable())
      worker.join();
  }

  void RethrowFailure() const
  {
    if (failure)
      std::rethrow_exception(failure);
  }

  SinkPtr sink;
  const std::size_t capacity;
  const OverflowPolicy overflow_policy;
  std::deque<Record> queue;
  mutable std::mutex mutex;
  std::condition_variable records_available;
  std::condition_variable space_available;
  std::condition_variable drained;
  std::thread worker;
  std::atomic<std::uint64_t> dropped{0};
  std::exception_ptr failure;
  bool writing = false;
  bool stopping = false;
};

} // namespace Trace
} // namespace EasyLocal