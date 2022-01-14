/**
 *  @note This file is part of Empirical, https://github.com/devosoft/Empirical
 *  @copyright Copyright (C) Michigan State University, MIT Software license; see doc/LICENSE.md
 *  @date 2019-2021.
 *
 *  @file hash_utils.hpp
 *  @brief This file provides tools for hashing values and containers.
 *  @note Status: BETA
 */

#ifndef EMP_DATASTRUCTS_HASH_UTILS_HPP_INCLUDE
#define EMP_DATASTRUCTS_HASH_UTILS_HPP_INCLUDE

#include <cstddef>
#include <cstring>
#include <numeric>
#include <stdint.h>
#include <type_traits>

#include "../base/Ptr.hpp"

namespace emp {

  namespace internal {
    // Allow a hash to be determined by a Hash() member function.
    template <typename T>
    auto Hash_impl(const T & x, bool) noexcept -> decltype(x.Hash()) { return x.Hash(); }

    // By default, use std::hash if nothing else exists.
    template <typename T>
    auto Hash_impl(const T & x, int) noexcept -> decltype(std::hash<T>()(x)) { return std::hash<T>()(x); }

    // Try direct cast to size_t if nothing else works.
    template <typename T>
    std::size_t Hash_impl(const T & x, ...) noexcept {
      // @CAO Setup directory structure to allow the following to work:
      // LibraryWarning("Resorting to casting to size_t for emp::Hash implementation.");
      return (size_t) x;
    }
  }

  // Setup hashes to be dynamically determined
  template <typename T>
  std::size_t Hash(const T & x) noexcept { return internal::Hash_impl(x, true); }


  /// Generate a unique long from a pair of ints.
  /// @param a_ First 32-bit unsigned int.
  /// @param b_ Second 32-bit unsigned int.
  /// @return 64-bit unsigned int representing the szudzik hash of both inputs.
  inline uint64_t szudzik_hash(uint32_t a_, uint32_t b_) noexcept
  {
    uint64_t a = a_, b = b_;
    return a >= b ? a * a + a + b : a + b * b;
  }

  /// If hash_combine has a single value, there's nothing to combine; just return it!
  constexpr inline std::size_t hash_combine(std::size_t hash1) noexcept { return hash1; }

  /// Boost's implementation of a simple hash-combining function.
  /// Taken from https://www.boost.org/doc/libs/1_37_0/doc/html/hash/reference.html#boost.hash_combine
  /// @param hash1 First hash to combine.
  /// @param hash2 Second hash to combine.
  /// @return Combined hash.
  constexpr inline std::size_t hash_combine(std::size_t hash1, std::size_t hash2) noexcept
  {
    return hash1 ^ (hash2 + 0x9e3779b9 + (hash1 << 6) + (hash1 >> 2));
  }

  /// Allow hash_combine to work with more than two input values.
  template <typename... Ts>
  constexpr inline std::size_t hash_combine(std::size_t hash1, std::size_t hash2,
                                            std::size_t hash3, Ts... extras) noexcept
  {
    // combine the first two, put them at the end (so the same ones don't keep getting recombined
    // every step of the way through), and recurse.
    std::size_t partial_hash = hash_combine(hash1, hash2);
    return hash_combine(hash3, extras..., partial_hash);
  }

  /// Allow hash_combine to take a series of size_t's to merge into a single hash.
  inline std::size_t hash_combine(emp::Ptr<const std::size_t> hashes, size_t num_hashes) noexcept
  {
    emp_assert(num_hashes > 0);                // At least one hash is required!
    if (num_hashes == 1) return hashes[0];     // If we have exactly one, just return it.

    // Combine the last two hashes into partial hash.
    const std::size_t partial_hash = hash_combine(hashes[num_hashes-1], hashes[num_hashes-2]);
    if (num_hashes == 2) return partial_hash;

    // Recurse!
    const std::size_t partial_hash2 = hash_combine(hashes, num_hashes-2);

    return hash_combine(partial_hash, partial_hash2);
  }

  /// Alias hash_combine() to CombineHash()
  template<typename... Ts>
  // inline std::size_t CombineHash(Ts &&... args) {
  //   return hash_combine(Hash<Ts>(std::forward<Ts>(args))...);
  inline std::size_t CombineHash(const Ts &... args) {
    return hash_combine(Hash<Ts>(args)...);
  }

  // helper functions for murmur hash
  #ifndef DOXYGEN_SHOULD_SKIP_THIS
  namespace internal {
    constexpr inline uint64_t rotate(const uint64_t x, const uint64_t r) noexcept {
      return (x << r) | (x >> (64 - r));
    }
    constexpr inline void fmix64(uint64_t& k) noexcept {
      k ^= k >> 33;
      k *= 0xff51afd7ed558ccd;
      k ^= k >> 33;
      k *= 0xc4ceb9fe1a85ec53;
      k ^= k >> 33;
    }
  }
  #endif // DOXYGEN_SHOULD_SKIP_THIS

  /// This structure serves as a hash for containers that are iterable.
  /// Use as a drop-in replacement for std::hash.
  template <typename Container, size_t Seed = 0>
  struct ContainerHash
  {
    size_t operator()(const Container& v) const {
        using item_type = typename std::decay<decltype(*v.begin())>::type;
        const std::hash<item_type> hasher;
        return std::accumulate(
          v.begin(),
          v.end(),
          Seed,
          [&hasher](const size_t accumulator, const auto& item){
            // TODO: profile and consider using murmur_hash instead
            return hash_combine(accumulator, hasher(item));
          }
        );
    }
  };
}

#endif // #ifndef EMP_DATASTRUCTS_HASH_UTILS_HPP_INCLUDE
