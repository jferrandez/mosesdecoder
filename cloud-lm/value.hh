#ifndef CLOUDLM_VALUE_H
#define CLOUDLM_VALUE_H

#include "cloud-lm/model_type.hh"
#include "cloud-lm/value_build.hh"
#include "cloud-lm/weights.hh"
#include "util/bit_packing.hh"

#include <stdint.h>

namespace cloudlm {
namespace ngram {

// Template proxy for probing unigrams and middle.
template <class Weights> class GenericProbingProxy {
  public:
    explicit GenericProbingProxy(const Weights &to) : to_(&to) {}

    GenericProbingProxy() : to_(0) {}

    bool Found() const { return to_ != 0; }

    float Prob() const {
      util::FloatEnc enc;
      enc.f = to_->prob;
      enc.i |= util::kSignBit;
      return enc.f;
    }

    float Backoff() const { return to_->backoff; }

    bool IndependentLeft() const {
      util::FloatEnc enc;
      enc.f = to_->prob;
      return enc.i & util::kSignBit;
    }

  protected:
    const Weights *to_;
};

struct BackoffValue {
  typedef ProbBackoff Weights;
  static const ModelType kProbingModelType = CLOUD_PROBING;

  class ProbingProxy : public GenericProbingProxy<Weights> {
    public:
      explicit ProbingProxy(const Weights &to) : GenericProbingProxy<Weights>(to) {}
      ProbingProxy() {}
      float Rest() const { return Prob(); }
  };

  template <class Model, class C> void Callback(const Config &, unsigned int, typename Model::Vocabulary &, C &callback) {
    NoRestBuild build;
    callback(build);
  }

  const static bool kDifferentRest = false;
};

} // namespace ngram
} // namespace cloudlm

#endif // CLOUDLM_VALUE_H
