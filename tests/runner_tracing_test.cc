#include <cassert>
#include <vector>

#include "runners/runner.hh"
#include "tracing/callbacksink.hh"

namespace
{

struct Input
{};

struct Solution
{
  explicit Solution(const Input &) {}
  int value = 0;
};

std::ostream &operator<<(std::ostream &stream, const Solution &solution)
{
  return stream << solution.value;
}

using Cost = EasyLocal::Core::DefaultCostStructure<int>;

class Manager : public EasyLocal::Core::SolutionManager<Input, Solution, Cost>
{
public:
  explicit Manager(const Input &input)
      : SolutionManager(input, "test_manager") {}

  void RandomState(Solution &solution) override
  {
    solution.value = 0;
  }

  Cost CostFunctionComponents(const Solution &solution, const std::vector<double> & = {}) const override
  {
    return Cost(solution.value, 0, solution.value, {solution.value});
  }

  bool LowerBoundReached(const Cost &) const override
  {
    return false;
  }

  bool CheckConsistency(const Solution &) const override
  {
    return true;
  }
};

class TestRunner : public EasyLocal::Core::Runner<Input, Solution, Cost>
{
public:
  TestRunner(const Input &input, Manager &manager)
      : Runner(input, manager, "test_runner")
  {
    SetMaxEvaluations(1);
  }

  size_t Modality() const override
  {
    return 1;
  }

private:
  void TerminateRun() override {}
  bool StopCriterion() override { return false; }

  void SelectMove() override
  {
    ++evaluations;
  }

  bool AcceptableMoveFound() override
  {
    return false;
  }

  void MakeMove() override {}
  void UpdateBestState() override {}
};

} // namespace

int main()
{
  Input input;
  Manager manager(input);
  TestRunner runner(input, manager);
  std::vector<EasyLocal::Trace::Record> records;

  runner.SetTraceOptions({EasyLocal::Trace::Event::RunStarted |
                              EasyLocal::Trace::Event::Progress |
                              EasyLocal::Trace::Event::RunFinished,
                          1});
  runner.SetTraceSink(std::make_shared<EasyLocal::Trace::CallbackSink>(
      [&](const EasyLocal::Trace::Record &record) { records.push_back(record); }));

  Solution solution(input);
  solution.value = 10;
  runner.Go(solution);

  assert(records.size() == 3);
  assert(records[0].event == EasyLocal::Trace::Event::RunStarted);
  assert(records[0].data["random_seed"] == EasyLocal::Core::Random::GetSeed());
  assert(records[0].data["parameters_complete"] == true);
  assert(records[0].data["current_cost"]["total"] == 10);

  assert(records[1].event == EasyLocal::Trace::Event::Progress);
  assert(records[1].iteration == 1);
  assert(records[1].evaluations == 1);

  assert(records[2].event == EasyLocal::Trace::Event::RunFinished);
  assert(records[2].data["stop_reason"] == "max_evaluations");
}