#pragma once

#include <cmath>
#include <stdexcept>
#include <algorithm>

#include "runners/simulatedannealing.hh"
#include "helpers/solutionmanager.hh"
#include "helpers/neighborhoodexplorer.hh"
namespace EasyLocal
{
namespace Core
{

/** The  Simulated annealing runner relies on a probabilistic local
 search technique whose name comes from the fact that it
 simulates the cooling of a collection of hot vibrating atoms.
 
 At each iteration a candidate move is generated at random, and
 it is always accepted if it is an improving move.  Instead, if
 the move is a worsening one, the new solution is accepted with
 time decreasing probability.
 
 The stop condition is delegated to the concrete subclasses
 
 @ingroup Runners
 */
template <class Input, class Solution, class Move, class CostStructure = DefaultCostStructure<int>>
class SimulatedAnnealingFixedTemperature : public SimulatedAnnealing<Input, Solution, Move, CostStructure>
{
public:
  using SimulatedAnnealing<Input, Solution, Move, CostStructure>::SimulatedAnnealing;
protected:
  void InitializeRun() override;

  bool CoolingNeeded() const override;

  bool StopCriterion() override;
};
  
  /*************************************************************************
 * Implementation
 *************************************************************************/
/**
 Initializes the run by invoking the companion superclass method, and
 setting the temperature to the start value.
 */
template <class Input, class Solution, class Move, class CostStructure>
void SimulatedAnnealingFixedTemperature<Input, Solution, Move, CostStructure>::InitializeRun()
{
  MoveRunner<Input, Solution, Move, CostStructure>::InitializeRun();

  this->compute_start_temperature = false; 

  if (!this->neighbors_accepted_ratio.IsSet() && !this->max_neighbors_accepted.IsSet())
     this->neighbors_accepted_ratio = 1.0;  
  
  if (this->cooling_rate.IsSet())
    throw IncorrectParameterValue(this->cooling_rate, "should not be set for fixed temperature simulated annealing");

  if (this->start_temperature != this->min_temperature)
    throw IncorrectParameterValue(this->start_temperature, "should be equal than min_temperature for fixed temperature simulated annealing");
  if (this->min_temperature <= 0.0)
    throw IncorrectParameterValue(this->min_temperature, "should be greater than zero");

  this->temperature = this->start_temperature;
  if (this->max_evaluations.IsSet())
    { // Compute max_neighbors_sampled from max_evaluations
      this->max_neighbors_sampled = this->max_evaluations;
    }

  // max_neighbors_sampled is fixed (and used for cut-off), its current value changes due to saved iterations
  this->current_max_neighbors_sampled = this->max_neighbors_sampled;

  this->max_neighbors_accepted = static_cast<unsigned>(this->max_neighbors_sampled * this->neighbors_accepted_ratio);
    
  // initialize dynamic counters
  this->neighbors_sampled = 0;
  this->neighbors_accepted = 0;
  this->number_of_temperatures = 1;
  this->iteration = 0;
}


template <class Input, class Solution, class Move, class CostStructure>
bool SimulatedAnnealingFixedTemperature<Input, Solution, Move, CostStructure>::CoolingNeeded() const
{
  return false;
} 

/**
   The search stops when a low temperature has reached.
*/
template <class Input, class Solution, class Move, class CostStructure>
bool SimulatedAnnealingFixedTemperature<Input, Solution, Move, CostStructure>::StopCriterion()
{
  return this->neighbors_accepted >= this->max_neighbors_accepted || this->evaluations >= this->max_neighbors_sampled;
}
  
} // namespace Core
} // namespace EasyLocal
