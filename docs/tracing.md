# Structured tracing

Runners can emit machine-readable search events to an optional trace sink. No
event data is collected when no sink is attached.

```cpp
auto sink = std::make_shared<EasyLocal::Trace::NdjsonSink>("search.ndjson");
runner.SetTraceSink(sink);
runner.SetTraceOptions({
    EasyLocal::Trace::Event::RunStarted |
        EasyLocal::Trace::Event::RunFinished |
        EasyLocal::Trace::Event::BestUpdated |
        EasyLocal::Trace::Event::Progress |
        EasyLocal::Trace::Event::TemperatureChanged,
    100
});
```

The output contains one JSON object per line. `OStreamSink`, `CallbackSink`,
and `CompositeSink` support streams, external adapters, and fan-out.
Changing sinks and options concurrently with an active run is safe; each event
uses a consistent snapshot. Applications should still synchronize logical run
boundaries so events are assigned to the intended `run_id`.

## Multithreaded tracing

`Channel` supports concurrent producers and assigns a unique sequence number to
every emitted record. Built-in file and stream sinks serialize concurrent
writes with a mutex. For local-search workers, wrap the destination in an
`AsyncSink` so serialization and I/O run on a dedicated worker. The event
snapshot and custom encoders still run on the producer thread:

```cpp
auto file = std::make_shared<EasyLocal::Trace::NdjsonSink>("search.ndjson");
auto async = std::make_shared<EasyLocal::Trace::AsyncSink>(
    file,
    8192,
    EasyLocal::Trace::OverflowPolicy::Block);

runner.SetTraceSink(async);
auto result = solver.Solve();
async->Flush();
```

`Block` preserves every event and applies backpressure when the queue is full.
`DropNewest` keeps search workers moving at the cost of incomplete traces;
`async->Dropped()` reports the number of discarded records. Backend exceptions
are rethrown by subsequent writes and by `Flush()`.

With concurrent producers, physical NDJSON line order is not guaranteed to
match sequence order. Consumers can reconstruct it by sorting on `(run_id,
seq)`.

`CallbackSink` may be invoked concurrently when used directly. Wrap it in an
`AsyncSink` when the callback is not thread-safe or should run outside search
threads.

The core events carry the following information:

- `run_started`: configured runner parameters, random seed, modality and initial
    costs;
- `progress`: current and best costs, sampled every `every_n_iterations`;
- `move_applied`: move delta, acceptance classification and optionally the move;
- `best_updated`: previous best, new best and their delta;
- `run_finished`: final costs and one of `stop_criterion`, `max_evaluations`,
    `lower_bound`, `timeout`, `interrupted`, or `empty_neighborhood`;
- `temperature_changed`: old/new temperature and sampling/acceptance counters.

Moves are serialized only when an encoder is configured:

```cpp
runner.SetTraceMoveEncoder([](const Move &move) {
        return nlohmann::json{{"from", move.from}, {"to", move.to}};
});
```

There is deliberately no generic `move_rejected` event: rejection semantics are
algorithm-specific and may represent an invalid move, a tabu move, a Metropolis
decision, or the absence of a selectable candidate.

Solution serialization is opt-in:

```cpp
runner.SetTraceSolutionEncoder([](const Solution &solution) {
    return nlohmann::json{{"values", solution.values}};
});

EasyLocal::Trace::Options options;
options.capture_solution = EasyLocal::Trace::CaptureSolution::Best;
runner.SetTraceOptions(options);
```

Defining `EASYLOCAL_DISABLE_TRACING` removes event collection at compile time.