#pragma once

#include "runners/runner.hh"
#include "helpers/solutionmanager.hh"
#include "helpers/neighborhoodexplorer.hh"

#include <functional>

namespace EasyLocal
{

namespace Core
{

/** A Move Runner is an instance of the Runner interface which it compels to
     with a particular definition of @Move (given as template instantiation).
     It is at the root of the inheritance hierarchy of actual runners.
     @ingroup Runners
     */
template <class Input, class Solution, class Move, class CostStructure = DefaultCostStructure<int>>
class MoveRunner : public Runner<Input, Solution, CostStructure>
{
public:
  /** Modality of this runner. */
  virtual size_t Modality() const override { return ne.Modality(); }

  /** Constructor.
       @param e_sm */
  MoveRunner(const Input &in, SolutionManager<Input, Solution, CostStructure> &e_sm,
             NeighborhoodExplorer<Input, Solution, Move, CostStructure> &e_ne,
             std::string name) : Runner<Input, Solution, CostStructure>(in, e_sm, name), ne(e_ne)
    {}

  void SetTraceMoveEncoder(std::function<nlohmann::json(const Move &)> encoder)
  {
    trace_move_encoder = std::move(encoder);
  }

protected:
  virtual void TerminateRun() override;

  virtual void InitializeRun() override;

  virtual bool AcceptableMoveFound() override;

  /** Actions to be perfomed at the beginning of the run. */

  /** Encodes the criterion used to select the move at each step. */
  virtual void MakeMove() override;

  void TraceAppliedMove() override;

  void UpdateBestState() override final;
  void UpdateStateCost();

  NeighborhoodExplorer<Input, Solution, Move, CostStructure> &ne; /**< A reference to the
                                                             attached neighborhood
                                                             explorer. */

  // data
  EvaluatedMove<Move, CostStructure> current_move; /**< The currently selected move. */

private:
  std::function<nlohmann::json(const Move &)> trace_move_encoder;
};

/*************************************************************************
     * Implementation
     *************************************************************************/

template <class Input, class Solution, class Move, class CostStructure>
void MoveRunner<Input, Solution, Move, CostStructure>::UpdateBestState()
{
  if (LessThan(this->current_state_cost.violations, this->best_state_cost.violations) || (EqualTo(this->current_state_cost.violations, this->best_state_cost.violations) &&
                                                                                          (LessThan(this->current_state_cost.total, this->best_state_cost.total))))
  {
    const CostStructure previous_best_cost = this->best_state_cost;
    {
      std::lock_guard<std::mutex> lock(this->best_state_mutex);
      *(this->p_best_state) = *(this->p_current_state);
      this->best_state_cost = this->current_state_cost;

      // so that idle iterations are printed correctly
      this->iteration_of_best = this->iteration;
    }
    this->TraceEvent(Trace::Event::BestUpdated, [&](Trace::EventBuilder &event) {
      event.Field("previous_best_cost", this->TraceCost(previous_best_cost));
      event.Field("best_delta", this->TraceCost(this->best_state_cost - previous_best_cost));
    });
    // FIXME: write out cost
#if VERBOSE >= 1 
    std::cerr << "V1" << " new best: " << this->best_state_cost << std::endl;
    if(this->best_state_cost.violations < 0)
    {
      std::cerr << *(this->p_best_state) << std::endl;
      char ch; std::cin >> ch;
    }
      
#endif
  }
}

template <class Input, class Solution, class Move, class CostStructure>
void MoveRunner<Input, Solution, Move, CostStructure>::InitializeRun()
{}

template <class Input, class Solution, class Move, class CostStructure>
void MoveRunner<Input, Solution, Move, CostStructure>::TerminateRun()
{}

template <class Input, class Solution, class Move, class CostStructure>
bool MoveRunner<Input, Solution, Move, CostStructure>::AcceptableMoveFound()
{
  this->no_acceptable_move_found = !this->current_move.is_valid;
  return this->current_move.is_valid;
}

/**
     Actually performs the move selected by the local search strategy.
     */
template <class Input, class Solution, class Move, class CostStructure>
void MoveRunner<Input, Solution, Move, CostStructure>::MakeMove()
{
  if (current_move.is_valid)
  {
    ne.MakeMove(*this->p_current_state, current_move.move);
    this->current_state_cost += current_move.cost;
  }
}

template <class Input, class Solution, class Move, class CostStructure>
void MoveRunner<Input, Solution, Move, CostStructure>::TraceAppliedMove()
{
  this->TraceEvent(Trace::Event::MoveApplied, [&](Trace::EventBuilder &event) {
    event.Field("delta_cost", this->TraceCost(current_move.cost));
    event.Field("acceptance", current_move.cost <= 0 ? "non_worsening" : "runner_criterion");
    if (trace_move_encoder)
      event.Field("move", trace_move_encoder(current_move.move));
  });
}
} // namespace Core
} // namespace EasyLocal
