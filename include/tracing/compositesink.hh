#pragma once

#include <initializer_list>
#include <mutex>
#include <utility>
#include <vector>

#include "tracing/sink.hh"

namespace EasyLocal
{

namespace Trace
{

class CompositeSink : public Sink
{
public:
  CompositeSink() = default;
  CompositeSink(std::initializer_list<SinkPtr> sinks) : sinks(sinks) {}

  void Add(SinkPtr sink)
  {
    std::lock_guard<std::mutex> lock(mutex);
    sinks.push_back(std::move(sink));
  }

  bool Accepts(Event event) const override
  {
    const std::vector<SinkPtr> active_sinks = Snapshot();
    for (const SinkPtr &sink : active_sinks)
      if (sink && sink->Accepts(event))
        return true;
    return false;
  }

  void Write(Record record) override
  {
    const std::vector<SinkPtr> active_sinks = Snapshot();
    std::size_t last_accepting = active_sinks.size();
    for (std::size_t index = active_sinks.size(); index > 0; --index)
      if (active_sinks[index - 1] && active_sinks[index - 1]->Accepts(record.event))
      {
        last_accepting = index - 1;
        break;
      }

    for (std::size_t index = 0; index < active_sinks.size(); ++index)
    {
      const SinkPtr &sink = active_sinks[index];
      if (sink && sink->Accepts(record.event))
      {
        if (index == last_accepting)
          sink->Write(std::move(record));
        else
          sink->Write(record);
      }
    }
  }

private:
  std::vector<SinkPtr> Snapshot() const
  {
    std::lock_guard<std::mutex> lock(mutex);
    return sinks;
  }

  std::vector<SinkPtr> sinks;
  mutable std::mutex mutex;
};

} // namespace Trace
} // namespace EasyLocal