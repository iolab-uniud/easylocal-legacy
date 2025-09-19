#pragma once

#include <random>
#include <iostream>

namespace EasyLocal
{
  
  namespace Core
  {
    
    // TODO: Create a singleton for the Random class
    
    /** Utility static class to generate pseudo-random values according to distributions.
     In order to make experiments repeatable, each solver must include:
     
     Random::Seed(value);
     */
    template <typename RNG = std::minstd_rand>
    class RandomTemplate
    {
    public:
      /** Generates an uniform random integer in [a, b].
       @param a lower bound
       @param b upper bound
       */
      template <typename T, typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
      static T Uniform(T a, T b)
      {
        std::uniform_int_distribution<T> d(a, b);
        return d(GetInstance().g);
      }
      
      /** Generates an uniform random float in [a, b].
       @param a lower bound
       @param b upper bound
       */
      template <typename T, typename std::enable_if<std::is_floating_point<T>::value>::type* = nullptr>
      static T Uniform(T a, T b)
      {
        std::uniform_real_distribution<T> d(a, b);
        return d(GetInstance().g);
      }
      
      /** Sets a new seed for the random engine. */
      static unsigned int SetSeed(unsigned int seed)
      {
        RandomTemplate& r = GetInstance();
        r.g.seed(seed);
        return r.seed = seed;
      }
      
      static unsigned int GetSeed()
      {
        return GetInstance().seed;
      }
      
      
      static RNG& GetGenerator()
      {
        return GetInstance().g;
      }

      template <typename T>
      static void Shuffle(std::vector<T>& v)
      {
        std::shuffle(v.begin(), v.end(), GetInstance().g);
      }
      
    private:
      static RandomTemplate& GetInstance() {
        static RandomTemplate instance;
        return instance;
      }
      
      RandomTemplate()
      {
        std::random_device dev;
        seed = dev();
        g.seed(seed);
      }
      
      RNG g;
      
      unsigned int seed;
    };

    typedef RandomTemplate<> Random;
  } // namespace Core
} // namespace EasyLocal
