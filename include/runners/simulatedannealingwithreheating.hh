#pragma once

#include "runners/simulatedannealing.hh"

#define VERBOSE 0

namespace EasyLocal
{
  
  namespace Core
  {
    
    /** The Simulated annealing with Reheating runner relies on a probabilistic local
     search technique whose name comes from the fact that it
     simulates the cooling of a collection of hot vibrating atoms.
     
     At each iteration a candidate move is generated at random, and
     it is always accepted if it is an improving move.  Instead, if
     the move is a worsening one, the new solution is accepted with
     time decreasing probability.
     
     @ingroup Runners
     */
    template <class Input, class Solution, class Move, class CostStructure = DefaultCostStructure<int>>
    class SimulatedAnnealingWithReheating : public SimulatedAnnealing<Input, Solution, Move, CostStructure>
    {
    public:
        SimulatedAnnealingWithReheating(const Input &in, SolutionManager<Input, Solution, CostStructure> &sm,
                                        NeighborhoodExplorer<Input, Solution, Move, CostStructure> &ne,
                                        std::string name) : SimulatedAnnealing<Input, Solution, Move, CostStructure>(in, sm, ne, name)
        {
            reheat_ratio("reheat_ratio", "Reheat ratio", this->parameters);
            first_descent_evaluations_share("first_descent_evaluations_share", "First descent cost function evaluations share", this->parameters);
            max_reheats("max_reheats", "Maximum number of reheats", this->parameters);
        }
      
      std::string StatusString() const;
      
    protected:
      bool StopCriterion();
      void CompleteMove();
      void InitializeRun();
      bool ReheatCondition();
      // additional parameters
      Parameter<double> reheat_ratio;
      Parameter<double> first_descent_evaluations_share;
      Parameter<unsigned int> max_reheats;
      unsigned int reheats;
      unsigned int first_descent_evaluations, other_descents_evaluations;
    };
    /*************************************************************************
     * Implementation
     *************************************************************************/    
    
    template <class Input, class Solution, class Move, class CostStructure>
    void SimulatedAnnealingWithReheating<Input, Solution, Move, CostStructure>::InitializeRun()
    {
      SimulatedAnnealing<Input, Solution, Move, CostStructure>::InitializeRun();
      reheats = 0;
      
      if (max_reheats > 0)
      {
        if (reheat_ratio <= 0.0)
        {
          throw IncorrectParameterValue(reheat_ratio, "should be greater than zero");
        }
        if (first_descent_evaluations_share <= 0.0 || first_descent_evaluations_share > 1.0)
        {
          throw IncorrectParameterValue(first_descent_evaluations_share, "should be a value in the interval ]0, 1]");
        }
        this->max_neighbors_sampled = ceil(this->max_neighbors_sampled * first_descent_evaluations_share);
        first_descent_evaluations = ceil(this->max_evaluations * first_descent_evaluations_share);
        other_descents_evaluations = ceil ((this->max_evaluations - first_descent_evaluations) / max_reheats);
      }
      // static_cast<unsigned>(max_neighbors_sampled * neighbors_accepted_ratio);
      this->max_neighbors_accepted = static_cast<unsigned>(this->max_neighbors_sampled * this->neighbors_accepted_ratio); // ceil(this->max_neighbors_sampled * this->neighbors_accepted_ratio);
      if (max_reheats == 0)
      {
        if (first_descent_evaluations_share != 1.0)
        {
          throw IncorrectParameterValue(first_descent_evaluations_share, "should be 1.0 when max_reheats is 0");
        }
        if (reheat_ratio != 0.0)
        {
          throw IncorrectParameterValue(reheat_ratio, "should be zero when max_reheats is 0");
        }
      }

#if VERBOSE == 1
      std::cout << "Initilize run done -----------------------" << std::endl;
      std::cout << "Iterations " << this->evaluations << std::endl;
      std::cout << "max_reheats " << max_reheats << std::endl;
      std::cout << "reheat_ratio " << reheat_ratio << std::endl;
      std::cout << "start_temperature " << this->start_temperature << std::endl;
      std::cout << "total_number_of_temperatures " << this->total_number_of_temperatures << std::endl;
      std::cout << "max_neighbors_sampled " << this->max_neighbors_sampled << std::endl;
      std::cout << "max_neighbors_accepted (cut-off) " << this->max_neighbors_accepted << std::endl;
      std::cout << "max_evaluations " << this->max_evaluations << std::endl;
      std::cout << "first_descent_evaluations " << first_descent_evaluations << " (share " << first_descent_evaluations_share << ")" << std::endl;
      std::cout << "other_descents_evaluations " << other_descents_evaluations << std::endl;
#endif
    }
    
    
    /**
     A move is randomly picked.
     */
    template <class Input, class Solution, class Move, class CostStructure>
    void SimulatedAnnealingWithReheating<Input, Solution, Move, CostStructure>::CompleteMove()
    {
      SimulatedAnnealing<Input, Solution, Move, CostStructure>::CompleteMove();
      
      if (ReheatCondition() && reheats <= max_reheats)
      {
        // all reheats are equal
        if (reheats == 0)
	      {
          this->start_temperature = this->start_temperature * reheat_ratio;
          // this->total_number_of_temperatures = -log(this->start_temperature / this->min_temperature) / log(this->cooling_rate);    
          this->total_number_of_temperatures = static_cast<unsigned>(ceil(-log(this->start_temperature / this->min_temperature) / log(this->cooling_rate)));      
          this->max_neighbors_sampled = other_descents_evaluations / this->total_number_of_temperatures; 
          // set the cutoff
          this->max_neighbors_accepted = ceil(this->neighbors_accepted_ratio * this->max_neighbors_sampled);

#if VERBOSE == 1
          std::cout << "Reheat parameters are reset -----------------------" << std::endl;
          std::cout << "Iterations " << this->evaluations << std::endl;
          std::cout << "max_reheats " << max_reheats << std::endl;
          std::cout << "reheats " << reheats << std::endl;
          std::cout << "reheat_ratio " << reheat_ratio << std::endl;
          std::cout << "start_temperature " << this->start_temperature << std::endl;
          std::cout << "total_number_of_temperatures " << this->total_number_of_temperatures  << std::endl;
          std::cout << "max_neighbors_sampled " << this->max_neighbors_sampled << std::endl;
          std::cout << "max_neighbors_accepted (cut-off) " << this->max_neighbors_accepted << std::endl;
          std::cout << "max_evaluations " << this->max_evaluations << std::endl;
          std::cout << "first_descent_evaluations " << first_descent_evaluations << " (share " << first_descent_evaluations_share << ")" << std::endl;
          std::cout << "other_descents_evaluations " << other_descents_evaluations << std::endl;
#endif
	      }
        reheats++;
        // reset temperature
        this->temperature = this->start_temperature;
#if VERBOSE == 1
        std::cout << "Performing reheat -----------------------" << std::endl;
        std::cout << "max_reheats " << max_reheats << std::endl;
        std::cout << "reheats " << reheats << std::endl;
        std::cout << "temperature " << this->temperature << std::endl;
#endif
      }

    }
    
    template <class Input, class Solution, class Move, class CostStructure>
    bool SimulatedAnnealingWithReheating<Input, Solution, Move, CostStructure>::ReheatCondition()
    {
      if (max_reheats == 0)
      {
        return false; 
      }
      return this->evaluations >= first_descent_evaluations + other_descents_evaluations * reheats;
    }
  
    template <class Input, class Solution, class Move, class CostStructure>
    bool SimulatedAnnealingWithReheating<Input, Solution, Move, CostStructure>::StopCriterion()
    {
      bool condition = reheats > max_reheats;
#if VERBOSE == 1
      if (condition)
      {
        std::cout << "Stop criterion check -----------------------" << std::endl;
        std::cout << "max_reheats " << max_reheats << std::endl;
        std::cout << "reheats " << reheats << std::endl;
      }
#endif
      return condition;
    }
    
    /**
     Create a string containing the status of the runner
     */
    template <class Input, class Solution, class Move, class CostStructure>
    std::string SimulatedAnnealingWithReheating<Input, Solution, Move, CostStructure>::StatusString() const
    {
      std::stringstream status;
      status << "["
      << "Temp = " << this->temperature << " (" << this->start_temperature << "), "
      << "NS = " << this->neighbors_sampled << " (" << this->max_neighbors_sampled << "), "
      << "NA = " << this->neighbors_accepted << " (" << this->max_neighbors_accepted << "), "
      << "Reheats = " << reheats << " (" << max_reheats << ")"
      << "]";
      return status.str();
    }
  } // namespace Core
} // namespace EasyLocal
