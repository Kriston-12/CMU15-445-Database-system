//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// hyperloglog.cpp
//
// Identification: src/primer/hyperloglog.cpp
//
// Copyright (c) 2015-2025, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "primer/hyperloglog.h"

namespace bustub {

/** @brief Parameterized constructor. */
template <typename KeyType>
HyperLogLog<KeyType>::HyperLogLog(int16_t n_bits) : cardinality_(0) {
  numRegisters = 1 << n_bits;
  indexBits = n_bits;
  registers.resize(numRegisters, 0);
}

/**
 * @brief Function that computes binary.
 *
 * @param[in] hash
 * @returns binary of a given hash
 */
template <typename KeyType>
auto HyperLogLog<KeyType>::ComputeBinary(const hash_t &hash) const -> std::bitset<BITSET_CAPACITY> {
  /** @TODO(student) Implement this function! */
  // std::bitset<BITSET_CAPACITY> rlt;
  // int i = BITSET_CAPACITY - 1;
  // while (hash) {
  //   if (hash % 2) {
  //     rlt[i] = 1;
  //   }
  //   else {
  //     rlt[i] = 0;
  //   }
  //   hash /= 2;
  //   i--;
  // }

  // return rlt;

  return std::bitset<BITSET_CAPACITY>(hash);
}

/**
 * @brief Function that computes leading zeros.
 *
 * @param[in] bset - binary values of a given bitset
 * @returns leading zeros of given binary set
 */
template <typename KeyType>
auto HyperLogLog<KeyType>::PositionOfLeftmostOne(const std::bitset<BITSET_CAPACITY> &bset) const -> uint64_t {
  /** @TODO(student) Implement this function! */
  uint64_t pos = 1;
  // From left to right
  for (int i = BITSET_CAPACITY - indexBits; i >= 0; --i) {
    if (bset[i]) {
      break;
    }
    else {
      pos++;
    }
  }
  return pos;
}

/**
 * @brief Adds a value into the HyperLogLog.
 *
 * @param[in] val - value that's added into hyperloglog
 */
template <typename KeyType>
auto HyperLogLog<KeyType>::AddElem(KeyType val) -> void {
  /** @TODO(student) Implement this function! */
  size_t hash = CalculateHash(val);
  std::bitset<BITSET_CAPACITY> binary = ComputeBinary(hash);
  
  int64_t index = (binary >> (BITSET_CAPACITY - indexBits)).to_ullong();
  uint64_t pLeftOne = PositionOfLeftmostOne(binary);

  registers[index] = std::max(pLeftOne, registers[index]);
}

/**
 * @brief Function that computes cardinality.
 */
template <typename KeyType>
auto HyperLogLog<KeyType>::ComputeCardinality() -> void {
  /** @TODO(student) Implement this function! */
  double divisor = 0.0;
  for (size_t i = 0; i < numRegisters; i++) {
    divisor += pow(2, -static_cast<int>(registers[i]));
  }

  double m = static_cast<double>(numRegisters);
  cardinality_ = floor(CONSTANT * m * m / divisor);
}

template class HyperLogLog<int64_t>;
template class HyperLogLog<std::string>;

}  // namespace bustub
