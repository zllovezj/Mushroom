/**
 *    > Author:        UncP
 *    > Github:  www.github.com/UncP/Mushroom
 *    > License:      BSD-3
 *    > Time:    2017-09-05 16:53:25
**/

#include <cstring>

#include "bloom.hpp"

namespace Mushroom {

BloomFilter::BloomFilter(int bits_per_key):bits_per_key_(bits_per_key)
{
  // We intentionally round down to reduce probing cost a little bit
  num_probes_ = static_cast<size_t>(bits_per_key_ * 0.69);  // 0.69 =~ ln(2)
  if (num_probes_ < 1) num_probes_ = 1;
  if (num_probes_ > 30) num_probes_ = 30;
}

void BloomFilter::CreateFilter(const Slice* keys, int n, std::string* dst) const
{
  // Compute bloom filter size (in both bits and bytes)
  size_t bits = n * bits_per_key_;

  // For small n, we can see a very high false positive rate.  Fix it
  // by enforcing a minimum bloom filter length.
  if (bits < 64) bits = 64;

  size_t bytes = (bits + 7) / 8;
  bits = bytes * 8;

  len_ = bytes + 1;
  filter_ = new char[len_];
  memset(filter_, 0, bytes);

  filter_[bytes] = static_cast<char>(num_probes_);  // Remember # of probes

  for (size_t i = 0; i < (size_t)n; i++) {
    // Use double-hashing to generate a sequence of hash values.
    // See analysis in [Kirsch,Mitzenmacher 2006].
    uint32_t h = BloomHash(keys[i]);
    const uint32_t delta = (h >> 17) | (h << 15);  // Rotate right 17 bits
    for (size_t j = 0; j < num_probes_; j++) {
      const uint32_t bitpos = h % bits;
      filter_[bitpos/8] |= (1 << (bitpos % 8));
      h += delta;
    }
  }
}

bool BloomFilter::KeyMayMatch(const Slice& key, const Slice& bloom_filter) const
{
  if (len_ < 2) return false;

  const size_t bits = (len_ - 1) * 8;

  // Use the encoded k so that we can read filters generated by
  // bloom filters created using different parameters.
  const size_t k = filter_[len_ - 1];
  if (k > 30) {
    // Reserved for potentially new encodings for short bloom filters.
    // Consider it a match.
    return true;
  }

  uint32_t h = BloomHash(key);
  const uint32_t delta = (h >> 17) | (h << 15);  // Rotate right 17 bits
  for (size_t j = 0; j < k; j++) {
    const uint32_t bitpos = h % bits;
    if ((filter_[bitpos / 8] & (1 << (bitpos % 8))) == 0) return false;
    h += delta;
  }
  return true;
}

} // namespace Mushroom
