#ifndef CLOUDLM_VALUE_BUILD_H
#define CLOUDLM_VALUE_BUILD_H

#include "cloud-lm/weights.hh"
#include "cloud-lm/word_index.hh"
#include "util/bit_packing.hh"

#include <vector>

namespace cloudlm {
namespace ngram {

struct Config;
struct BackoffValue;
struct RestValue;

class NoRestBuild {
  public:
    typedef BackoffValue Value;

    NoRestBuild() {}

    // TODO delete void SetRest(const WordIndex *, unsigned int, const Prob &/*prob*/) const {}
    void SetRest(const WordIndex *, unsigned int, const ProbBackoff &) const {}

    template <class Second> bool MarkExtends(ProbBackoff &weights, const Second &) const {
      util::UnsetSign(weights.prob);
      return false;
    }

    // Probing doesn't need to go back to unigram.
    const static bool kMarkEvenLower = false;
};

} // namespace ngram
} // namespace cloudlm

#endif // CLOUDLM_VALUE_BUILD_H
