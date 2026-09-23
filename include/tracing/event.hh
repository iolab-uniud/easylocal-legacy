#pragma once

#include <cstdint>
#include <string>
#include <utility>

#include "utils/json.hpp"

namespace EasyLocal
{

namespace Trace
{

enum class Event : std::uint32_t
{
  None = 0,
  RunStarted = 1u << 0,
  RunFinished = 1u << 1,
  Progress = 1u << 2,
  MoveApplied = 1u << 3,
  BestUpdated = 1u << 4,
  TemperatureChanged = 1u << 5,
  Reheat = 1u << 6,
  All = (1u << 7) - 1
};

enum class StopReason
{
  StopCriterion,
  MaxEvaluations,
  LowerBound,
  Timeout,
  Interrupted,
  EmptyNeighborhood
};

using EventMask = std::uint32_t;

constexpr EventMask Mask(Event event)
{
  return static_cast<EventMask>(event);
}

constexpr EventMask operator|(Event lhs, Event rhs)
{
  return Mask(lhs) | Mask(rhs);
}

constexpr EventMask operator|(EventMask lhs, Event rhs)
{
  return lhs | Mask(rhs);
}

constexpr bool Contains(EventMask mask, Event event)
{
  return (mask & Mask(event)) != 0;
}

inline const char *Name(Event event)
{
  switch (event)
  {
  case Event::RunStarted:
    return "run_started";
  case Event::RunFinished:
    return "run_finished";
  case Event::Progress:
    return "progress";
  case Event::MoveApplied:
    return "move_applied";
  case Event::BestUpdated:
    return "best_updated";
  case Event::TemperatureChanged:
    return "temperature_changed";
  case Event::Reheat:
    return "reheat";
  case Event::None:
    return "none";
  case Event::All:
    return "all";
  }
  return "unknown";
}

inline const char *Name(StopReason reason)
{
  switch (reason)
  {
  case StopReason::StopCriterion:
    return "stop_criterion";
  case StopReason::MaxEvaluations:
    return "max_evaluations";
  case StopReason::LowerBound:
    return "lower_bound";
  case StopReason::Timeout:
    return "timeout";
  case StopReason::Interrupted:
    return "interrupted";
  case StopReason::EmptyNeighborhood:
    return "empty_neighborhood";
  }
  return "unknown";
}

struct Record
{
  Event event = Event::None;
  std::string run_id;
  std::string runner;
  std::uint64_t sequence = 0;
  std::uint64_t elapsed_ns = 0;
  std::uint64_t iteration = 0;
  std::uint64_t evaluations = 0;
  nlohmann::json data = nlohmann::json::object();
};

class EventBuilder
{
public:
  explicit EventBuilder(Record &record) : record(record) {}

  template <typename Value>
  EventBuilder &Field(std::string name, Value &&value)
  {
    record.data[std::move(name)] = std::forward<Value>(value);
    return *this;
  }

private:
  Record &record;
};

} // namespace Trace
} // namespace EasyLocal