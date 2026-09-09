
#pragma once

#include "runners/simulatedannealing.hh"
//#define VERBOSE 0

namespace EasyLocal
{
  
  namespace Core
  {
    template <class Input, class Solution, class Move, class CostStructure = DefaultCostStructure<int>>
    class SimulatedAnnealingOnlyCutoff : public SimulatedAnnealing<Input, Solution, Move, CostStructure>
    {
    public:
        SimulatedAnnealingOnlyCutoff(const Input &in, SolutionManager<Input, Solution, CostStructure> &sm,
                                        NeighborhoodExplorer<Input, Solution, Move, CostStructure> &ne,
                                        std::string name) : SimulatedAnnealing<Input, Solution, Move, CostStructure>(in, sm, ne, name)
        {}
        
    protected:
      bool CoolingNeeded() const;
      void ApplyCooling();
    };

    template <class Input, class Solution, class Move, class CostStructure>
    bool SimulatedAnnealingOnlyCutoff<Input, Solution, Move, CostStructure>::CoolingNeeded() const
    {
        return this->neighbors_accepted >= this->max_neighbors_accepted;
    }
        
    template <class Input, class Solution, class Move, class CostStructure>
    void SimulatedAnnealingOnlyCutoff<Input, Solution, Move, CostStructure>::ApplyCooling()
    {
        this->residual_temperatures = this->total_number_of_temperatures - this->number_of_temperatures;
        // We don't need to redustribute the spared iterations, as the sampled counter is used just for setting accepted moves but does not control anything else.
        
        #if VERBOSE >= 1
          std::cerr << "V1 ";
        this->PrintStatus(std::cerr);
          std::cerr << std::endl;
        #endif
        this->temperature *= this->cooling_rate;
        this->number_of_temperatures++;
        this->neighbors_sampled = 0;
        this->neighbors_accepted = 0;
    }
  } // namespace Core
} // namespace EasyLocal
