#pragma once

#include <random>
#include <iostream>
#include <algorithm>

namespace EasyLocal
{

  namespace Core
  {

    // TODO: Create a singleton for the Random class

    /** Utility static class to generate pseudo-random values according to distributions.
     In order to make experiments repeatable, each solver must include:

     Random::Seed(value);
     */
    class Random
    {
    public:
      /** Uniform integer in [a, b] (inclusive).
       * Optimized to pick the smallest engine that can cover the value range implied by a, b. */
      template <typename T, typename std::enable_if<std::is_integral<T>::value>::type * = nullptr>
      static T Uniform(T a, T b)
      {
        if (b < a)
          std::swap(a, b);

        auto &r = GetInstance();

        if constexpr (std::is_signed<T>::value)
        {
          // Use signed bounds for the chosen width
          if (a >= std::numeric_limits<int16_t>::min() && b <= std::numeric_limits<int16_t>::max())
          {
            std::uniform_int_distribution<T> d(a, b);
            return d(r.g16);
          }
          else if (a >= std::numeric_limits<int32_t>::min() && b <= std::numeric_limits<int32_t>::max())
          {
            std::uniform_int_distribution<T> d(a, b);
            return d(r.g32);
          }
          else
          {
            std::uniform_int_distribution<T> d(a, b);
            return d(r.g64);
          }
        }
        else
        {
          // Unsigned
          using U = T;
          if (b <= static_cast<U>(std::numeric_limits<uint16_t>::max()))
          {
            std::uniform_int_distribution<U> d(static_cast<U>(a), static_cast<U>(b));
            return static_cast<T>(d(r.g16));
          }
          else if (b <= static_cast<U>(std::numeric_limits<uint32_t>::max()))
          {
            std::uniform_int_distribution<U> d(static_cast<U>(a), static_cast<U>(b));
            return static_cast<T>(d(r.g32));
          }
          else
          {
            std::uniform_int_distribution<U> d(static_cast<U>(a), static_cast<U>(b));
            return static_cast<T>(d(r.g64));
          }
        }
      }

      /** Uniform real in [a, b).
       * Chooses g32 for float, g64 for double/long double.
       */
      template <typename T, typename std::enable_if<std::is_floating_point<T>::value>::type * = nullptr>
      static T Uniform(T a, T b)
      {
        if (b < a)
          std::swap(a, b);
        auto &r = GetInstance();
        std::uniform_real_distribution<T> d(a, b);

        if constexpr (std::is_same<T, float>::value)
        {
          return d(r.g32);
        }
        else
        {
          return d(r.g64);
        }
      }

      // Pick engine for shuffle based on size
      template <typename T>
      static void Shuffle(std::vector<T> &v)
      {
        auto &r = GetInstance();
        if (v.size() <= (1u << 16))
        {
          std::shuffle(v.begin(), v.end(), r.g16);
        }
        else if (v.size() <= (1ull << 32))
        {
          std::shuffle(v.begin(), v.end(), r.g32);
        }
        else
        {
          std::shuffle(v.begin(), v.end(), r.g64);
        }
      }

      /** Sets a new seed for all engines (same seed for reproducibility). */
      static unsigned int SetSeed(unsigned int seed)
      {
        auto &r = GetInstance();
        r.seed = seed ? seed : 1u;
        r.g16.seed(static_cast<decltype(r.g16)::result_type>(r.seed));
        r.g32.seed(static_cast<decltype(r.g32)::result_type>(r.seed));
        r.g64.seed(static_cast<decltype(r.g64)::result_type>(r.seed));
        return r.seed;
      }

      static unsigned int GetSeed()
      {
        return GetInstance().seed;
      }

      template <class T>
      static auto &GetGenerator()
      {
        auto &r = GetInstance();
        if constexpr (sizeof(T) <= 2)
          return r.g16;
        else if constexpr (sizeof(T) <= 4)
          return r.g32;
        else
          return r.g64;
      }

    private:
      static Random& GetInstance() {
        static Random instance;              // Meyers singleton (thread-safe)
        return instance;
      }

      Random()
      {
        std::random_device rd;
        seed = rd() ? rd() : 1u; // avoid seed=0
        g16.seed(static_cast<decltype(g16)::result_type>(seed));
        g32.seed(static_cast<decltype(g32)::result_type>(seed));
        g64.seed(static_cast<decltype(g64)::result_type>(seed));
      }

      std::linear_congruential_engine<uint_fast16_t, 26125, 62303, 0> g16;
      std::linear_congruential_engine<uint_fast32_t, 48271, 0, 2147483647> g32;
      std::linear_congruential_engine<uint_fast64_t, 6364136223846793005ULL, 1442695040888963407ULL, 0> g64;

      unsigned int seed;
    };
  } // namespace Core
} // namespace EasyLocal
