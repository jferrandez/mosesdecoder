#ifndef CLOUDLM_VOCAB_H
#define CLOUDLM_VOCAB_H

#include "cloud-lm/enumerate_vocab.hh"
#include "cloud-lm/lm_exception.hh"
#include "cloud-lm/virtual_interface.hh"
#include "util/fake_ofstream.hh"
#include "util/murmur_hash.hh"
#include "util/pool.hh"
#include "util/probing_hash_table.hh"
#include "util/sorted_uniform.hh"
#include "util/string_piece.hh"

#include <limits>
#include <string>
#include <vector>
#include <boost/bimap.hpp>

namespace cloudlm {
struct ProbBackoff;
class EnumerateVocab;

namespace ngram {
struct Config;

namespace detail {

uint64_t HashForVocab(const char *str, std::size_t len);
inline uint64_t HashForVocab(const StringPiece &str) {
  return HashForVocab(str.data(), str.length());
}
struct ProbingVocabularyHeader;
} // namespace detail

class CloudVocabulary : public base::Vocabulary {
  public:
	CloudVocabulary();

    void SaveSpecialWords(WordIndex begin, WordIndex end, WordIndex unk) {
	  SetSpecial(begin, end, unk);
    }

    bool SawUnk() const { return saw_unk_; }

  private:
    bool saw_unk_;
};

void MissingUnknown(const Config &config) throw(SpecialWordMissingException);
void MissingSentenceMarker(const Config &config, const char *str) throw(SpecialWordMissingException);

template <class Vocab> void CheckSpecials(const Config &config, const Vocab &vocab) throw(SpecialWordMissingException) {
  if (!vocab.SawUnk()) MissingUnknown(config);
  if (vocab.BeginSentence() == vocab.NotFound()) MissingSentenceMarker(config, "<s>");
  if (vocab.EndSentence() == vocab.NotFound()) MissingSentenceMarker(config, "</s>");
}

} // namespace ngram
} // namespace cloudlm

#endif // CLOUDLM_VOCAB_H
