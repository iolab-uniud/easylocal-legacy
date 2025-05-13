#pragma once

#include "runners/simulatedannealingtimebased.hh"

#define VERBOSE 0
#define CHECKER 0

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
    class SimulatedAnnealingWithReheatingTimeBased : public SimulatedAnnealingTimeBased<Input, Solution, Move, CostStructure>
    {
    public:
        SimulatedAnnealingWithReheatingTimeBased(const Input &in, SolutionManager<Input, Solution, CostStructure> &sm,
                                        NeighborhoodExplorer<Input, Solution, Move, CostStructure> &ne,
                                        std::string name) : SimulatedAnnealingTimeBased<Input, Solution, Move, CostStructure>(in, sm, ne, name)
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
      void ApplyCooling();
      bool CoolingNeeded() const;
      // additional parameters
      Parameter<double> reheat_ratio;
      Parameter<double> first_descent_evaluations_share;
      Parameter<unsigned int> max_reheats;
      unsigned int reheats;
      std::chrono::milliseconds first_descent_time, other_descents_time;
    };
    /*************************************************************************
     * Implementation
     *************************************************************************/    
    
    template <class Input, class Solution, class Move, class CostStructure>
    void SimulatedAnnealingWithReheatingTimeBased<Input, Solution, Move, CostStructure>::InitializeRun()
    {
      SimulatedAnnealingTimeBased<Input, Solution, Move, CostStructure>::InitializeRun();
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
        first_descent_time =  std::chrono::milliseconds(static_cast<int>(1000.0 * this->allowed_running_time * first_descent_evaluations_share));
        other_descents_time = (this->run_duration - first_descent_time) / max_reheats; // the other descents will last equally

        // resetting of shared parameters 
        this->allowed_running_time_per_temperature = first_descent_time / this->total_number_of_temperatures;
        this->max_neighbors_sampled = static_cast<unsigned>(this->max_evaluations / this->total_number_of_temperatures); // this is a strong assumption.
        this->current_max_neighbors_sampled = this->max_neighbors_sampled;
        this->max_neighbors_accepted = static_cast<unsigned>(this->max_neighbors_sampled * this->neighbors_accepted_ratio); 
        this->temperature_start_time = this->run_start;
      }      
      if (max_reheats == 0) // this case should be equal to the case where no reheating is performed.
      {
        if (first_descent_evaluations_share != 1.0)
        {
          throw IncorrectParameterValue(first_descent_evaluations_share, "should be 1.0 when max_reheats is 0");
        }
        if (reheat_ratio != 0.0)
        {
          throw IncorrectParameterValue(reheat_ratio, "should be zero when max_reheats is 0");
        }
        first_descent_time = std::chrono::milliseconds(static_cast<int>(1000.0 * this->allowed_running_time));
        other_descents_time = std::chrono::milliseconds(0);
      }

#if VERBOSE == 1
      std::cout << "Initialize run done in reheating -----------------------" << std::endl;
      std::cout << "Iterations " << this->evaluations << std::endl;
      std::cout << "max_reheats " << max_reheats << std::endl;
      std::cout << "reheat_ratio " << reheat_ratio << std::endl;
      std::cout << "start_temperature " << this->start_temperature << std::endl;
      std::cout << "min_temperature " << this->min_temperature << std::endl;
      std::cout << "total_number_of_temperatures " << this->total_number_of_temperatures << std::endl;
      std::cout << "max_neighbors_sampled " << this->max_neighbors_sampled << std::endl;
      std::cout << "max_neighbors_accepted (cut-off) " << this->max_neighbors_accepted << std::endl;
      std::cout << "max_evaluations " << this->max_evaluations << std::endl;
      std::cout << "max_evaluations " << this->max_evaluations << std::endl;
      std::cout << "first_descent_time " << std::chrono::duration_cast<std::chrono::milliseconds>(first_descent_time).count()/1000.0 << " (share " << first_descent_evaluations_share << ")" << std::endl;
      std::cout << "other_descents_time " << std::chrono::duration_cast<std::chrono::milliseconds>(other_descents_time).count()/1000.0 << std::endl;
      std::cout << "running_time_per_temperature " << std::chrono::duration_cast<std::chrono::milliseconds>(this->allowed_running_time_per_temperature).count()/1000.0 << std::endl;
#endif
    }    
    /**
     A move is randomly picked.
     */
    template <class Input, class Solution, class Move, class CostStructure>
    void SimulatedAnnealingWithReheatingTimeBased<Input, Solution, Move, CostStructure>::CompleteMove()
    {
      SimulatedAnnealing<Input, Solution, Move, CostStructure>::CompleteMove(); // just update the neighbors accepted parameter      

      if (ReheatCondition() && reheats < max_reheats)
      {
        // all reheats are equal
        if (reheats == 0) // at the first reheat possibility, set the parameters
	      {
          this->start_temperature = this->start_temperature * reheat_ratio;
          this->total_number_of_temperatures = static_cast<unsigned>(ceil(-log(this->start_temperature / this->min_temperature) / log(this->cooling_rate)));      
          this->allowed_running_time_per_temperature = other_descents_time / this->total_number_of_temperatures;
          this->max_neighbors_sampled = static_cast<unsigned>(((this->max_evaluations - this->evaluations)/max_reheats) / this->total_number_of_temperatures);
          this->current_max_neighbors_sampled = this->max_neighbors_sampled;
          this->max_neighbors_accepted = static_cast<unsigned>(this->max_neighbors_sampled * this->neighbors_accepted_ratio); 
#if VERBOSE == 1
          std::cout << "Reheat parameters are reset -----------------------" << std::endl;
          std::cout << "Iterations " << this->evaluations << std::endl;
          std::cout << "max_reheats " << max_reheats << std::endl;
          std::cout << "reheats " << reheats << std::endl;
          std::cout << "reheat_ratio " << reheat_ratio << std::endl;
          std::cout << "start_temperature " << this->start_temperature << std::endl;
          std::cout << "min_temperature " << this->min_temperature << std::endl;
          std::cout << "total_number_of_temperatures " << this->total_number_of_temperatures  << std::endl;
          std::cout << "max_neighbors_sampled " << this->max_neighbors_sampled << std::endl;
          std::cout << "max_neighbors_accepted (cut-off) " << this->max_neighbors_accepted << std::endl;
          std::cout << "max_evaluations " << this->max_evaluations << std::endl;
          std::cout << "total_time " << std::chrono::duration_cast<std::chrono::milliseconds>(this->run_duration).count()/1000.0 << std::endl;
          std::cout << "first_descent_time " << std::chrono::duration_cast<std::chrono::milliseconds>(first_descent_time).count()/1000.0 << " (share " << first_descent_evaluations_share << ")" << std::endl;
          std::cout << "other_descents_time " << std::chrono::duration_cast<std::chrono::milliseconds>(other_descents_time).count()/1000.0 << std::endl;
          std::cout << "running_time_per_temperature " << std::chrono::duration_cast<std::chrono::milliseconds>(this->allowed_running_time_per_temperature).count()/1000.0 << std::endl;
#endif
	      }
        reheats++;
        // reset temperature and other counters
        this->temperature = this->start_temperature;
        this->number_of_temperatures = 1;
        this->neighbors_sampled = 0;
        this->neighbors_accepted = 0;
        this->temperature_end_time = std::chrono::system_clock::now();
        this->temperature_start_time = this->temperature_end_time;
#if VERBOSE == 1
        std::cout << "Performing reheat -----------------------" << std::endl;
        std::cout << "Iterations " << this->evaluations << std::endl;
        std::cout << "max_reheats " << max_reheats << std::endl;
        std::cout << "reheats " << reheats << std::endl;
        std::cout << "temperature " << this->temperature << std::endl;
#endif
      }

    }

    template <class Input, class Solution, class Move, class CostStructure>
    void SimulatedAnnealingWithReheatingTimeBased<Input, Solution, Move, CostStructure>::ApplyCooling() // this is redone because you need to account for the several temperature descents
    {
      this->temperature_end_time = std::chrono::system_clock::now();
      this->residual_temperatures = this->total_number_of_temperatures - this->number_of_temperatures; 
      if (this->temperature_end_time - this->temperature_start_time < this->allowed_running_time_per_temperature && this->residual_temperatures > 0)
      {
        this->residual_running_time = std::chrono::duration_cast<std::chrono::milliseconds>(first_descent_time - (this->temperature_end_time - this->run_start));
        if (reheats > 0)
        {
          this->residual_running_time = std::chrono::duration_cast<std::chrono::milliseconds>(other_descents_time - (this->temperature_end_time - this->run_start));
        }
        this->allowed_running_time_per_temperature = this->residual_running_time/this->residual_temperatures;
      }
      if (this->residual_temperatures > 0) // this is a "guard": if you arrive at the last temperature
      {
        this->temperature *= this->cooling_rate;
        this->number_of_temperatures++;
        this->neighbors_sampled = 0;
        this->neighbors_accepted = 0;
        this->temperature_start_time = this->temperature_end_time;
      }
#if VERBOSE == 1
      std::cout << "Applied cooling, temperature is now " << this->temperature  << " allowed running time " <<  std::chrono::duration_cast<std::chrono::milliseconds>(this->allowed_running_time_per_temperature).count()/1000.0 << std::endl;
#endif
#if CHECKER == 1
      if (this->temperature < this->min_temperature)
      {
        std::cout << "Temperature is not under control -- " << this->number_of_temperatures << " (min "<< this->min_temperature << " vs temperature " << this->temperature << ")" << std::endl;
      }
#endif
      
    }
    
    template <class Input, class Solution, class Move, class CostStructure>
    bool SimulatedAnnealingWithReheatingTimeBased<Input, Solution, Move, CostStructure>::CoolingNeeded() const
    {
      // In this version of SA (TimeBased)temperature is decreased based on running
      // time or cut-off (no cooling based on number of iterations)
      bool condition = std::chrono::system_clock::now() > this->temperature_start_time + this->allowed_running_time_per_temperature 
      || SimulatedAnnealing<Input, Solution, Move, CostStructure>::CoolingNeeded();
#if CHECKER == 1
      if (condition)
      {
        std::cout << "Condition for cooling in SA reheating tb is satisfied " 
        << std::chrono::duration_cast<std::chrono::milliseconds>( this->temperature_start_time - std::chrono::system_clock::now() ).count()/1000.0 
        << " running for " << std::chrono::duration_cast<std::chrono::milliseconds>( this->run_start - std::chrono::system_clock::now() ).count()/1000.0 
        << std::endl;
      }
#endif
      return condition;
    }

    template <class Input, class Solution, class Move, class CostStructure>
    bool SimulatedAnnealingWithReheatingTimeBased<Input, Solution, Move, CostStructure>::ReheatCondition()
    {
      if (max_reheats == 0)
      {
        return false; 
      }
      bool condition = std::chrono::system_clock::now() >= this->run_start + first_descent_time + (other_descents_time * reheats) && reheats < max_reheats;
# if VERBOSE == 1
      if (condition)
      {
        std::cout << "Reheat condition is true -----------------------" << std::endl;
        std::cout << "other_descents_time " << std::chrono::duration_cast<std::chrono::milliseconds>(other_descents_time).count()/1000.0 << std::endl;
      }
#endif
      return condition;
      //  >= first_descent_evaluations + other_descents_evaluations * reheats; // se hai fatto le valutazioni del caso; tempo per la discesa è passato
    }
  
    template <class Input, class Solution, class Move, class CostStructure>
    bool SimulatedAnnealingWithReheatingTimeBased<Input, Solution, Move, CostStructure>::StopCriterion()
    {
      bool condition = reheats >= max_reheats && this->temperature <= this->min_temperature;
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
    std::string SimulatedAnnealingWithReheatingTimeBased<Input, Solution, Move, CostStructure>::StatusString() const
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
