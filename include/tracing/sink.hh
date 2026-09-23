#pragma once

#include <memory>

#include "tracing/event.hh"

namespace EasyLocal
{

namespace Trace
{

class Sink
{
public:
  virtual ~Sink() = default;

  virtual bool Accepts(Event) const
  {
    return true;
  }

  virtual void Write(Record record) = 0;
};

using SinkPtr = std::shared_ptr<Sink>;

} // namespace Trace
} // namespace EasyLocal