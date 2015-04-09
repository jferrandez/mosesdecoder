#ifndef CLOUDLM_MODEL_H
#define CLOUDLM_MODEL_H

//#include "cloud-lm/bhiksha.hh"
//#include "cloud-lm/binary_format.hh"
#include "cloud-lm/config.hh"
#include "cloud-lm/facade.hh"
//#include "cloud-lm/quantize.hh"
//#include "cloud-lm/search_hashed.hh"
//#include "cloud-lm/search_trie.hh"
#include "cloud-lm/search_cloud.hh"
#include "cloud-lm/state.hh"
#include "cloud-lm/value.hh"
#include "cloud-lm/vocab.hh"
#include "cloud-lm/weights.hh"

//#include "util/murmur_hash.hh"

#include <algorithm>
#include <vector>

#include <string.h>

namespace util { class FilePiece; }

namespace cloudlm {
namespace ngram {
namespace detail {

// Should return the same results as SRI.  
// ModelFacade typedefs Vocabulary so we use VocabularyT to avoid naming conflicts.
template <class Search, class VocabularyT> class GenericModel : public base::ModelFacade<GenericModel<Search, VocabularyT>, State, VocabularyT> {
  private:
    typedef base::ModelFacade<GenericModel<Search, VocabularyT>, State, VocabularyT> P;
  public:
    // This is the model type returned by RecognizeBinary.
    static const ModelType kModelType;

    static const unsigned int kVersion = Search::kVersion;

    /* Just for save the config
     */
    explicit GenericModel(WordIndex begin, WordIndex end, WordIndex unk);

    /* Score p(new_word | in_state) and incorporate new_word into out_state.
     * Note that in_state and out_state must be different references:
     * &in_state != &out_state.  
     */
    FullScoreReturn FullScore(const State &in_state, const WordIndex new_word, State &out_state, const cloudlm::WordIndexToString words) const;

    /* Slower call without in_state.  Try to remember state, but sometimes it
     * would cost too much memory or your decoder isn't setup properly.  
     * To use this function, make an array of WordIndex containing the context
     * vocabulary ids in reverse order.  Then, pass the bounds of the array:
     * [context_rbegin, context_rend).  The new_word is not part of the context
     * array unless you intend to repeat words.  
     */
    FullScoreReturn FullScoreForgotState(const WordIndex *context_rbegin, const WordIndex *context_rend, const WordIndex new_word, State &out_state, const cloudlm::WordIndexToString words) const;

    /* Get the state for a context.  Don't use this if you can avoid it.  Use
     * BeginSentenceState or NullContextState and extend from those.  If
     * you're only going to use this state to call FullScore once, use
     * FullScoreForgotState. 
     * To use this function, make an array of WordIndex containing the context
     * vocabulary ids in reverse order.  Then, pass the bounds of the array:
     * [context_rbegin, context_rend).  
     */
    void GetState(const WordIndex *context_rbegin, const WordIndex *context_rend, State &out_state, const cloudlm::WordIndexToString words) const;

    /* More efficient version of FullScore where a partial n-gram has already
     * been scored.  
     * NOTE: THE RETURNED .rest AND .prob ARE RELATIVE TO THE .rest RETURNED BEFORE.  
     */
    FullScoreReturn ExtendLeft(
        // Additional context in reverse order.  This will update add_rend to 
        const WordIndex *add_rbegin, const WordIndex *add_rend,
        // Backoff weights to use.  
        const float *backoff_in,
        // extend_left returned by a previous query.
        uint64_t extend_pointer,
        // Length of n-gram that the pointer corresponds to.  
        unsigned char extend_length,
        // Where to write additional backoffs for [extend_length + 1, min(Order() - 1, return.ngram_length)]
        float *backoff_out,
        // Amount of additional content that should be considered by the next call.
        unsigned char &next_use,
		const cloudlm::WordIndexToString words) const;

    /* Return probabilities minus rest costs for an array of pointers.  The
     * first length should be the length of the n-gram to which pointers_begin
     * points.  
     */
    float UnRest(const uint64_t *pointers_begin, const uint64_t *pointers_end, unsigned char first_length, const cloudlm::WordIndexToString words) const {
      // Compiler should optimize this if away.  
      return Search::kDifferentRest ? InternalUnRest(pointers_begin, pointers_end, first_length, words) : 0.0;
    }

  private:
    FullScoreReturn ScoreExceptBackoff(const WordIndex *const context_rbegin, const WordIndex *const context_rend, const WordIndex new_word, State &out_state, const cloudlm::WordIndexToString words) const;

    // Score bigrams and above.  Do not include backoff.   
    void ResumeScore(const WordIndex *context_rbegin, const WordIndex *const context_rend, unsigned char starting_order_minus_2, typename Search::Node &node, float *backoff_out, unsigned char &next_use, FullScoreReturn &ret, const cloudlm::WordIndexToString words) const;

    // Appears after Size in the cc file.
    //void SetupMemory(void *start, const std::vector<uint64_t> &counts, const Config &config);

    //void InitializeFromARPA(int fd, const char *file, const Config &config);

    float InternalUnRest(const uint64_t *pointers_begin, const uint64_t *pointers_end, unsigned char first_length, const cloudlm::WordIndexToString words) const;
    
    VocabularyT vocab_;

    Search search_;
};

} // namespace detail

// Instead of typedef, inherit.  This allows the Model etc to be forward declared.  
// Oh the joys of C and C++.
#ifndef LM_COMMA
#define LM_COMMA() ,
#endif
#ifndef LM_NAME_MODEL
#define LM_NAME_MODEL(name, from)\
class name : public from {\
  public:\
    name(WordIndex begin, WordIndex end, WordIndex unk) : from(begin, end, unk) {}\
};
#endif

/*LM_NAME_MODEL(ProbingModel, detail::GenericModel<detail::HashedSearch<BackoffValue> LM_COMMA() ProbingVocabulary>);
LM_NAME_MODEL(RestProbingModel, detail::GenericModel<detail::HashedSearch<RestValue> LM_COMMA() ProbingVocabulary>);
LM_NAME_MODEL(TrieModel, detail::GenericModel<trie::TrieSearch<DontQuantize LM_COMMA() trie::DontBhiksha> LM_COMMA() SortedVocabulary>);
LM_NAME_MODEL(ArrayTrieModel, detail::GenericModel<trie::TrieSearch<DontQuantize LM_COMMA() trie::ArrayBhiksha> LM_COMMA() SortedVocabulary>);
LM_NAME_MODEL(QuantTrieModel, detail::GenericModel<trie::TrieSearch<SeparatelyQuantize LM_COMMA() trie::DontBhiksha> LM_COMMA() SortedVocabulary>);
LM_NAME_MODEL(QuantArrayTrieModel, detail::GenericModel<trie::TrieSearch<SeparatelyQuantize LM_COMMA() trie::ArrayBhiksha> LM_COMMA() SortedVocabulary>);*/
LM_NAME_MODEL(CloudModel, detail::GenericModel<detail::CloudSearch<BackoffValue> LM_COMMA() CloudVocabulary>);

// Default implementation.  No real reason for it to be the default.  
//typedef ::cloudlm::ngram::ProbingVocabulary Vocabulary;
//typedef ProbingModel Model;

} // namespace ngram
} // namespace cloudlm

#endif // CLOUDLM_MODEL_H
