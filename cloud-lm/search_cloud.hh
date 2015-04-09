#ifndef CLOUDLM_SEARCH_CLOUD__
#define CLOUDLM_SEARCH_CLOUD__

#include "cloud-lm/config.hh"
#include "cloud-lm/blank.hh"
#include "cloud-lm/model_type.hh"
#include "cloud-lm/return.hh"
#include "cloud-lm/weights.hh"
#include "cloud-lm/cloud_format.hh"
#include "cloud-lm/vocab.hh"
#include "cloud-lm/value.hh"

#include "util/bit_packing.hh"

#include <boost/algorithm/string/predicate.hpp>
#include <algorithm>
#include <iostream>
#include <vector>

namespace cloudlm {
namespace ngram {

namespace detail {

inline uint64_t CombineWordHash(uint64_t current, const WordIndex next) {
  uint64_t ret = (current * 8978948897894561157ULL) ^ (static_cast<uint64_t>(1 + next) * 17894857484156487943ULL);
  return ret;
}

class LongestPointer {
  public:
    explicit LongestPointer(const float &to) : to_(&to) {}

    LongestPointer() : to_(NULL) {}

    bool Found() const {
      return to_ != NULL;
    }

    float Prob() const {
      return *to_;
    }

  private:
    const float *to_;
};

template <class Value> class CloudSearch {
  public:
    typedef uint64_t Node;

    typedef typename Value::ProbingProxy UnigramPointer;
    typedef typename Value::ProbingProxy MiddlePointer;
    typedef ::cloudlm::ngram::detail::LongestPointer LongestPointer;

    static const ModelType kModelType = Value::kProbingModelType;
    static const bool kDifferentRest = Value::kDifferentRest;
    static const unsigned int kVersion = 0;

    void Initialize(const Config &config, CloudVocabulary &vocab);

    UnigramPointer LookupUnigram(std::string s_unigram, WordIndex word, Node &next, bool &independent_left, uint64_t &extend_left) const {
      extend_left = static_cast<uint64_t>(word);
      next = extend_left;

      // TODO : save StaticData::Instance().GetVerboseLevel() in Config.hh
      /*IFVERBOSE(4) {
        cloudlm::ngram::UpdateRequestStats("unigram", 1);
      }*/

      // TODO leer de config
      Config *config = Config::Instance();
      Data request;
      request.gram = s_unigram;
      request.order = 1;
      typename Value::Weights unigram;

      bool unk = false;
      if (!SendRequest(request, unigram)) {
		unigram.prob = config->unknown_missing_logprob;
		unigram.backoff = kNoExtensionBackoff;
		unk = true;
	  }
      // TODO : test -> arreglar si funciona (esto funciona, lo podemos dejar aquí de momento) 07/04/2015
      if (s_unigram != "<s>" && s_unigram != "<unk>" && !unk) {
          util::UnsetSign(unigram.prob);
      }
      UnigramPointer ret(unigram);
      //std::cout << "1: " << ret.Prob() << " - " << ret.Backoff();
      independent_left = ret.IndependentLeft();
      //std::cout << " - ind: " << independent_left/* << " - " << s_unigram*/ << std::endl;
      // std::cout << ret.Prob() << " - " << ret.Backoff() << std::endl;

      return ret;
    }

    MiddlePointer Unpack(std::string s_ngram, uint64_t extend_pointer, unsigned char extend_length, Node &node) const {
      node = extend_pointer;
      // TODO: check if the unigram "word" always exists.
      Config *config = Config::Instance();
      Data request;
	  request.gram = s_ngram;
	  request.order = extend_length;

	  typename Value::Weights ngram_weights;
	  if (!SendRequest(request, ngram_weights)) {
		//cloudlm::ngram::UpdateRequestStats("no encontrado22", -22);
		return MiddlePointer();
	  }
	  MiddlePointer ret(ngram_weights);

      // TODO : test -> arreglar si funciona (esto funciona, lo podemos dejar aquí de momento) - habría que moverlo al preproceso 07/04/2015
      if (boost::starts_with(s_ngram,"<s>")) {
          util::UnsetSign(ngram_weights.prob);
      }
      return ret;
    }

    MiddlePointer LookupMiddle(std::string s_ngram, unsigned char order, WordIndex word, Node &node, bool &independent_left, uint64_t &extend_pointer) const {

      // TODO remover calls to UpdateRequestStats
      //std::stringstream sstm;
      //sstm << (order_minus_2+2) << "-gram";
      //cloudlm::ngram::UpdateRequestStats(sstm.str(), (order_minus_2+2));

      /*if (word == vocabulary->NotFound()) {
          cloudlm::ngram::UpdateRequestStats("no encontrado2", -2);
        independent_left = true;
        return MiddlePointer();
      }*/

      node = CombineWordHash(node, word);

      // TODO: check if the unigram "word" always exists.
      Config *config = Config::Instance();
      Data request;
  	  request.gram = s_ngram;
	  request.order = order;

	  typename Value::Weights ngram_weights;
	  if (!cloudlm::ngram::SendRequest(request, ngram_weights)) {
          //cloudlm::ngram::UpdateRequestStats("no encontrado22", -22);
		  independent_left = true;
		  //std::cout << (int)order << "n: 0 - 0 - ind: " << independent_left/* << " - " << s_ngram*/ << std::endl;
		  return MiddlePointer();
	  }

      /*typename Middle::ConstIterator found;
      if (!middle_[order_minus_2].Find(node, found)) {
        independent_left = true;
        return MiddlePointer();
      }*/
      extend_pointer = node;
      MiddlePointer ret(ngram_weights);

      // TODO : test -> arreglar si funciona (esto funciona, lo podemos dejar aquí de momento) - habría que moverlo al preproceso 07/04/2015
      if (!boost::starts_with(s_ngram,"<s>")) {
          util::UnsetSign(ngram_weights.prob);
      }
      //util::UnsetSign(ngram_weights.prob);
      //std::cout << (int)order << ": " << ret.Prob() << " - " << ret.Backoff();
      independent_left = ret.IndependentLeft();
      //std::cout << " - ind: " << independent_left/* << " - " << s_ngram*/ << std::endl;
      // std::cout << ret.Prob() << " - " << ret.Backoff() << std::endl;
      return ret;
    }

    LongestPointer LookupLongest(std::string s_ngram, WordIndex word, const Node &node) const {
      // Sign bit is always on because longest n-grams do not extend left.
      /*typename Longest::ConstIterator found;
      if (!longest_.Find(CombineWordHash(node, word), found)) return LongestPointer();
      return LongestPointer(found->value.prob);*/
  	  // TODO remover calls to UpdateRequestStats
  	  //cloudlm::ngram::UpdateRequestStats("l-gram", 4);
    	/*if (word == vocabulary->NotFound()) {
            cloudlm::ngram::UpdateRequestStats("no encontrado3", -3);
		  return LongestPointer();
	    }*/
        Config *config = Config::Instance();
		Data request;
		request.gram = s_ngram;
		request.order = config->max_order;

		typename Value::Weights ngram_weights;
		if (!SendRequest(request, ngram_weights)) {
		  LongestPointer a;
		  //std::cout << "Mn: 0 - "/* << s_ngram */<< std::endl;
		  return LongestPointer();
		}
	    //std::cout << "M: " << ngram_weights.prob << " - "/* << s_ngram*/ << std::endl;
		return LongestPointer(ngram_weights.prob);
    }

    // Generate a node without necessarily checking that it actually exists.
    // Optionally return false if it's know to not exist.
    bool FastMakeNode(const WordIndex *begin, const WordIndex *end, Node &node) const {
      assert(begin != end);
      node = static_cast<Node>(*begin);
      for (const WordIndex *i = begin + 1; i < end; ++i) {
        node = CombineWordHash(node, *i);
      }
      return true;
    }
};

} // namespace detail
} // namespace ngram
} // namespace cloudlm

#endif // CLOUDLM_SEARCH_CLOUD__
