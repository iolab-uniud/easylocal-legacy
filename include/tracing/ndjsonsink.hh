#pragma once

#include <fstream>
#include <mutex>
#include <ostream>
#include <stdexcept>
#include <string>

#include "tracing/sink.hh"

namespace EasyLocal
{

namespace Trace
{

inline nlohmann::json ToJson(const Record &record)
{
  nlohmann::json value = {
      {"schema", "easylocal.trace/v1"},
      {"run_id", record.run_id},
      {"seq", record.sequence},
      {"event", Name(record.event)},
      {"runner", record.runner},
      {"elapsed_ns", record.elapsed_ns},
      {"iteration", record.iteration},
      {"evaluations", record.evaluations}};
  if (!record.data.empty())
    value["data"] = record.data;
  return value;
}

class OStreamSink : public Sink
{
public:
  explicit OStreamSink(std::ostream &stream, bool flush_each_record = false)
      : stream(stream), flush_each_record(flush_each_record) {}

  void Write(Record record) override
  {
    std::lock_guard<std::mutex> lock(mutex);
    stream << ToJson(record).dump() << '\n';
    if (flush_each_record)
      stream.flush();
  }

private:
  std::ostream &stream;
  bool flush_each_record;
  std::mutex mutex;
};

class NdjsonSink : public Sink
{
public:
  explicit NdjsonSink(const std::string &path, bool append = false, bool flush_each_record = false)
      : stream(path, append ? std::ios::app : std::ios::trunc), flush_each_record(flush_each_record)
  {
    if (!stream)
      throw std::runtime_error("Cannot open trace file: " + path);
  }

  void Write(Record record) override
  {
    std::lock_guard<std::mutex> lock(mutex);
    stream << ToJson(record).dump() << '\n';
    if (flush_each_record)
      stream.flush();
  }

private:
  std::ofstream stream;
  bool flush_each_record;
  std::mutex mutex;
};

} // namespace Trace
} // namespace EasyLocal