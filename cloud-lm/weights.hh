#ifndef CLOUDLM_WEIGHTS_H
#define CLOUDLM_WEIGHTS_H

// Weights for n-grams.  Probability and possibly a backoff.  

namespace cloudlm {
// No inheritance so this will be a POD.  
struct ProbBackoff {
  float prob;
  float backoff;
};

} // namespace cloudlm
#endif // CLOUDLM_WEIGHTS_H
