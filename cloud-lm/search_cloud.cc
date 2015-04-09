#include "cloud-lm/search_cloud.hh"

#include <string>

namespace cloudlm {
namespace ngram {

class ProbingModel;

namespace {

} // namespace
namespace detail {

template <class Value> void CloudSearch<Value>::Initialize(const Config &config, CloudVocabulary &vocab) {

	// TODO check if specials exist in CLOUD, put into cache (special method for <unk> if no exists)
  CheckSpecials(config, vocab);

}

template class CloudSearch<BackoffValue>;

} // namespace detail
} // namespace ngram
} // namespace cloudlm
