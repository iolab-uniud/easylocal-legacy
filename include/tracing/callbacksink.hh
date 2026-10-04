#pragma once

#include <functional>
#include <utility>

#include "tracing/sink.hh"

namespace EasyLocal
{

namespace Trace
{

class CallbackSink : public Sink
{
public:
  using Callback = std::function<void(const Record &)>;
  using Filter = std::function<bool(Event)>;

  explicit CallbackSink(Callback callback, Filter filter = {})
      : callback(std::move(callback)), filter(std::move(filter)) {}

  bool Accepts(Event event) const override
  {
    return !filter || filter(event);
  }

  void Write(Record record) override
  {
    callback(record);
  }

private:
  Callback callback;
  Filter filter;
};

} // namespace Trace
} // namespace EasyLocal