#include <cassert>
#include <sstream>
#include <string>

#include "tracing.hh"

int main()
{
  using namespace EasyLocal::Trace;

  Channel channel;
  bool collected = false;
  channel.Emit(Event::RunStarted, "runner", 0, 0, [&](EventBuilder &) { collected = true; });
  assert(!collected);

  std::ostringstream output;
  int callback_count = 0;
  auto stream_sink = std::make_shared<OStreamSink>(output);
  auto callback_sink = std::make_shared<CallbackSink>([&](const Record &) { ++callback_count; });
  channel.SetSink(std::make_shared<CompositeSink>(
      std::initializer_list<SinkPtr>{stream_sink, callback_sink}));
  channel.SetOptions({Event::RunStarted | Event::Progress | Event::RunFinished, 2});
  channel.BeginRun();

  channel.Emit(Event::RunStarted, "runner", 0, 0,
               [](EventBuilder &event) { event.Field("random_seed", 42); });
  channel.Emit(Event::Progress, "runner", 1, 3);
  channel.Emit(Event::Progress, "runner", 2, 6,
               [](EventBuilder &event) { event.Field("current_cost", 125); });
  channel.Emit(Event::BestUpdated, "runner", 2, 6);
  channel.Emit(Event::RunFinished, "runner", 2, 6, [](EventBuilder &event) {
    event.Field("stop_reason", Name(StopReason::MaxEvaluations));
  });

  assert(callback_count == 3);

  std::istringstream lines(output.str());
  std::string line;
  std::getline(lines, line);
  const nlohmann::json started = nlohmann::json::parse(line);
  assert(started["schema"] == "easylocal.trace/v1");
  assert(started["event"] == "run_started");
  assert(started["data"]["random_seed"] == 42);

  std::getline(lines, line);
  const nlohmann::json progress = nlohmann::json::parse(line);
  assert(progress["event"] == "progress");
  assert(progress["iteration"] == 2);
  assert(progress["data"]["current_cost"] == 125);

  std::getline(lines, line);
  const nlohmann::json finished = nlohmann::json::parse(line);
  assert(finished["event"] == "run_finished");
  assert(finished["data"]["stop_reason"] == "max_evaluations");
  assert(!std::getline(lines, line));
}