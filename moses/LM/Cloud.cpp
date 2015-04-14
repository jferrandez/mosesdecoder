// $Id$

/***********************************************************************
Moses - factored phrase-based language decoder
Copyright (C) 2006 University of Edinburgh

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
***********************************************************************/

#include <cstring>
#include <iostream>
#include <memory>
#include <stdlib.h>
#include <boost/shared_ptr.hpp>
#include <boost/algorithm/string.hpp>

#include "cloud-lm/left.hh"
#include "cloud-lm/model.hh"
#include "cloud-lm/state.hh"
#include "cloud-lm/config.hh"

#include "util/exception.hh"

#include "Cloud.h"
#include "Base.h"
#include "moses/FF/FFState.h"
#include "moses/TypeDef.h"
#include "moses/Util.h"
#include "moses/FactorCollection.h"
#include "moses/Phrase.h"
#include "moses/InputFileStream.h"
#include "moses/StaticData.h"
#include "moses/ChartHypothesis.h"
#include "moses/Incremental.h"
#include "moses/Syntax/SVertex.h"

using namespace std;

namespace Moses
{
namespace
{

struct CloudLMState : public FFState {
  cloudlm::ngram::State state;
  int Compare(const FFState &o) const {
    const CloudLMState &other = static_cast<const CloudLMState &>(o);
    if (state.length < other.state.length) return -1;
    if (state.length > other.state.length) return 1;
    return std::memcmp(state.words, other.state.words, sizeof(cloudlm::WordIndex) * state.length);
  }
};

} // namespace

template <class Model> LanguageModelCloud<Model>::LanguageModelCloud(const std::string &line, const std::string &url, size_t cache, FactorType factorType, size_t order)
  :LanguageModel(line)
  ,m_factorType(factorType)
{
  cloudlm::ngram::Config *config = cloudlm::ngram::Config::Instance();
  IFVERBOSE(1) {
    config->messages = &std::cerr;
  }
  else {
    config->messages = NULL;
  }

  vector<string> strs;
  boost::split(strs,url,boost::is_any_of(":"));
  if (strs.size() == 2) {
	config->request.base_url = strs[0];
	config->request.port = strs[1];
  }

  // 0 -> no cache; 1 -> individual requests to cache; 2 -> multiple query in request;
  if (cache < 0 || cache > 2) {
	  cache = 0;
  }

  VERBOSE(2, "CLOUDLM INFO The level of the cache is: " << cache);

  config->cache = cache;
  config->max_order = order;
  config->request.url = url;
  config->verbose_level = StaticData::Instance().GetVerboseLevel();

  FactorCollection &collection = FactorCollection::Instance();
  m_beginSentenceFactor = collection.AddFactor(BOS_);
  const Factor *endFactor = collection.AddFactor(EOS_);
  const Factor *unkFactor = collection.AddFactor("<unk>");
  m_ngram.reset(new Model(m_beginSentenceFactor->GetId(), endFactor->GetId(), unkFactor->GetId()));
}

template <class Model> LanguageModelCloud<Model>::LanguageModelCloud(const LanguageModelCloud<Model> &copy_from)
  :LanguageModel(copy_from.GetArgLine()),
   m_ngram(copy_from.m_ngram),
   m_factorType(copy_from.m_factorType),
   m_beginSentenceFactor(copy_from.m_beginSentenceFactor)
{
}

template <class Model> const FFState * LanguageModelCloud<Model>::EmptyHypothesisState(const InputType &/*input*/) const
{
  CloudLMState *ret = new CloudLMState();
  ret->state = m_ngram->BeginSentenceState();
  return ret;
}

template <class Model> void LanguageModelCloud<Model>::CalcScore(const Phrase &phrase, float &fullScore, float &ngramScore, size_t &oovCount) const
{
  fullScore = 0;
  ngramScore = 0;
  oovCount = 0;

  if (!phrase.GetSize()) return;

  cloudlm::ngram::ChartState discarded_sadly;
  cloudlm::ngram::RuleScore<Model> scorer(*m_ngram, discarded_sadly);

  size_t position;
  if (m_beginSentenceFactor == phrase.GetWord(0)[m_factorType]) {
    scorer.BeginSentence();
    position = 1;
  } else {
    position = 0;
  }

  size_t ngramBoundary = m_ngram->Order() - 1;

  size_t end_loop = std::min(ngramBoundary, phrase.GetSize());

  //std::cout << "Phrase: " << phrase.ToString() << std::endl;
  cloudlm::ngram::Config *config = cloudlm::ngram::Config::Instance();
  cloudlm::WordIndexToString words;
  if (config->cache == 2) {
	  std::vector<std::string> words_cache;
	  for (size_t i = 0; i < phrase.GetSize(); ++i) {
		const Word &word = phrase.GetWord(i);
		std::string w = word[m_factorType]->GetString().as_string();
		words[word[m_factorType]->GetId()] = w;
		words_cache.push_back(w);
	  }
	  // CALL THE CACHE IN ONE REQUEST
	  cloudlm::ngram::SendRequest(words_cache);
  }
  else {
	  for (size_t i = 0; i < phrase.GetSize(); ++i) {
		const Word &word = phrase.GetWord(i);
		words[word[m_factorType]->GetId()] = word[m_factorType]->GetString().as_string();
	  }
  }

  for (; position < end_loop; ++position) {
    const Word &word = phrase.GetWord(position);
    if (word.IsNonTerminal()) {
      fullScore += scorer.Finish();
      scorer.Reset();
    } else {
      cloudlm::WordIndex index = word[m_factorType]->GetId();
      scorer.Terminal(index, words);
      if (!index) ++oovCount;
    }
  }
  float before_boundary = fullScore + scorer.Finish();
  for (; position < phrase.GetSize(); ++position) {
    const Word &word = phrase.GetWord(position);
    if (word.IsNonTerminal()) {
      fullScore += scorer.Finish();
      scorer.Reset();
    } else {
	  cloudlm::WordIndex index = word[m_factorType]->GetId();
      scorer.Terminal(index, words);
      if (!index) ++oovCount;
    }
  }
  fullScore += scorer.Finish();

  ngramScore = TransformLMScore(fullScore - before_boundary);
  fullScore = TransformLMScore(fullScore);
}

template <class Model> FFState *LanguageModelCloud<Model>::EvaluateWhenApplied(const Hypothesis &hypo, const FFState *ps, ScoreComponentCollection *out) const
{
  const cloudlm::ngram::State &in_state = static_cast<const CloudLMState&>(*ps).state;

  std::auto_ptr<CloudLMState> ret(new CloudLMState());

  if (!hypo.GetCurrTargetLength()) {
    ret->state = in_state;
    return ret.release();
  }

  const std::size_t begin = hypo.GetCurrTargetWordsRange().GetStartPos();
  //[begin, end) in STL-like fashion.
  const std::size_t end = hypo.GetCurrTargetWordsRange().GetEndPos() + 1;
  const std::size_t adjust_end = std::min(end, begin + m_ngram->Order() - 1);

  std::size_t position = begin;
  typename Model::State aux_state;
  typename Model::State *state0 = &ret->state, *state1 = &aux_state;

  //std::cout << "Hypo: " << hypo.ToString() << std::endl;
  cloudlm::ngram::Config *config = cloudlm::ngram::Config::Instance();
  cloudlm::WordIndexToString words;
  if (config->cache == 2) {
	  std::vector<std::string> words_cache;
	  for (size_t i = 0; i < end; ++i) {
		const Word &word = hypo.GetWord(i);
		std::string w = word[m_factorType]->GetString().as_string();
		words[word[m_factorType]->GetId()] = w;
		words_cache.push_back(w);
	  }
	  // CALL THE CACHE IN ONE REQUEST
	  cloudlm::ngram::SendRequest(words_cache);
  }
  else {
	  for (size_t i = 0; i < end; ++i) {
		const Word &word = hypo.GetWord(i);
		words[word[m_factorType]->GetId()] = word[m_factorType]->GetString().as_string();
	  }
  }

  float score = m_ngram->Score(in_state, hypo.GetWord(position)[m_factorType]->GetId(), *state0, words);
  ++position;
  for (; position < adjust_end; ++position) {
    score += m_ngram->Score(*state0, hypo.GetWord(position)[m_factorType]->GetId(), *state1, words);
    std::swap(state0, state1);
  }

  if (hypo.IsSourceCompleted()) {
    // Score end of sentence.
    std::vector<cloudlm::WordIndex> indices(m_ngram->Order() - 1);
    const cloudlm::WordIndex *last = LastIDs(hypo, &indices.front());
    score += m_ngram->FullScoreForgotState(&indices.front(), last, m_ngram->GetVocabulary().EndSentence(), ret->state, words).prob;
  } else if (adjust_end < end) {
    // Get state after adding a long phrase.
    std::vector<cloudlm::WordIndex> indices(m_ngram->Order() - 1);
    const cloudlm::WordIndex *last = LastIDs(hypo, &indices.front());
    m_ngram->GetState(&indices.front(), last, ret->state, words);
  } else if (state0 != &ret->state) {
    // Short enough phrase that we can just reuse the state.
    ret->state = *state0;
  }

  score = TransformLMScore(score);

  if (OOVFeatureEnabled()) {
    std::vector<float> scores(2);
    scores[0] = score;
    scores[1] = 0.0;
    out->PlusEquals(this, scores);
  } else {
    out->PlusEquals(this, score);
  }

  return ret.release();
}

class LanguageModelChartStateCloudLM : public FFState
{
public:
  LanguageModelChartStateCloudLM() {}

  const cloudlm::ngram::ChartState &GetChartState() const {
    return m_state;
  }
  cloudlm::ngram::ChartState &GetChartState() {
    return m_state;
  }

  int Compare(const FFState& o) const {
    const LanguageModelChartStateCloudLM &other = static_cast<const LanguageModelChartStateCloudLM&>(o);
    int ret = m_state.Compare(other.m_state);
    return ret;
  }

private:
  cloudlm::ngram::ChartState m_state;
};

template <class Model> FFState *LanguageModelCloud<Model>::EvaluateWhenApplied(const ChartHypothesis& hypo, int featureID, ScoreComponentCollection *accumulator) const
{
  LanguageModelChartStateCloudLM *newState = new LanguageModelChartStateCloudLM();
  cloudlm::ngram::RuleScore<Model> ruleScore(*m_ngram, newState->GetChartState());
  const TargetPhrase &target = hypo.GetCurrTargetPhrase();
  const AlignmentInfo::NonTermIndexMap &nonTermIndexMap =
    target.GetAlignNonTerm().GetNonTermIndexMap();

  const size_t size = hypo.GetCurrTargetPhrase().GetSize();
  size_t phrasePos = 0;
  // Special cases for first word.
  if (size) {
    const Word &word = hypo.GetCurrTargetPhrase().GetWord(0);
    if (word[m_factorType] == m_beginSentenceFactor) {
      // Begin of sentence
      ruleScore.BeginSentence();
      phrasePos++;
    } else if (word.IsNonTerminal()) {
      // Non-terminal is first so we can copy instead of rescoring.
      const ChartHypothesis *prevHypo = hypo.GetPrevHypo(nonTermIndexMap[phrasePos]);
      const cloudlm::ngram::ChartState &prevState = static_cast<const LanguageModelChartStateCloudLM*>(prevHypo->GetFFState(featureID))->GetChartState();
      float prob = UntransformLMScore(prevHypo->GetScoreBreakdown().GetScoresForProducer(this)[0]);
      ruleScore.BeginNonTerminal(prevState, prob);
      phrasePos++;
    }
  }

  cloudlm::ngram::Config *config = cloudlm::ngram::Config::Instance();
  cloudlm::WordIndexToString words;
  if (config->cache == 2) {
	  std::vector<std::string> words_cache;
	  for (size_t i = 0; i < size; ++i) {
		const Word &word = hypo.GetCurrTargetPhrase().GetWord(i);
		std::string w = word[m_factorType]->GetString().as_string();
		words[word[m_factorType]->GetId()] = w;
		words_cache.push_back(w);
	  }
	  // CALL THE CACHE IN ONE REQUEST
	  cloudlm::ngram::SendRequest(words_cache);
  }
  else {
	for (size_t i = 0; i < size; ++i) {
		const Word &word = hypo.GetCurrTargetPhrase().GetWord(i);
		words[word[m_factorType]->GetId()] = word[m_factorType]->GetString().as_string();
	}
  }
  /*cloudlm::WordIndexToString words;
  for (size_t i = 0; i < size; ++i) {
	const Word &word = hypo.GetCurrTargetPhrase().GetWord(i);
	words[word[m_factorType]->GetId()] = word[m_factorType]->GetString().as_string();
  }*/

  for (; phrasePos < size; phrasePos++) {
    const Word &word = hypo.GetCurrTargetPhrase().GetWord(phrasePos);
    if (word.IsNonTerminal()) {
      const ChartHypothesis *prevHypo = hypo.GetPrevHypo(nonTermIndexMap[phrasePos]);
      const cloudlm::ngram::ChartState &prevState = static_cast<const LanguageModelChartStateCloudLM*>(prevHypo->GetFFState(featureID))->GetChartState();
      float prob = UntransformLMScore(prevHypo->GetScoreBreakdown().GetScoresForProducer(this)[0]);
      ruleScore.NonTerminal(prevState, words, prob);
    } else {
      ruleScore.Terminal(word[m_factorType]->GetId(), words);
    }
  }

  float score = ruleScore.Finish();
  score = TransformLMScore(score);
  if (OOVFeatureEnabled()) {
    std::vector<float> scores(2);
    scores[0] = score;
    scores[1] = 0.0;
    accumulator->Assign(this, scores);
  }
  else {
    accumulator->Assign(this, score);
  }
  return newState;
}

template <class Model> FFState *LanguageModelCloud<Model>::EvaluateWhenApplied(const Syntax::SHyperedge& hyperedge, int featureID, ScoreComponentCollection *accumulator) const
{
  LanguageModelChartStateCloudLM *newState = new LanguageModelChartStateCloudLM();
  cloudlm::ngram::RuleScore<Model> ruleScore(*m_ngram, newState->GetChartState());
  const TargetPhrase &target = *hyperedge.label.translation;
  const AlignmentInfo::NonTermIndexMap &nonTermIndexMap =
    target.GetAlignNonTerm().GetNonTermIndexMap2();

  const size_t size = target.GetSize();
  size_t phrasePos = 0;
  // Special cases for first word.
  if (size) {
    const Word &word = target.GetWord(0);
    if (word[m_factorType] == m_beginSentenceFactor) {
      // Begin of sentence
      ruleScore.BeginSentence();
      phrasePos++;
    } else if (word.IsNonTerminal()) {
      // Non-terminal is first so we can copy instead of rescoring.
      const Syntax::SVertex *pred = hyperedge.tail[nonTermIndexMap[phrasePos]];
      const cloudlm::ngram::ChartState &prevState = static_cast<const LanguageModelChartStateCloudLM*>(pred->state[featureID])->GetChartState();
      float prob = UntransformLMScore(pred->best->label.scoreBreakdown.GetScoresForProducer(this)[0]);
      ruleScore.BeginNonTerminal(prevState, prob);
      phrasePos++;
    }
  }

  cloudlm::ngram::Config *config = cloudlm::ngram::Config::Instance();
  cloudlm::WordIndexToString words;
  if (config->cache == 2) {
	  std::vector<std::string> words_cache;
	  for (size_t i = 0; i < size; ++i) {
		const Word &word = target.GetWord(i);
		std::string w = word[m_factorType]->GetString().as_string();
		words[word[m_factorType]->GetId()] = w;
		words_cache.push_back(w);
	  }
	  // CALL THE CACHE IN ONE REQUEST
	  cloudlm::ngram::SendRequest(words_cache);
  }
  else {
	for (size_t i = 0; i < size; ++i) {
		const Word &word = target.GetWord(i);
		words[word[m_factorType]->GetId()] = word[m_factorType]->GetString().as_string();
	}
  }
  /*cloudlm::WordIndexToString words;
  for (size_t i = 0; i < size; ++i) {
	const Word &word = target.GetWord(i);
	words[word[m_factorType]->GetId()] = word[m_factorType]->GetString().as_string();
  }*/

  for (; phrasePos < size; phrasePos++) {
    const Word &word = target.GetWord(phrasePos);
    if (word.IsNonTerminal()) {
      const Syntax::SVertex *pred = hyperedge.tail[nonTermIndexMap[phrasePos]];
      const cloudlm::ngram::ChartState &prevState = static_cast<const LanguageModelChartStateCloudLM*>(pred->state[featureID])->GetChartState();
      float prob = UntransformLMScore(pred->best->label.scoreBreakdown.GetScoresForProducer(this)[0]);
      ruleScore.NonTerminal(prevState, words, prob);
    } else {
      ruleScore.Terminal(word[m_factorType]->GetId(), words);
    }
  }

  float score = ruleScore.Finish();
  score = TransformLMScore(score);
  accumulator->Assign(this, score);
  return newState;
}

template <class Model> void LanguageModelCloud<Model>::IncrementalCallback(Incremental::Manager &manager) const
{
	// TODO : it doesn't work. To fix it.
	//manager.LMCallback(*m_ngram, m_lmIdLookup);
}

template <class Model> void LanguageModelCloud<Model>::ReportHistoryOrder(std::ostream &out, const Phrase &phrase) const
{
  out << "|lm=(";
  if (!phrase.GetSize()) return;

  typename Model::State aux_state;
  typename Model::State start_of_sentence_state = m_ngram->BeginSentenceState();
  typename Model::State *state0 = &start_of_sentence_state;
  typename Model::State *state1 = &aux_state;

  cloudlm::WordIndexToString words;
  for (size_t i = 0; i < phrase.GetSize(); ++i) {
	const Word &word = phrase.GetWord(i);
	words[word[m_factorType]->GetId()] = word[m_factorType]->GetString().as_string();
  }

  for (std::size_t position=0; position<phrase.GetSize(); position++) {
    const cloudlm::WordIndex idx = phrase.GetWord(position)[m_factorType]->GetId();
    cloudlm::FullScoreReturn ret(m_ngram->FullScore(*state0, idx, *state1, words));
    if (position) out << ",";
    out << (int) ret.ngram_length << ":" << TransformLMScore(ret.prob);
    if (idx == 0) out << ":unk";
    std::swap(state0, state1);
  }
  out << ")| ";
}

template <class Model>
bool LanguageModelCloud<Model>::IsUseable(const FactorMask &mask) const
{
  bool ret = mask[m_factorType];
  return ret;
}


LanguageModel *ConstructCloudLM(const std::string &line)
{
  FactorType factorType = 0;
  string url;
  size_t nGramOrder = 0;
  size_t cache = 0;

  vector<string> toks = Tokenize(line);
  for (size_t i = 1; i < toks.size(); ++i) {
    vector<string> args = Tokenize(toks[i], "=");
    UTIL_THROW_IF2(args.size() != 2,
    		"Incorrect format of CloudLM property: " << toks[i]);

    if (args[0] == "factor") {
      factorType = Scan<FactorType>(args[1]);
    } else if (args[0] == "order") {
      nGramOrder = Scan<size_t>(args[1]);
    } else if (args[0] == "url") {
      url = args[1];
    } else if (args[0] == "cache") {
      cache = Scan<size_t>(args[1]);
    } /*else if (args[0] == "name") {
      // that's ok. do nothing, passes onto LM constructor
    }*/
  }

  return new LanguageModelCloud<cloudlm::ngram::CloudModel>(line, url, cache, factorType, nGramOrder);
}

}

